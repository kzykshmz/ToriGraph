"""Pure-Python 3.8+ JSON-RPC 2.0 client SDK for ToriGraph."""

from __future__ import annotations

import ctypes
import json
import math
import os
import queue
import shutil
import socket
import subprocess
import tempfile
import threading
import time
import weakref
from pathlib import Path
from typing import Any, Callable, Dict, Iterable, List, Mapping, Optional, Sequence, Tuple, Union


class ToriGraphError(RuntimeError):
    """Base exception for ToriGraph SDK errors."""


class TransportError(ToriGraphError):
    """Raised when the underlying TCP or Named Pipe connection fails."""


class RpcError(ToriGraphError):
    """Raised when ToriGraph returns a JSON-RPC 2.0 error object."""

    def __init__(self, code: int, message: str, data: Any = None) -> None:
        super().__init__(f"[RPC {code}] {message}")
        self.code = int(code)
        self.message = str(message)
        self.data = data
        self.available: Optional[List[str]] = (
            [str(x) for x in data["available"]]
            if isinstance(data, Mapping) and isinstance(data.get("available"), (list, tuple))
            else None
        )


ToriGraphRpcError = RpcError


def _cleanup_spawned_session(
    proc: Optional[subprocess.Popen[bytes]],
    transport: Any,
    cleanup_files: Sequence[str],
) -> None:
    """GC / explicit cleanup helper for spawned ToriGraph child processes and temp files."""
    if transport is not None:
        try:
            transport.close()
        except Exception:
            pass
    if proc is not None:
        try:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=5.0)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait(timeout=2.0)
        except Exception:
            pass
    for fpath in cleanup_files:
        try:
            if fpath and os.path.exists(fpath):
                os.remove(fpath)
        except OSError:
            pass


def _sanitize_for_json(value: Any) -> Any:
    """Recursively convert NaN/Inf -> None, Path -> str, proxies -> name, and duck-typed arrays -> list."""
    if value is None or isinstance(value, (bool, int, str)):
        return value
    if isinstance(value, float):
        return value if math.isfinite(value) else None
    if isinstance(value, os.PathLike):
        return os.fspath(value)
    if isinstance(value, (TableProxy, MatrixProxy, GraphProxy)):
        return value.name
    if isinstance(value, Mapping):
        return {str(k): _sanitize_for_json(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [_sanitize_for_json(v) for v in value]
    if hasattr(value, "tolist") and callable(value.tolist):
        return _sanitize_for_json(value.tolist())
    if isinstance(value, Iterable) and not isinstance(value, (bytes, bytearray)):
        return [_sanitize_for_json(v) for v in value]
    return value


def _clean_kwargs(kwargs: Dict[str, Any]) -> Dict[str, Any]:
    """Drop None-valued optional arguments and sanitize remaining values for JSON."""
    return {k: _sanitize_for_json(v) for k, v in kwargs.items() if v is not None}


def _name_of(obj: Union[str, "TableProxy", "MatrixProxy", "GraphProxy", None]) -> Optional[str]:
    if obj is None:
        return None
    if isinstance(obj, str):
        return obj
    if hasattr(obj, "name"):
        return str(obj.name)
    return str(obj)


class _TcpTransport:
    """Newline-delimited UTF-8 JSON-RPC 2.0 transport over loopback TCP."""

    def __init__(self, host: str, port: int, timeout: float = 30.0) -> None:
        self.host = host
        self.port = int(port)
        self.timeout = float(timeout)
        try:
            self._sock = socket.create_connection((self.host, self.port), timeout=self.timeout)
            self._sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self._sock.settimeout(self.timeout)
            self._reader = self._sock.makefile("rb")
        except OSError as exc:
            raise TransportError(f"Failed to connect to tcp://{self.host}:{self.port}: {exc}") from exc

    @property
    def endpoint(self) -> str:
        return f"tcp://{self.host}:{self.port}"

    def request_line(self, payload: bytes) -> bytes:
        try:
            self._sock.sendall(payload + b"\n")
            while True:
                line = self._reader.readline()
                if not line:
                    raise TransportError(f"Connection closed by ToriGraph ({self.endpoint}).")
                stripped = line.strip()
                if stripped:
                    return stripped
        except OSError as exc:
            raise TransportError(f"Socket I/O error on {self.endpoint}: {exc}") from exc

    def close(self) -> None:
        try:
            self._reader.close()
        except Exception:
            pass
        try:
            self._sock.close()
        except Exception:
            pass


class _LocalPipeTransport:
    """Newline-delimited UTF-8 JSON-RPC 2.0 transport over Windows Named Pipe / Unix socket."""

    def __init__(self, pipe_name: str, timeout: float = 30.0) -> None:
        self.pipe_name = pipe_name
        self.timeout = max(0.05, float(timeout))
        self._buf = bytearray()
        self._win_handle: Any = None
        self._win_osfhandle: Optional[int] = None
        self._unix_sock: Optional[socket.socket] = None
        self._endpoint = f"pipe://{pipe_name}"

        if os.name == "nt":
            import msvcrt

            full_path = (
                pipe_name
                if pipe_name.startswith("\\\\.\\pipe\\")
                else f"\\\\.\\pipe\\{pipe_name}"
            )
            deadline = time.monotonic() + self.timeout
            last_err: Optional[Exception] = None
            kernel32 = getattr(ctypes, "windll", None)
            wait_named_pipe = (
                getattr(kernel32.kernel32, "WaitNamedPipeW", None)
                if kernel32 is not None and hasattr(kernel32, "kernel32")
                else None
            )
            while True:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    break
                if wait_named_pipe is not None:
                    wait_ms = max(1, min(100, int(remaining * 1000.0)))
                    wait_named_pipe(ctypes.c_wchar_p(full_path), ctypes.c_uint32(wait_ms))
                rem_open = deadline - time.monotonic()
                if rem_open <= 0:
                    break
                try:
                    self._win_handle = self._run_win_op(
                        lambda: open(full_path, "r+b", buffering=0),
                        "connect",
                        rem_open,
                    )
                    self._win_osfhandle = int(msvcrt.get_osfhandle(self._win_handle.fileno()))
                    break
                except TransportError as exc:
                    last_err = exc
                    break
                except OSError as exc:
                    last_err = exc
                    sleep_s = min(0.02, max(0.0, deadline - time.monotonic()))
                    if sleep_s <= 0:
                        break
                    time.sleep(sleep_s)
            if self._win_handle is None:
                raise TransportError(
                    f"Failed to open named pipe '{full_path}' within {self.timeout:.2f}s: {last_err}"
                )
        else:
            path = pipe_name if "/" in pipe_name else f"/tmp/{pipe_name}"
            try:
                sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                sock.settimeout(self.timeout)
                sock.connect(path)
                self._unix_sock = sock
            except OSError as exc:
                raise TransportError(f"Failed to connect Unix domain socket '{path}': {exc}") from exc

    @property
    def endpoint(self) -> str:
        return self._endpoint

    def _abort_win_handle_nonblocking(self) -> None:
        fh = self._win_handle
        osfh = self._win_osfhandle
        self._win_handle = None
        self._win_osfhandle = None
        if osfh is not None and os.name == "nt":
            try:
                ctypes.windll.kernel32.CancelIoEx(ctypes.c_void_p(osfh), None)
            except Exception:
                pass
        if fh is not None:
            threading.Thread(
                target=lambda: self._quiet_close(fh),
                name="torigraph-pipe-close",
                daemon=True,
            ).start()

    @staticmethod
    def _quiet_close(fh: Any) -> None:
        try:
            fh.close()
        except Exception:
            pass

    def _run_win_op(
        self, fn: Callable[[], Any], op_name: str, timeout_sec: float
    ) -> Any:
        result_q: "queue.Queue[Tuple[bool, Any]]" = queue.Queue(maxsize=1)

        def _worker() -> None:
            try:
                result_q.put((True, fn()))
            except BaseException as exc:  # noqa: BLE001
                result_q.put((False, exc))

        th = threading.Thread(target=_worker, name="torigraph-pipe-io", daemon=True)
        th.start()
        try:
            ok, payload = result_q.get(timeout=max(0.01, timeout_sec))
        except queue.Empty as exc:
            self._abort_win_handle_nonblocking()
            raise TransportError(
                f"Named pipe {op_name} timed out after {self.timeout:.2f}s ({self._endpoint})."
            ) from exc
        if ok:
            return payload
        raise payload

    def request_line(self, payload: bytes) -> bytes:
        msg = payload + b"\n"
        deadline = time.monotonic() + self.timeout
        try:
            if self._win_handle is not None:
                fh = self._win_handle
                rem = deadline - time.monotonic()
                if rem <= 0:
                    self._abort_win_handle_nonblocking()
                    raise TransportError(
                        f"Named pipe write timed out after {self.timeout:.2f}s ({self._endpoint})."
                    )
                self._run_win_op(
                    lambda: (fh.write(msg), fh.flush()),
                    "write",
                    rem,
                )
                peek_fn = ctypes.windll.kernel32.PeekNamedPipe
                avail = ctypes.c_uint32(0)
                sleep_step = 0.001
                while True:
                    nl = self._buf.find(b"\n")
                    if nl >= 0:
                        raw = bytes(self._buf[:nl]).strip()
                        del self._buf[: nl + 1]
                        if raw:
                            return raw
                        continue
                    rem = deadline - time.monotonic()
                    if rem <= 0:
                        self._abort_win_handle_nonblocking()
                        raise TransportError(
                            f"Named pipe read timed out after {self.timeout:.2f}s ({self._endpoint})."
                        )
                    fh_cur = self._win_handle
                    osfh = self._win_osfhandle
                    if fh_cur is None or osfh is None:
                        raise TransportError(f"Named pipe closed ({self._endpoint}).")
                    avail.value = 0
                    ok_peek = peek_fn(
                        ctypes.c_void_p(osfh),
                        None,
                        0,
                        None,
                        ctypes.byref(avail),
                        None,
                    )
                    if not ok_peek:
                        self.close()
                        raise TransportError(f"Named pipe closed ({self._endpoint}).")
                    if avail.value == 0:
                        time.sleep(min(sleep_step, max(0.0, deadline - time.monotonic())))
                        sleep_step = min(0.01, sleep_step * 1.5)
                        continue
                    sleep_step = 0.001
                    to_read = min(65536, int(avail.value))
                    chunk = self._run_win_op(lambda: fh_cur.read(to_read), "read", rem)
                    if not chunk:
                        raise TransportError(f"Named pipe closed ({self._endpoint}).")
                    self._buf.extend(chunk)
            elif self._unix_sock is not None:
                self._unix_sock.settimeout(self.timeout)
                self._unix_sock.sendall(msg)
                while True:
                    nl = self._buf.find(b"\n")
                    if nl >= 0:
                        raw = bytes(self._buf[:nl]).strip()
                        del self._buf[: nl + 1]
                        if raw:
                            return raw
                        continue
                    rem = deadline - time.monotonic()
                    if rem <= 0:
                        self.close()
                        raise TransportError(
                            f"Unix socket read timed out after {self.timeout:.2f}s ({self._endpoint})."
                        )
                    self._unix_sock.settimeout(max(0.01, rem))
                    chunk = self._unix_sock.recv(65536)
                    if not chunk:
                        raise TransportError(f"Unix socket closed ({self._endpoint}).")
                    self._buf.extend(chunk)
            raise TransportError("Transport is closed.")
        except TransportError:
            raise
        except OSError as exc:
            raise TransportError(f"Pipe I/O error on {self._endpoint}: {exc}") from exc

    def close(self) -> None:
        self._win_osfhandle = None
        if self._win_handle is not None:
            try:
                self._win_handle.close()
            except Exception:
                pass
            self._win_handle = None
        if self._unix_sock is not None:
            try:
                self._unix_sock.close()
            except Exception:
                pass
            self._unix_sock = None


def _parse_and_connect_transport(
    endpoint: str, timeout: float = 30.0
) -> Union[_TcpTransport, _LocalPipeTransport]:
    ep = str(endpoint).strip()
    if not ep:
        raise TransportError("Empty ToriGraph RPC endpoint.")
    if ep.startswith("tcp://"):
        host_port = ep[len("tcp://") :]
        host, sep, port_s = host_port.rpartition(":")
        if not sep or not port_s.isdigit():
            raise TransportError(f"Invalid TCP endpoint '{ep}'.")
        return _TcpTransport(host or "127.0.0.1", int(port_s), timeout=timeout)
    if ep.startswith("pipe://"):
        return _LocalPipeTransport(ep[len("pipe://") :], timeout=timeout)
    if ep.startswith("\\\\.\\pipe\\"):
        return _LocalPipeTransport(ep, timeout=timeout)
    if ep.isdigit():
        return _TcpTransport("127.0.0.1", int(ep), timeout=timeout)
    if ":" in ep:
        host, _, port_s = ep.rpartition(":")
        if port_s.isdigit():
            return _TcpTransport(host or "127.0.0.1", int(port_s), timeout=timeout)
    return _LocalPipeTransport(ep, timeout=timeout)


class _NamespaceProxy:
    """Dynamic dotted namespace forwarding e.g. `app.analysis.fit_nonlinear(...)`."""

    def __init__(self, session: "Session", prefix: str) -> None:
        self._session = session
        self._prefix = prefix

    def __getattr__(self, method_suffix: str) -> Any:
        rpc_method = f"{self._prefix}.{method_suffix}"

        def _invoke(
            _params: Optional[Dict[str, Any]] = None, /, **kwargs: Any
        ) -> Dict[str, Any]:
            return self._session.call(rpc_method, _params, **kwargs)

        return _invoke


class _TableDomain(_NamespaceProxy):
    """Callable `app.table(name) -> TableProxy` AND dotted `app.table.create(...)`."""

    def __init__(self, session: "Session") -> None:
        super().__init__(session, "table")

    def __call__(self, name: str) -> "TableProxy":
        return TableProxy(self._session, name)


class _MatrixDomain(_NamespaceProxy):
    """Callable `app.matrix(name) -> MatrixProxy` AND dotted `app.matrix.create(...)`."""

    def __init__(self, session: "Session") -> None:
        super().__init__(session, "matrix")

    def __call__(self, name: str) -> "MatrixProxy":
        return MatrixProxy(self._session, name)


class _GraphDomain(_NamespaceProxy):
    """Callable `app.graph(name) -> GraphProxy` AND dotted `app.graph.export(...)`."""

    def __init__(self, session: "Session") -> None:
        super().__init__(session, "graph")

    def __call__(self, name: str) -> "GraphProxy":
        return GraphProxy(self._session, name)


class TableProxy:
    """High-level proxy for a ToriGraph Worksheet (`Table` window)."""

    def __init__(self, session: "Session", name: str) -> None:
        self.session = session
        self.name = str(name)

    def __repr__(self) -> str:
        return f"TableProxy(name={self.name!r})"

    def info(self) -> Dict[str, Any]:
        return self.session.call("table.get_info", table=self.name)

    get_info = info

    def resize(self, rows: Optional[int] = None, cols: Optional[int] = None) -> Dict[str, Any]:
        return self.session.call("table.resize", table=self.name, rows=rows, cols=cols)

    def add_column(
        self,
        name: Optional[str] = None,
        plot_designation: str = "Y",
        mode: Optional[str] = None,
        data: Optional[Iterable[Any]] = None,
        **meta_kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "table.add_column",
            table=self.name,
            name=name,
            plot_designation=plot_designation,
            mode=mode,
            data=(list(data) if data is not None else None),
            **meta_kwargs,
        )

    def remove_column(self, col: Union[int, str]) -> Dict[str, Any]:
        return self.session.call("table.remove_column", table=self.name, col=col)

    def set_column(
        self,
        col: Union[int, str],
        values: Optional[Iterable[Any]] = None,
        mode: Optional[str] = None,
        *,
        data: Optional[Iterable[Any]] = None,
        **meta_kwargs: Any,
    ) -> Dict[str, Any]:
        seq = data if data is not None else values
        if seq is None:
            raise ValueError("Provide column 'values' (or 'data') iterable.")
        res = self.session.call(
            "table.set_column_data",
            table=self.name,
            col=col,
            data=list(seq),
            mode=mode,
            **meta_kwargs,
        )
        return res

    set_column_data = set_column
    set_col = set_column

    def get_column(self, col: Union[int, str]) -> List[Any]:
        res = self.session.call("table.get_column_data", table=self.name, col=col)
        return list(res.get("data", res.get("values", [])))

    get_column_data = get_column
    get_col = get_column

    def set_data_dict(
        self,
        data: Mapping[str, Iterable[Any]],
        meta: Optional[Mapping[str, Mapping[str, Any]]] = None,
        units: Optional[Mapping[str, str]] = None,
        long_names: Optional[Mapping[str, str]] = None,
        comments: Optional[Mapping[str, str]] = None,
        designations: Optional[Mapping[str, str]] = None,
        column_order: Optional[Sequence[str]] = None,
    ) -> Dict[str, Any]:
        payload_data = {str(k): list(v) for k, v in data.items()}
        ordered = (
            [str(k) for k in column_order]
            if column_order is not None
            else list(payload_data.keys())
        )
        return self.session.call(
            "table.set_data_dict",
            table=self.name,
            data=payload_data,
            column_order=ordered,
            meta=meta,
            units=units,
            long_names=long_names,
            comments=comments,
            designations=designations,
        )

    def get_data_dict(
        self, columns: Optional[Sequence[Union[int, str]]] = None
    ) -> Dict[str, List[Any]]:
        """Return `{column_name: [values, ...]}` preserving column order (symmetric with `set_data_dict`)."""
        cols_info = self.info().get("columns", [])
        if columns is None:
            return {
                str(c["name"]): self.get_column(int(c["index"]))
                for c in cols_info
                if "name" in c and "index" in c
            }
        by_idx = {int(c["index"]): str(c["name"]) for c in cols_info if "index" in c and "name" in c}
        out: Dict[str, List[Any]] = {}
        for col_spec in columns:
            if isinstance(col_spec, int) and not isinstance(col_spec, bool):
                col_name = by_idx.get(col_spec) or str(self.get_meta(col_spec).get("name", col_spec))
                out[col_name] = self.get_column(col_spec)
            else:
                col_name = str(col_spec)
                out[col_name] = self.get_column(col_name)
        return out

    to_dict = get_data_dict

    def set_cell(self, row: int, col: Union[int, str], value: Any) -> Dict[str, Any]:
        return self.session.call(
            "table.set_cell", table=self.name, row=int(row), col=col, value=value
        )

    def get_cell(self, row: int, col: Union[int, str]) -> Any:
        res = self.session.call("table.get_cell", table=self.name, row=int(row), col=col)
        return res.get("value")

    def set_meta(
        self,
        col: Union[int, str],
        *,
        name: Optional[str] = None,
        long_name: Optional[str] = None,
        units: Optional[str] = None,
        comment: Optional[str] = None,
        plot_designation: Optional[str] = None,
        numeric_format: Optional[str] = None,
        precision: Optional[int] = None,
    ) -> Dict[str, Any]:
        return self.session.call(
            "table.set_column_meta",
            table=self.name,
            col=col,
            name=name,
            long_name=long_name,
            units=units,
            comment=comment,
            plot_designation=plot_designation,
            numeric_format=numeric_format,
            precision=precision,
        )

    set_column_meta = set_meta

    def get_meta(self, col: Union[int, str]) -> Dict[str, Any]:
        return self.session.call("table.get_column_meta", table=self.name, col=col)

    get_column_meta = get_meta

    def set_formula(
        self, col: Union[int, str], formula: str, recalculate: bool = True
    ) -> Dict[str, Any]:
        return self.session.call(
            "table.set_formula",
            table=self.name,
            col=col,
            formula=formula,
            recalculate=recalculate,
        )

    def recalculate(self) -> Dict[str, Any]:
        return self.session.call("table.recalculate", table=self.name)

    def mask_rows(
        self, rows: Union[int, Sequence[int]], col: Optional[Union[int, str]] = None
    ) -> Dict[str, Any]:
        row_list = [int(rows)] if isinstance(rows, int) else [int(r) for r in rows]
        return self.session.call("table.mask_rows", table=self.name, rows=row_list, col=col)

    def unmask_rows(
        self,
        rows: Optional[Union[int, Sequence[int]]] = None,
        col: Optional[Union[int, str]] = None,
    ) -> Dict[str, Any]:
        row_list = (
            None
            if rows is None
            else ([int(rows)] if isinstance(rows, int) else [int(r) for r in rows])
        )
        return self.session.call("table.unmask_rows", table=self.name, rows=row_list, col=col)

    def clear_masks(self, col: Optional[Union[int, str]] = None) -> Dict[str, Any]:
        return self.session.call("table.clear_masks", table=self.name, col=col)

    def get_masked_rows(self, col: Optional[Union[int, str]] = None) -> List[int]:
        res = self.session.call("table.get_masked_rows", table=self.name, col=col)
        return [int(r) for r in res.get("rows", res.get("masked_rows", []))]

    def sort(
        self,
        by_col: Optional[Union[int, str]] = None,
        ascending: bool = True,
        separately: bool = False,
        *,
        by: Optional[Union[int, str]] = None,
        only_column: Optional[bool] = None,
    ) -> Dict[str, Any]:
        target_col = by if by is not None else (by_col if by_col is not None else 0)
        single_col = separately if only_column is None else only_column
        return self.session.call(
            "table.sort",
            table=self.name,
            by=target_col,
            ascending=ascending,
            only_column=single_col,
        )

    def stats(
        self, col: Optional[Union[int, str]] = None, ignore_masked: bool = True
    ) -> Dict[str, Any]:
        return self.session.call(
            "table.column_stats", table=self.name, col=col, ignore_masked=ignore_masked
        )

    column_stats = stats

    def row_stats(self, rows: Optional[Sequence[int]] = None) -> Dict[str, Any]:
        return self.session.call("table.row_stats", table=self.name, rows=rows)

    def export_csv(
        self,
        path: Union[str, os.PathLike[str]],
        separator: str = ",",
        with_labels: bool = True,
        with_comments: bool = False,
        *,
        header: Optional[bool] = None,
    ) -> Dict[str, Any]:
        use_header = with_labels if header is None else header
        return self.session.call(
            "table.export_csv",
            table=self.name,
            path=os.fspath(path),
            separator=separator,
            header=use_header,
            with_labels=use_header,
            with_comments=with_comments,
        )

    def to_matrix(self, name: Optional[str] = None) -> "MatrixProxy":
        res = self.session.call("table.to_matrix", table=self.name, name=name)
        return MatrixProxy(self.session, res["name"])

    def paste_tsv(
        self,
        text: Optional[str] = None,
        *,
        header: Optional[bool] = None,
        separator: Optional[str] = None,
        encoding: str = "auto",
        units_row: Optional[Union[int, bool, str]] = None,
        numeric: bool = True,
        start_row: int = 0,
        start_col: int = 0,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "table.paste_tsv",
            table=self.name,
            text=text,
            header=header,
            separator=separator,
            encoding=encoding,
            units_row=units_row,
            numeric=numeric,
            start_row=int(start_row),
            start_col=int(start_col),
            **kwargs,
        )

    def undo(self) -> Dict[str, Any]:
        return self.session.call("app.undo", table=self.name)

    def redo(self) -> Dict[str, Any]:
        return self.session.call("app.redo", table=self.name)


class MatrixProxy:
    """High-level proxy for a ToriGraph 2D Matrix window."""

    def __init__(self, session: "Session", name: str) -> None:
        self.session = session
        self.name = str(name)

    def __repr__(self) -> str:
        return f"MatrixProxy(name={self.name!r})"

    def info(self) -> Dict[str, Any]:
        return self.session.call("matrix.get_info", matrix=self.name)

    get_info = info

    def set_coordinates(
        self, x_min: float, x_max: float, y_min: float, y_max: float
    ) -> Dict[str, Any]:
        return self.session.call(
            "matrix.set_coordinates",
            matrix=self.name,
            x_min=x_min,
            x_max=x_max,
            y_min=y_min,
            y_max=y_max,
        )

    def set_data(self, data: Sequence[Sequence[Optional[float]]]) -> Dict[str, Any]:
        return self.session.call("matrix.set_data", matrix=self.name, data=data)

    def get_data(self) -> List[List[Optional[float]]]:
        res = self.session.call("matrix.get_data", matrix=self.name)
        return list(res.get("data", []))

    def set_formula(self, formula: str) -> Dict[str, Any]:
        return self.session.call("matrix.set_formula", matrix=self.name, formula=formula)

    def transpose(self) -> Dict[str, Any]:
        return self.session.call("matrix.transpose", matrix=self.name)

    def invert(self) -> Dict[str, Any]:
        return self.session.call("matrix.invert", matrix=self.name)

    def determinant(self) -> float:
        res = self.session.call("matrix.determinant", matrix=self.name)
        return float(res["determinant"])

    def flip(self, mode: str = "h", *, direction: Optional[str] = None) -> Dict[str, Any]:
        chosen = direction if direction is not None else mode
        return self.session.call("matrix.flip", matrix=self.name, mode=chosen)

    def fft2d(
        self,
        inverse: bool = False,
        shift: bool = True,
        *,
        shift_center: Optional[bool] = None,
    ) -> Dict[str, Any]:
        use_shift = shift if shift_center is None else shift_center
        return self.session.call(
            "matrix.fft2d",
            matrix=self.name,
            inverse=inverse,
            shift_center=use_shift,
        )

    def filter2d(
        self,
        method: str = "gaussian",
        sigma: float = 1.2,
        radius: int = 1,
        in_place: bool = True,
        plot_heatmap: bool = False,
    ) -> "MatrixProxy":
        res = self.session.call(
            "matrix.filter2d",
            matrix=self.name,
            method=method,
            sigma=sigma,
            radius=radius,
            in_place=in_place,
            plot_heatmap=plot_heatmap,
        )
        out_name = res.get("name", self.name)
        return self if out_name == self.name else MatrixProxy(self.session, out_name)

    def analyze_2d_peaks(
        self,
        prominence: Optional[float] = None,
        max_peaks: int = 10,
        graph: Optional[Union[str, "GraphProxy"]] = None,
        *,
        min_prominence: Optional[float] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        use_prom = (
            0.10
            if (prominence is None and min_prominence is None)
            else prominence
        )
        return self.session.call(
            "matrix.analyze_2d_peaks",
            matrix=self.name,
            prominence=use_prom,
            min_prominence=min_prominence,
            max_peaks=max_peaks,
            graph=_name_of(graph),
            **kwargs,
        )

    find_2d_peaks = analyze_2d_peaks

    def to_table(self, name: Optional[str] = None) -> TableProxy:
        res = self.session.call("matrix.to_table", matrix=self.name, name=name)
        return TableProxy(self.session, res["name"])

    def plot_heatmap(self, **kwargs: Any) -> "GraphProxy":
        return self.session.plot_matrix(self.name, style="colormap", **kwargs)

    def plot_contour(self, **kwargs: Any) -> "GraphProxy":
        return self.session.plot_matrix(self.name, style="contour", **kwargs)

    def plot_grayscale(self, **kwargs: Any) -> "GraphProxy":
        return self.session.plot_matrix(self.name, style="grayscale", **kwargs)

    def plot_3d(self, style: str = "surface", **kwargs: Any) -> "GraphProxy":
        return self.session.plot_3d_matrix(self.name, style=style, **kwargs)

    def undo(self) -> Dict[str, Any]:
        return self.session.call("app.undo", matrix=self.name)

    def redo(self) -> Dict[str, Any]:
        return self.session.call("app.redo", matrix=self.name)


class GraphProxy:
    """High-level proxy for a ToriGraph 2D MultiLayer or 3D Graph window."""

    def __init__(self, session: "Session", name: str) -> None:
        self.session = session
        self.name = str(name)

    def __repr__(self) -> str:
        return f"GraphProxy(name={self.name!r})"

    def info(self) -> Dict[str, Any]:
        return self.session.call("graph.get_info", graph=self.name)

    get_info = info

    def inspect(
        self,
        *,
        include_ticks: bool = True,
        diagnose: bool = True,
    ) -> Dict[str, Any]:
        """Return full semantic + pixel-geometry snapshot and deterministic layout diagnostics."""
        return self.session.call(
            "graph.inspect",
            graph=self.name,
            include_ticks=bool(include_ticks),
            diagnose=bool(diagnose),
        )

    def diagnose_layout(self) -> List[Dict[str, Any]]:
        """Return deterministic layout warning dicts from `graph.inspect`."""
        res = self.inspect(include_ticks=True, diagnose=True)
        return list(res.get("warnings", []))

    def set_title(
        self,
        title: str = "",
        *,
        font_size: Optional[int] = None,
        color: Optional[str] = None,
        layer: Optional[int] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.set_title",
            graph=self.name,
            title=title,
            font_size=font_size,
            color=color,
            layer=layer,
            **kwargs,
        )

    def add_curve(
        self,
        table: Union[str, TableProxy],
        y_col: Union[int, str],
        x_col: Optional[Union[int, str]] = None,
        style: str = "line_symbol",
        right_axis: bool = False,
        layer: Optional[int] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "plot.add_curve",
            graph=self.name,
            table=_name_of(table),
            x_col=x_col,
            y_col=y_col,
            style=style,
            right_axis=right_axis,
            layer=layer,
            **kwargs,
        )

    def remove_curve(
        self, curve: Union[int, str], layer: Optional[int] = None
    ) -> Dict[str, Any]:
        return self.session.call(
            "plot.remove_curve", graph=self.name, curve=curve, layer=layer
        )

    def add_error_bars(
        self,
        table: Union[str, TableProxy],
        curve: Union[int, str],
        err_col: Union[int, str],
        direction: str = "y",
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "plot.add_error_bars",
            graph=self.name,
            table=_name_of(table),
            curve=curve,
            err_col=err_col,
            direction=direction,
            **kwargs,
        )

    def add_layer(self, **kwargs: Any) -> Dict[str, Any]:
        return self.session.call("graph.add_layer", graph=self.name, **kwargs)

    def remove_layer(self, layer: int) -> Dict[str, Any]:
        return self.session.call("graph.remove_layer", graph=self.name, layer=int(layer))

    def add_linked_layer(
        self,
        link_x: bool = True,
        right_y: bool = False,
        *,
        link_y: Optional[bool] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        resolved_link_y = right_y if link_y is None else link_y
        return self.session.call(
            "graph.add_linked_layer",
            graph=self.name,
            link_x=link_x,
            link_y=resolved_link_y,
            **kwargs,
        )

    def add_inset(
        self,
        x_min: float,
        x_max: float,
        y_min: float,
        y_max: float,
        *,
        show_connectors: bool = True,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.add_inset_layer",
            graph=self.name,
            x_min=x_min,
            x_max=x_max,
            y_min=y_min,
            y_max=y_max,
            show_connectors=show_connectors,
            **kwargs,
        )

    add_inset_layer = add_inset

    def arrange_layers(
        self,
        rows: Optional[int] = None,
        cols: Optional[int] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.arrange_layers",
            graph=self.name,
            rows=(rows if (rows is not None and rows > 0) else None),
            cols=(cols if (cols is not None and cols > 0) else None),
            **kwargs,
        )

    def set_page_size(
        self,
        width: float,
        height: float,
        unit: str = "mm",
        dpi: int = 300,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.set_page_size",
            graph=self.name,
            width=width,
            height=height,
            unit=unit,
            dpi=dpi,
            **kwargs,
        )

    def fit_page_to_layers(self) -> Dict[str, Any]:
        return self.session.call("graph.fit_page_to_layers", graph=self.name)

    def fit_layers_to_page(self) -> Dict[str, Any]:
        return self.session.call("graph.fit_layers_to_page", graph=self.name)

    def apply_style_preset(self, preset: str) -> Dict[str, Any]:
        return self.session.call(
            "graph.apply_style_preset", graph=self.name, preset=preset
        )

    def isij_style(self) -> Dict[str, Any]:
        return self.apply_style_preset("isij_85mm")

    def slide_style(self) -> Dict[str, Any]:
        return self.apply_style_preset("slide_28pt")

    def nature_style(self) -> Dict[str, Any]:
        return self.apply_style_preset("nature_89mm")

    def apply_frame_preset(
        self, preset: str = "box_inward", layer: Optional[int] = None
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.apply_frame_preset", graph=self.name, preset=preset, layer=layer
        )

    def apply_square_frame(self) -> Dict[str, Any]:
        return self.session.call("graph.apply_square_frame", graph=self.name)

    def set_panel_labels(
        self,
        enabled: bool = True,
        placement: str = "outside_top_left",
        pattern: str = "(%a)",
        font_pt: int = 0,
        *,
        position: Optional[str] = None,
    ) -> Dict[str, Any]:
        eff_placement = position if (position is not None and str(position).strip()) else placement
        return self.session.call(
            "graph.set_panel_labels",
            graph=self.name,
            enabled=enabled,
            placement=eff_placement,
            pattern=pattern,
            font_pt=int(font_pt),
        )

    def sync_axis_color(
        self,
        axis: str = "right",
        curve: Optional[Union[int, str]] = None,
        layer: Optional[int] = None,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.sync_axis_color",
            graph=self.name,
            axis=axis,
            curve=curve,
            layer=layer,
        )

    def step_zoom_history(
        self, delta: int = -1, layer: Optional[int] = None
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.step_zoom_history", graph=self.name, delta=int(delta), layer=layer
        )

    def undo(self) -> Dict[str, Any]:
        return self.session.call("app.undo", graph=self.name)

    def redo(self) -> Dict[str, Any]:
        return self.session.call("app.redo", graph=self.name)

    def drop_columns(
        self,
        table: Union[str, TableProxy],
        columns: Sequence[Union[int, str]],
        x_column: Optional[Union[int, str]] = None,
        right_y: bool = False,
        replace: bool = False,
        layer: Optional[int] = None,
    ) -> Dict[str, Any]:
        col_list = (
            [columns]
            if isinstance(columns, (str, int))
            else [c for c in columns]
        )
        return self.session.call(
            "graph.drop_columns",
            table=_name_of(table),
            columns=col_list,
            x_column=(x_column if x_column is not None else ""),
            graph=self.name,
            layer=(int(layer) if layer is not None else None),
            right_y=bool(right_y),
            replace=bool(replace),
        )

    def set_aspect_ratio(
        self,
        enabled: bool = True,
        ratio: float = 1.0,
        *,
        keep: Optional[bool] = None,
    ) -> Dict[str, Any]:
        use_keep = enabled if keep is None else keep
        return self.session.call(
            "graph.set_aspect_ratio", graph=self.name, keep=use_keep, ratio=ratio
        )

    def copy_format(self, layer: Optional[int] = None) -> Dict[str, Any]:
        return self.session.call("graph.copy_format", graph=self.name, layer=layer)

    def paste_format(
        self,
        layer: Optional[int] = None,
        scopes: Optional[Sequence[str]] = None,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.paste_format",
            graph=self.name,
            layer=layer,
            scopes=(list(scopes) if scopes is not None else None),
        )

    def batch_clone(
        self, tables: Sequence[Union[str, TableProxy]], **kwargs: Any
    ) -> Dict[str, Any]:
        t_names = [_name_of(t) for t in tables if t is not None]
        return self.session.call(
            "graph.batch_clone", graph=self.name, tables=t_names, **kwargs
        )

    def set_axis(
        self,
        axis: str = "x",
        *,
        layer: Optional[int] = None,
        min: Optional[float] = None,
        max: Optional[float] = None,
        lim: Optional[Sequence[float]] = None,
        step: Optional[float] = None,
        scale: Optional[str] = None,
        title: Optional[str] = None,
        font_family: Optional[str] = None,
        font_size: Optional[int] = None,
        title_font_size: Optional[int] = None,
        numbers_font_size: Optional[int] = None,
        ticks_inward: Optional[bool] = None,
        show_backbone: Optional[bool] = None,
        show_labels: Optional[bool] = None,
        show_ticks: Optional[bool] = None,
        unit_format: Optional[str] = None,
        tick_format: Optional[Union[str, int]] = None,
        precision: Optional[int] = None,
        label_rotation: Optional[float] = None,
        divide_by: Optional[float] = None,
        prefix: Optional[str] = None,
        suffix: Optional[str] = None,
        inverted: Optional[bool] = None,
        enabled: Optional[bool] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.set_axis",
            graph=self.name,
            layer=layer,
            axis=axis,
            min=min,
            max=max,
            lim=(list(lim) if lim is not None else None),
            step=step,
            scale=scale,
            title=title,
            font_family=font_family,
            font_size=font_size,
            title_font_size=title_font_size,
            numbers_font_size=numbers_font_size,
            ticks_inward=ticks_inward,
            show_backbone=show_backbone,
            show_labels=show_labels,
            show_ticks=show_ticks,
            unit_format=unit_format,
            tick_format=tick_format,
            precision=precision,
            label_rotation=label_rotation,
            divide_by=divide_by,
            prefix=prefix,
            suffix=suffix,
            inverted=inverted,
            enabled=enabled,
            **kwargs,
        )

    def set_axis_break(
        self,
        axis: str = "x",
        *,
        enabled: bool = True,
        start: Optional[float] = None,
        end: Optional[float] = None,
        position_pct: float = 0.5,
        log_after: Optional[bool] = None,
        layer: Optional[int] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.set_axis_break",
            graph=self.name,
            layer=layer,
            axis=axis,
            enabled=enabled,
            start=start,
            end=end,
            position_pct=position_pct,
            log_after=log_after,
            **kwargs,
        )

    def add_reference_line(
        self,
        value: float = 0.0,
        orientation: str = "horizontal",
        label: str = "",
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.add_reference_line",
            graph=self.name,
            value=value,
            orientation=orientation,
            label=label,
            **kwargs,
        )

    def add_shaded_band(self, **kwargs: Any) -> Dict[str, Any]:
        return self.session.call("graph.add_shaded_band", graph=self.name, **kwargs)

    def remove_reference_line(
        self, index: int = 0, *, layer: Optional[int] = None
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.remove_reference_line",
            graph=self.name,
            index=int(index),
            layer=layer,
        )

    def clear_reference_lines(
        self, *, include_bands: bool = False, layer: Optional[int] = None
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.clear_reference_lines",
            graph=self.name,
            include_bands=bool(include_bands),
            layer=layer,
        )

    def set_grid(self, **kwargs: Any) -> Dict[str, Any]:
        return self.session.call("graph.set_grid", graph=self.name, **kwargs)

    def set_curve_style(
        self,
        curve_index: Optional[Union[int, str]] = 0,
        *,
        curve: Optional[Union[int, str]] = None,
        color: Optional[str] = None,
        line_width: Optional[float] = None,
        linewidth: Optional[float] = None,
        lw: Optional[float] = None,
        line_style: Optional[str] = None,
        linestyle: Optional[str] = None,
        ls: Optional[str] = None,
        alpha: Optional[float] = None,
        symbol_type: Optional[str] = None,
        marker: Optional[str] = None,
        symbol_size: Optional[int] = None,
        markersize: Optional[int] = None,
        ms: Optional[int] = None,
        symbol_skip: Optional[int] = None,
        symbol_hollow: Optional[bool] = None,
        symbol_fill: Optional[bool] = None,
        visible: Optional[bool] = None,
        y_axis: Optional[str] = None,
        fill_area: Optional[bool] = None,
        bar_gap: Optional[int] = None,
        bar_offset: Optional[int] = None,
        label: Optional[str] = None,
        layer: Optional[int] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        resolved_idx: Optional[int] = None
        resolved_curve: Optional[Union[int, str]] = curve
        if isinstance(curve_index, str):
            resolved_curve = curve_index
        elif curve is None and curve_index is not None:
            resolved_idx = int(curve_index)
        return self.session.call(
            "graph.set_curve_style",
            graph=self.name,
            layer=layer,
            curve_index=resolved_idx,
            curve=resolved_curve,
            color=color,
            line_width=line_width,
            linewidth=linewidth,
            lw=lw,
            line_style=line_style,
            linestyle=linestyle,
            ls=ls,
            alpha=alpha,
            symbol_type=symbol_type,
            marker=marker,
            symbol_size=symbol_size,
            markersize=markersize,
            ms=ms,
            symbol_skip=symbol_skip,
            symbol_hollow=symbol_hollow,
            symbol_fill=symbol_fill,
            visible=visible,
            y_axis=y_axis,
            fill_area=fill_area,
            bar_gap=bar_gap,
            bar_offset=bar_offset,
            label=label,
            **kwargs,
        )

    def set_curve_visible(
        self,
        curve: Union[int, str] = 0,
        visible: bool = True,
        layer: Optional[int] = None,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.set_curve_visible",
            graph=self.name,
            layer=layer,
            curve=curve,
            visible=bool(visible),
        )

    def move_curve(
        self,
        from_index: int,
        to_index: int,
        layer: Optional[int] = None,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.move_curve",
            graph=self.name,
            layer=layer,
            from_index=int(from_index),
            to_index=int(to_index),
        )

    def set_stack_offsets(
        self,
        mode: Union[int, str] = 1,
        y_offset_pct: float = 15.0,
        x_offset_pct: float = 0.0,
        normalize: bool = False,
        layer: Optional[int] = None,
        *,
        y_gap_pct: Optional[float] = None,
        x_gap_pct: Optional[float] = None,
        reset: bool = False,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.set_stack_offsets",
            graph=self.name,
            layer=layer,
            mode=mode,
            y_gap_pct=(y_offset_pct if y_gap_pct is None else y_gap_pct),
            x_gap_pct=(x_offset_pct if x_gap_pct is None else x_gap_pct),
            normalize=normalize,
            reset=reset,
        )

    def apply_series_scheme(
        self,
        scheme: str = "bw_hollow_filled",
        layer: Optional[int] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.apply_series_scheme",
            graph=self.name,
            scheme=scheme,
            layer=layer,
            **kwargs,
        )

    def apply_palette(
        self,
        palette: str = "okabe_ito",
        layer: Optional[int] = None,
        gradient: Optional[bool] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.apply_palette",
            graph=self.name,
            palette=palette,
            layer=layer,
            gradient=gradient,
            **kwargs,
        )

    def set_colorbar(
        self,
        palette: Optional[str] = None,
        z_min: Optional[float] = None,
        z_max: Optional[float] = None,
        title: Optional[str] = None,
        show_contour_lines: Optional[bool] = None,
        levels: Optional[int] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.set_colorbar",
            graph=self.name,
            palette=palette,
            z_min=z_min,
            z_max=z_max,
            title=title,
            show_contour_lines=show_contour_lines,
            levels=levels,
            **kwargs,
        )

    def set_legend(
        self,
        *,
        position: Optional[str] = None,
        visible: Optional[bool] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.set_legend",
            graph=self.name,
            position=position,
            visible=visible,
            **kwargs,
        )

    def auto_place_legend(
        self,
        margin_px: int = 12,
        layer: Optional[int] = None,
        *,
        position: Optional[str] = None,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.auto_place_legend",
            graph=self.name,
            margin_px=int(margin_px),
            layer=layer,
            position=position,
        )

    def add_text(
        self,
        text: str,
        x: Optional[float] = None,
        y: Optional[float] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.add_text", graph=self.name, text=text, x=x, y=y, **kwargs
        )

    def add_arrow(
        self, x1: float, y1: float, x2: float, y2: float, **kwargs: Any
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.add_arrow",
            graph=self.name,
            x1=x1,
            y1=y1,
            x2=x2,
            y2=y2,
            **kwargs,
        )

    def remove_marker(
        self,
        marker_id: Optional[int] = None,
        *,
        kind: Optional[str] = None,
        index: Optional[int] = None,
        clear_all: bool = False,
        layer: Optional[int] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.remove_marker",
            graph=self.name,
            id=marker_id,
            kind=kind,
            index=index,
            clear_all=bool(clear_all),
            layer=layer,
            **kwargs,
        )

    def mask_region(
        self,
        x_min: float,
        x_max: float,
        y_min: float,
        y_max: float,
        curve: Optional[Union[int, str]] = None,
        unmask: bool = False,
        *,
        mask: Optional[bool] = None,
        layer: Optional[int] = None,
    ) -> Dict[str, Any]:
        do_mask = (not unmask) if mask is None else mask
        return self.session.call(
            "graph.mask_region",
            graph=self.name,
            layer=layer,
            x_min=x_min,
            x_max=x_max,
            y_min=y_min,
            y_max=y_max,
            curve=curve,
            mask=do_mask,
        )

    def clear_masks(
        self,
        curve: Optional[Union[int, str]] = None,
        layer: Optional[int] = None,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.clear_masks", graph=self.name, curve=curve, layer=layer
        )

    def export(
        self,
        path: Union[str, os.PathLike[str]],
        dpi: int = 300,
        transparent: bool = False,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.export",
            graph=self.name,
            path=os.fspath(path),
            dpi=dpi,
            transparent=transparent,
            **kwargs,
        )

    def copy_image(self, dpi: int = 300) -> Dict[str, Any]:
        return self.session.call("graph.copy_image", graph=self.name, dpi=dpi)

    def copy_ole(self) -> Dict[str, Any]:
        return self.session.call("graph.copy_ole", graph=self.name)

    def set_3d_options(
        self,
        *,
        title: Optional[str] = None,
        title_color: Optional[str] = None,
        title_font_size: Optional[int] = None,
        colormap: Optional[str] = None,
        palette: Optional[str] = None,
        reverse_colormap: Optional[bool] = None,
        from_color: Optional[str] = None,
        to_color: Optional[str] = None,
        x_rotation: Optional[float] = None,
        y_rotation: Optional[float] = None,
        z_rotation: Optional[float] = None,
        zoom: Optional[float] = None,
        orthogonal: Optional[bool] = None,
        style: Optional[str] = None,
        coord_style: Optional[str] = None,
        axes_labels: Optional[Sequence[str]] = None,
        x_scale: Optional[float] = None,
        y_scale: Optional[float] = None,
        z_scale: Optional[float] = None,
        scale: Optional[Sequence[float]] = None,
        x_range: Optional[Sequence[float]] = None,
        y_range: Optional[Sequence[float]] = None,
        z_range: Optional[Sequence[float]] = None,
        resolution: Optional[int] = None,
        legend: Optional[bool] = None,
        transparency: Optional[float] = None,
        mesh_line_width: Optional[float] = None,
        smooth_mesh: Optional[bool] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Configure 3D graph appearance, colormap, camera angles, axes, and mesh options."""
        return self.session.call(
            "graph.set_3d_options",
            graph=self.name,
            title=title,
            title_color=title_color,
            title_font_size=title_font_size,
            colormap=colormap,
            palette=palette,
            reverse_colormap=reverse_colormap,
            from_color=from_color,
            to_color=to_color,
            x_rotation=x_rotation,
            y_rotation=y_rotation,
            z_rotation=z_rotation,
            zoom=zoom,
            orthogonal=orthogonal,
            style=style,
            coord_style=coord_style,
            axes_labels=(list(axes_labels) if axes_labels is not None else None),
            x_scale=x_scale,
            y_scale=y_scale,
            z_scale=z_scale,
            scale=(list(scale) if scale is not None else None),
            x_range=(list(x_range) if x_range is not None else None),
            y_range=(list(y_range) if y_range is not None else None),
            z_range=(list(z_range) if z_range is not None else None),
            resolution=resolution,
            legend=legend,
            transparency=transparency,
            mesh_line_width=mesh_line_width,
            smooth_mesh=smooth_mesh,
            **kwargs,
        )

    def attach_gadget(self, gadget_type: str = "roi_stats", **kwargs: Any) -> Dict[str, Any]:
        return self.session.call(
            "gadget.attach", graph=self.name, type=gadget_type, **kwargs
        )

    def set_gadget_roi(
        self,
        x_min: Optional[float] = None,
        x_max: Optional[float] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "gadget.set_roi", graph=self.name, x_min=x_min, x_max=x_max, **kwargs
        )

    def query_gadget(
        self, gadget_type: Optional[str] = None, **kwargs: Any
    ) -> Dict[str, Any]:
        return self.session.call(
            "gadget.query", graph=self.name, type=gadget_type, **kwargs
        )

    def clear_gadgets(self, gadget_type: str = "all") -> Dict[str, Any]:
        return self.session.call("gadget.clear", graph=self.name, type=gadget_type)

    def analyze_tensile(
        self,
        curve: Optional[Union[int, str]] = None,
        *,
        layer: Optional[int] = None,
        offset: float = 0.002,
        strain_in_percent: Optional[bool] = None,
        elastic_min: Optional[float] = None,
        elastic_max: Optional[float] = None,
        plot_construction: bool = True,
        add_callout: bool = True,
        create_true_stress_table: bool = False,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "curvemath.offset_yield",
            graph=self.name,
            layer=layer,
            curve=curve,
            offset=float(offset),
            strain_in_percent=strain_in_percent,
            elastic_min=elastic_min,
            elastic_max=elastic_max,
            plot_construction=bool(plot_construction),
            add_callout=bool(add_callout),
            create_true_stress_table=bool(create_true_stress_table),
            **kwargs,
        )

    offset_yield = analyze_tensile


class Session:
    """Connected ToriGraph JSON-RPC 2.0 Automation Session."""

    def __init__(
        self,
        transport: Union[_TcpTransport, _LocalPipeTransport],
        process: Optional[subprocess.Popen[bytes]] = None,
        cleanup_files: Optional[Sequence[str]] = None,
    ) -> None:
        self._transport = transport
        self._process = process
        self._cleanup_files = list(cleanup_files or [])
        self._next_id = 1
        self._closed = False
        self._finalizer: Optional[weakref.finalize] = (
            weakref.finalize(
                self,
                _cleanup_spawned_session,
                self._process,
                self._transport,
                list(self._cleanup_files),
            )
            if (self._process is not None or bool(self._cleanup_files))
            else None
        )

        # Callable + dotted domain namespaces
        self.app = _NamespaceProxy(self, "app")
        self.table = _TableDomain(self)
        self.matrix = _MatrixDomain(self)
        self.plot = _NamespaceProxy(self, "plot")
        self.graph = _GraphDomain(self)
        self.analysis = _NamespaceProxy(self, "analysis")
        self.gadget = _NamespaceProxy(self, "gadget")
        self.signal = _NamespaceProxy(self, "signal")
        self.curvemath = _NamespaceProxy(self, "curvemath")
        self.stats = _NamespaceProxy(self, "stats")
        self.digitizer = _NamespaceProxy(self, "digitizer")
        self.table_ops = self.table
        self.matrix_ops = self.matrix
        self.graph_ops = self.graph

    def __enter__(self) -> "Session":
        return self

    def __exit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None:
        self.close()

    @property
    def endpoint(self) -> str:
        return self._transport.endpoint

    @property
    def process(self) -> Optional[subprocess.Popen[bytes]]:
        return self._process

    def close(self) -> None:
        if self._closed:
            return
        self._closed = True
        if self._finalizer is not None:
            self._finalizer.detach()
        proc = self._process
        files = list(self._cleanup_files)
        self._process = None
        self._cleanup_files.clear()
        _cleanup_spawned_session(proc, self._transport, files)

    def call(
        self,
        _method: str,
        _params: Optional[Mapping[str, Any]] = None,
        /,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Invoke any ToriGraph JSON-RPC 2.0 method and return its result dictionary."""
        if self._closed:
            raise TransportError("Session is already closed.")

        merged: Dict[str, Any] = {}
        if _params is not None:
            merged.update(_sanitize_for_json(dict(_params)))
        if kwargs:
            merged.update(_clean_kwargs(kwargs))

        req_id = self._next_id
        self._next_id += 1
        req_obj = {
            "jsonrpc": "2.0",
            "id": req_id,
            "method": str(_method),
            "params": merged,
        }
        raw_req = json.dumps(req_obj, ensure_ascii=False, allow_nan=False).encode("utf-8")
        raw_resp = self._transport.request_line(raw_req)

        try:
            resp = json.loads(raw_resp.decode("utf-8"))
        except Exception as exc:
            raise TransportError(f"Malformed JSON response from ToriGraph: {exc}") from exc

        if "error" in resp and resp["error"] is not None:
            err = resp["error"]
            if isinstance(err, Mapping):
                raise RpcError(
                    int(err.get("code", -32000)),
                    str(err.get("message", "Unknown RPC error")),
                    err.get("data"),
                )
            raise RpcError(-32000, str(err))

        result = resp.get("result")
        return result if isinstance(result, dict) else {"value": result}

    # ------------------------------------------------------------------
    # 1. App / Project / Window / Action / Update
    # ------------------------------------------------------------------
    def ping(self) -> Dict[str, Any]:
        return self.call("ping")

    def info(self) -> Dict[str, Any]:
        return self.call("app.info")

    def supported_methods(self) -> List[str]:
        res = self.call("app.supported_methods")
        return list(res.get("methods", []))

    def describe_api(
        self,
        method: Optional[str] = None,
        *,
        domain: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Return structured JSON-RPC 2.0 self-discovery catalog (all methods, by domain, or single method)."""
        return self.call("app.describe_api", method=method, domain=domain)

    def new_project(self) -> Dict[str, Any]:
        return self.call("app.new_project")

    def open_project(
        self, path: Union[str, os.PathLike[str]], append: bool = False
    ) -> Dict[str, Any]:
        return self.call("app.open_project", path=os.fspath(path), append=append)

    def save_project(
        self,
        path: Optional[Union[str, os.PathLike[str]]] = None,
        compress: bool = False,
    ) -> Dict[str, Any]:
        return self.call(
            "app.save_project",
            path=os.fspath(path) if path is not None else None,
            compress=compress,
        )

    def import_csv(
        self,
        path: Union[str, os.PathLike[str]],
        separator: Optional[str] = None,
        skip_lines: int = 0,
        header: bool = True,
        with_comments: bool = False,
        name: Optional[str] = None,
        *,
        encoding: str = "auto",
        units_row: Optional[Union[int, bool, str]] = None,
        numeric: bool = True,
        **kwargs: Any,
    ) -> TableProxy:
        res = self.call(
            "app.import_csv",
            path=os.fspath(path),
            separator=separator,
            skip_lines=skip_lines,
            header=header,
            with_comments=with_comments,
            name=name,
            encoding=encoding,
            units_row=units_row,
            numeric=numeric,
            **kwargs,
        )
        return TableProxy(self, res["name"])

    def paste_tsv(
        self,
        text: Optional[str] = None,
        *,
        table: Optional[Union[str, TableProxy]] = None,
        name: Optional[str] = None,
        header: Optional[bool] = None,
        separator: Optional[str] = None,
        encoding: str = "auto",
        units_row: Optional[Union[int, bool, str]] = None,
        numeric: bool = True,
        start_row: int = 0,
        start_col: int = 0,
        **kwargs: Any,
    ) -> TableProxy:
        res = self.call(
            "table.paste_tsv",
            text=text,
            table=_name_of(table),
            name=name,
            header=header,
            separator=separator,
            encoding=encoding,
            units_row=units_row,
            numeric=numeric,
            start_row=int(start_row),
            start_col=int(start_col),
            **kwargs,
        )
        return TableProxy(self, res["name"])

    def import_opj(
        self, path: Union[str, os.PathLike[str]], codec: str = ""
    ) -> Dict[str, Any]:
        return self.call("app.import_opj", path=os.fspath(path), codec=codec)

    def list_windows(self, window_type: Optional[str] = None) -> List[Dict[str, Any]]:
        res = self.call("app.list_windows", type=window_type)
        return list(res.get("windows", []))

    def window_info(
        self, name: Union[str, TableProxy, MatrixProxy, GraphProxy]
    ) -> Dict[str, Any]:
        return self.call("app.window_info", name=_name_of(name))

    def activate_window(
        self, name: Union[str, TableProxy, MatrixProxy, GraphProxy]
    ) -> Dict[str, Any]:
        return self.call("app.activate_window", name=_name_of(name))

    def rename_window(
        self, name: Union[str, TableProxy, MatrixProxy, GraphProxy], new_name: str
    ) -> Dict[str, Any]:
        res = self.call("app.rename_window", name=_name_of(name), new_name=new_name)
        if isinstance(name, (TableProxy, MatrixProxy, GraphProxy)) and res.get("name"):
            name.name = str(res["name"])
        return res

    def close_window(
        self, name: Union[str, TableProxy, MatrixProxy, GraphProxy]
    ) -> Dict[str, Any]:
        return self.call("app.close_window", name=_name_of(name))

    def clone_window(
        self,
        name: Union[str, TableProxy, MatrixProxy, GraphProxy],
        new_name: Optional[str] = None,
    ) -> Dict[str, Any]:
        return self.call("app.clone_window", name=_name_of(name), new_name=new_name)

    def set_language(self, language: str) -> Dict[str, Any]:
        return self.call("app.set_language", language=language)

    def get_language(self) -> str:
        res = self.call("app.get_language")
        return str(res.get("language", ""))

    def set_recalc_mode(self, mode: str = "auto") -> Dict[str, Any]:
        return self.call("app.set_recalc_mode", mode=mode)

    def recalculate(self) -> Dict[str, Any]:
        return self.call("app.recalculate")

    def undo(
        self,
        window: Optional[Union[str, TableProxy, MatrixProxy, GraphProxy]] = None,
        *,
        graph: Optional[Union[str, GraphProxy]] = None,
        table: Optional[Union[str, TableProxy]] = None,
        matrix: Optional[Union[str, MatrixProxy]] = None,
    ) -> Dict[str, Any]:
        win_name: Optional[str] = None
        if isinstance(window, GraphProxy) and graph is None:
            graph = window
        elif isinstance(window, TableProxy) and table is None:
            table = window
        elif isinstance(window, MatrixProxy) and matrix is None:
            matrix = window
        elif window is not None:
            win_name = _name_of(window)
        return self.call(
            "app.undo",
            window=win_name,
            graph=_name_of(graph),
            table=_name_of(table),
            matrix=_name_of(matrix),
        )

    def redo(
        self,
        window: Optional[Union[str, TableProxy, MatrixProxy, GraphProxy]] = None,
        *,
        graph: Optional[Union[str, GraphProxy]] = None,
        table: Optional[Union[str, TableProxy]] = None,
        matrix: Optional[Union[str, MatrixProxy]] = None,
    ) -> Dict[str, Any]:
        win_name: Optional[str] = None
        if isinstance(window, GraphProxy) and graph is None:
            graph = window
        elif isinstance(window, TableProxy) and table is None:
            table = window
        elif isinstance(window, MatrixProxy) and matrix is None:
            matrix = window
        elif window is not None:
            win_name = _name_of(window)
        return self.call(
            "app.redo",
            window=win_name,
            graph=_name_of(graph),
            table=_name_of(table),
            matrix=_name_of(matrix),
        )

    def drop_columns(
        self,
        table: Union[str, TableProxy],
        columns: Sequence[Union[int, str]],
        x_column: Optional[Union[int, str]] = None,
        graph: Optional[Union[str, GraphProxy]] = None,
        layer: Optional[int] = None,
        right_y: bool = False,
        replace: bool = False,
    ) -> GraphProxy:
        col_list = (
            [columns]
            if isinstance(columns, (str, int))
            else [c for c in columns]
        )
        res = self.call(
            "graph.drop_columns",
            table=_name_of(table),
            columns=col_list,
            x_column=(x_column if x_column is not None else ""),
            graph=(_name_of(graph) if graph is not None else ""),
            layer=(int(layer) if layer is not None else None),
            right_y=bool(right_y),
            replace=bool(replace),
        )
        g_name = res.get("graph") or res.get("name") or (_name_of(graph) if graph is not None else "")
        return GraphProxy(self, str(g_name))

    def set_focus_hud(self, enabled: bool) -> Dict[str, Any]:
        return self.call("app.set_focus_hud", enabled=enabled)

    def get_focus_hud(self) -> bool:
        res = self.call("app.get_focus_hud")
        return bool(res.get("enabled", False))

    def set_workspace_mode(self, mode: str) -> Dict[str, Any]:
        return self.call("app.set_workspace_mode", mode=mode)

    def check_updates(
        self,
        feed_url: Optional[str] = None,
        include_prerelease: bool = False,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.call(
            "app.check_updates",
            feed_url=feed_url,
            include_prerelease=include_prerelease,
            **kwargs,
        )

    def list_actions(self, filter: Optional[str] = None) -> List[Dict[str, Any]]:
        res = self.call("app.list_actions", filter=filter)
        return list(res.get("actions", []))

    def trigger_action(
        self, name: str, checked: Optional[bool] = None, **kwargs: Any
    ) -> Dict[str, Any]:
        return self.call("app.trigger_action", name=name, checked=checked, **kwargs)

    # ------------------------------------------------------------------
    # 2 & 3. Table & Matrix Creation
    # ------------------------------------------------------------------
    def create_table(
        self,
        rows: Union[int, str] = 30,
        cols: Optional[int] = None,
        name: Optional[str] = None,
        *,
        col_names: Optional[Sequence[str]] = None,
        designations: Optional[Union[Sequence[str], Mapping[str, str]]] = None,
        data: Optional[Mapping[str, Sequence[Any]]] = None,
        **kwargs: Any,
    ) -> TableProxy:
        if isinstance(rows, str):
            if name is None:
                name = rows
            rows = 30
        if data is not None:
            data_keys = list(data.keys())
            if col_names is None:
                col_names = data_keys
            inferred_rows = max((len(v) for v in data.values()), default=1)
            rows = max(1, inferred_rows) if cols is None else max(int(rows), inferred_rows)
        resolved_cols = (
            cols
            if cols is not None
            else (max(1, len(col_names)) if col_names is not None else 2)
        )
        if col_names is not None and resolved_cols > 0 and len(col_names) > resolved_cols:
            resolved_cols = len(col_names)
        res = self.call(
            "table.create",
            rows=int(rows),
            cols=resolved_cols,
            name=name,
            col_names=(list(col_names) if col_names is not None else None),
            designations=(
                dict(designations)
                if isinstance(designations, Mapping)
                else (list(designations) if designations is not None else None)
            ),
            **kwargs,
        )
        tbl = TableProxy(self, res["name"])
        if data is not None:
            for k, vals in data.items():
                tbl.set_column(k, list(vals))
        return tbl

    def create_matrix(
        self,
        rows: Union[int, str] = 32,
        cols: int = 32,
        name: Optional[str] = None,
        x_min: float = 0.0,
        x_max: float = 1.0,
        y_min: float = 0.0,
        y_max: float = 1.0,
        *,
        values: Optional[Sequence[Sequence[Optional[float]]]] = None,
        data: Optional[Sequence[Sequence[Optional[float]]]] = None,
        **kwargs: Any,
    ) -> MatrixProxy:
        if isinstance(rows, str):
            if name is None:
                name = rows
            rows = 32
        grid = values if values is not None else data
        if grid is not None and len(grid) > 0:
            rows = len(grid)
            cols = len(grid[0]) if len(grid[0]) > 0 else 1
        res = self.call(
            "matrix.create",
            rows=int(rows),
            cols=int(cols),
            name=name,
            x_min=x_min,
            x_max=x_max,
            y_min=y_min,
            y_max=y_max,
            **kwargs,
        )
        mat = MatrixProxy(self, res["name"])
        if grid is not None:
            mat.set_data(grid)
        return mat

    # ------------------------------------------------------------------
    # 4. 2D / 3D / Matrix Plotting
    # ------------------------------------------------------------------
    def plot_xy(
        self,
        table: Union[str, TableProxy],
        x_col: Optional[Union[int, str]] = None,
        y_cols: Optional[Union[int, str, Sequence[Union[int, str]]]] = None,
        y_err_col: Optional[Union[int, str]] = None,
        x_err_col: Optional[Union[int, str]] = None,
        style: str = "line_symbol",
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        if graph_name is None and "name" in kwargs:
            graph_name = kwargs.pop("name")
        res = self.call(
            "plot.xy",
            table=_name_of(table),
            x_col=x_col,
            y_cols=y_cols,
            y_err_col=y_err_col,
            x_err_col=x_err_col,
            style=style,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_stacked_bars(
        self,
        table: Union[str, TableProxy],
        x_col: Optional[Union[int, str]] = None,
        y_cols: Optional[Sequence[Union[int, str]]] = None,
        horizontal: bool = False,
        bar_gap: int = 25,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        res = self.call(
            "plot.stacked_bars",
            table=_name_of(table),
            x_col=x_col,
            y_cols=(list(y_cols) if y_cols is not None else None),
            horizontal=horizontal,
            bar_gap=bar_gap,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_double_y(
        self,
        table: Union[str, TableProxy],
        x_col: Optional[Union[int, str]] = None,
        y1_col: Optional[Union[int, str]] = None,
        y2_col: Optional[Union[int, str]] = None,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        res = self.call(
            "plot.double_y",
            table=_name_of(table),
            x_col=x_col,
            y1_col=y1_col,
            y2_col=y2_col,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_waterfall(
        self,
        table: Union[str, TableProxy],
        x_col: Optional[Union[int, str]] = None,
        y_cols: Optional[Sequence[Union[int, str]]] = None,
        y_offset_pct: float = 15.0,
        x_offset_pct: float = 5.0,
        normalize: bool = False,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        res = self.call(
            "plot.waterfall",
            table=_name_of(table),
            x_col=x_col,
            y_cols=y_cols,
            y_offset_pct=y_offset_pct,
            x_offset_pct=x_offset_pct,
            normalize=normalize,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_histogram(
        self,
        table: Union[str, TableProxy],
        cols: Optional[Union[int, str, Sequence[Union[int, str]]]] = None,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        key = "cols" if isinstance(cols, (list, tuple)) else "col"
        params = {"table": _name_of(table), **kwargs}
        if graph_name is not None:
            params["graph_name"] = graph_name
        if cols is not None:
            params[key] = cols
        res = self.call("plot.histogram", params)
        return GraphProxy(self, res["name"])

    def plot_box(
        self,
        table: Union[str, TableProxy],
        cols: Optional[Sequence[Union[int, str]]] = None,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        res = self.call(
            "plot.box",
            table=_name_of(table),
            cols=cols,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_violin(
        self,
        table: Union[str, TableProxy],
        cols: Optional[Sequence[Union[int, str]]] = None,
        show_points: bool = True,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        res = self.call(
            "plot.violin",
            table=_name_of(table),
            cols=cols,
            show_points=show_points,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_pareto(
        self,
        table: Union[str, TableProxy],
        col: Optional[Union[int, str]] = None,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        res = self.call(
            "plot.pareto",
            table=_name_of(table),
            col=col,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_qq(
        self,
        table: Union[str, TableProxy],
        col: Optional[Union[int, str]] = None,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        res = self.call(
            "plot.qq",
            table=_name_of(table),
            col=col,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_pie(
        self,
        table: Union[str, TableProxy],
        col: Optional[Union[int, str]] = None,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        res = self.call(
            "plot.pie",
            table=_name_of(table),
            col=col,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_vectors(
        self,
        table: Union[str, TableProxy],
        cols: Optional[Sequence[Union[int, str]]] = None,
        mode: str = "xyxy",
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        res = self.call(
            "plot.vectors",
            table=_name_of(table),
            cols=cols,
            mode=mode,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_ternary(
        self,
        table: Union[str, TableProxy],
        a_col: Union[int, str] = 0,
        b_col: Union[int, str] = 1,
        c_col: Union[int, str] = 2,
        *,
        cols: Optional[Sequence[Union[int, str]]] = None,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        chosen_cols = list(cols) if cols is not None else [a_col, b_col, c_col]
        res = self.call(
            "plot.ternary",
            table=_name_of(table),
            cols=chosen_cols,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_function(
        self,
        formula: str,
        x_min: float = 0.0,
        x_max: float = 10.0,
        points: int = 200,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        res = self.call(
            "plot.function",
            formula=formula,
            x_min=x_min,
            x_max=x_max,
            points=points,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_matrix(
        self,
        matrix: Union[str, MatrixProxy],
        style: str = "colormap",
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        res = self.call(
            "plot.matrix",
            matrix=_name_of(matrix),
            style=style,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_3d_matrix(
        self,
        matrix: Union[str, MatrixProxy],
        style: str = "surface",
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        res = self.call(
            "plot.3d_matrix",
            matrix=_name_of(matrix),
            style=style,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_3d_xyz(
        self,
        table: Union[str, TableProxy],
        x_col: Union[int, str] = 0,
        y_col: Union[int, str] = 1,
        z_col: Union[int, str] = 2,
        style: str = "scatter",
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        res = self.call(
            "plot.3d_xyz",
            table=_name_of(table),
            x_col=x_col,
            y_col=y_col,
            z_col=z_col,
            style=style,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_3d_function(
        self,
        formula: str,
        x_min: float = -5.0,
        x_max: float = 5.0,
        y_min: float = -5.0,
        y_max: float = 5.0,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        res = self.call(
            "plot.3d_function",
            formula=formula,
            x_min=x_min,
            x_max=x_max,
            y_min=y_min,
            y_max=y_max,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    # ------------------------------------------------------------------
    # 6. Curve Fitting & Peak Analysis
    # ------------------------------------------------------------------
    def list_fit_models(self) -> List[Dict[str, Any]]:
        res = self.call("analysis.list_fit_models")
        return list(res.get("models", []))

    def fit_linear(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.fit_linear", **kwargs)

    def fit_york(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.fit_york", **kwargs)

    def fit_polynomial(self, order: int = 2, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.fit_polynomial", order=order, **kwargs)

    def fit_nonlinear(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.fit_nonlinear", **kwargs)

    def fit_global(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.fit_global", **kwargs)

    def fit_piecewise(self, segments: int = 2, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.fit_piecewise", segments=segments, **kwargs)

    def fit_robust(self, method: str = "bisquare", **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.fit_robust", method=method, **kwargs)

    def fit_quantile(self, tau: float = 0.5, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.fit_quantile", tau=tau, **kwargs)

    def fit_logistic(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.fit_logistic", **kwargs)

    def fit_mlr(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.fit_mlr", **kwargs)

    def fit_implicit_odr(
        self,
        shape: Optional[str] = None,
        *,
        model: Optional[str] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        chosen_model = (
            model
            if model is not None
            else (
                shape
                if shape is not None
                else ("custom" if kwargs.get("formula") else "circle")
            )
        )
        return self.call("analysis.fit_implicit_odr", model=chosen_model, **kwargs)

    def fit_surface_3d(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.fit_surface_3d", **kwargs)

    def compare_models(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.compare_models", **kwargs)

    def compare_datasets(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.compare_datasets", **kwargs)

    def analyze_peaks(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.analyze_peaks", **kwargs)

    def batch_peaks(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.batch_peaks", **kwargs)

    # ------------------------------------------------------------------
    # 7. Gadgets, Signal Processing, Curve Math, MatSci, Statistics & Digitizer
    # ------------------------------------------------------------------
    def attach_gadget(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("gadget.attach", **kwargs)

    def set_gadget_roi(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("gadget.set_roi", **kwargs)

    def query_gadget(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("gadget.query", **kwargs)

    def clear_gadgets(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("gadget.clear", **kwargs)

    clear_gadget = clear_gadgets

    def smooth(self, method: str = "savgol", **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.smooth", method=method, **kwargs)

    def savgol_derivative(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.savgol_derivative", **kwargs)

    def fft(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.fft", **kwargs)

    def fft_filter(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.fft_filter", **kwargs)

    def iir_filter(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.iir_filter", **kwargs)

    def stft(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.stft", **kwargs)

    def envelope(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.envelope", **kwargs)

    hilbert_envelope = envelope

    def wavelet_cwt(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.wavelet_cwt", **kwargs)

    def wavelet_denoise(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.wavelet_denoise", **kwargs)

    def welch_psd(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.welch_psd", **kwargs)

    def coherence(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.coherence", **kwargs)

    def decimate(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.decimate", **kwargs)

    def fsd(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.fsd", **kwargs)

    fourier_self_deconvolution = fsd

    def curve_eval(self, op: str = "-", **kwargs: Any) -> Dict[str, Any]:
        return self.call("curvemath.eval", op=op, **kwargs)

    curve_math = curve_eval
    eval_curves = curve_eval

    def average_curves(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("curvemath.average_curves", **kwargs)

    def normalize_curve(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("curvemath.normalize", **kwargs)

    def integrate_curve(
        self,
        graph: Optional[Union[str, GraphProxy]] = None,
        curve: Optional[Union[int, str]] = None,
        *,
        plot_curve: bool = False,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.call(
            "curvemath.integrate",
            graph=_name_of(graph),
            curve=curve,
            plot_curve=bool(plot_curve),
            **kwargs,
        )

    def differentiate_curve(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("curvemath.differentiate", **kwargs)

    def interpolate_curve(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("curvemath.interpolate", **kwargs)

    def subtract_line(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("curvemath.subtract_line", **kwargs)

    subtract_straight_line = subtract_line

    def intersections(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("curvemath.intersections", **kwargs)

    curve_intersections = intersections
    find_intersections = intersections

    def lookup_xy(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("curvemath.lookup_xy", **kwargs)

    def polygon_area(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("curvemath.polygon_area", **kwargs)

    def fill_between(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("curvemath.fill_between", **kwargs)

    def analyze_tensile(
        self,
        graph: Optional[Union[str, GraphProxy]] = None,
        curve: Optional[Union[int, str]] = None,
        *,
        table: Optional[Union[str, TableProxy]] = None,
        x_col: Optional[Union[int, str]] = None,
        y_col: Optional[Union[int, str]] = None,
        offset: float = 0.002,
        strain_in_percent: Optional[bool] = None,
        elastic_min: Optional[float] = None,
        elastic_max: Optional[float] = None,
        plot_construction: bool = True,
        add_callout: bool = True,
        create_true_stress_table: bool = False,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.call(
            "curvemath.offset_yield",
            graph=_name_of(graph),
            curve=curve,
            table=_name_of(table),
            x_col=x_col,
            y_col=y_col,
            offset=float(offset),
            strain_in_percent=strain_in_percent,
            elastic_min=elastic_min,
            elastic_max=elastic_max,
            plot_construction=bool(plot_construction),
            add_callout=bool(add_callout),
            create_true_stress_table=bool(create_true_stress_table),
            **kwargs,
        )

    offset_yield = analyze_tensile

    def arrhenius(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("curvemath.arrhenius", **kwargs)

    arrhenius_plot = arrhenius

    def tauc(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("curvemath.tauc", **kwargs)

    tauc_bandgap = tauc

    def eis_fit(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("curvemath.eis_fit", **kwargs)

    def normality(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("stats.normality", **kwargs)

    normality_test = normality

    def outlier(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("stats.outlier", **kwargs)

    outlier_test = outlier

    def anova_oneway(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("stats.anova_oneway", **kwargs)

    anova_one_way = anova_oneway

    def anova_twoway(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("stats.anova_twoway", **kwargs)

    anova_two_way = anova_twoway

    def nonparametric(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("stats.nonparametric", **kwargs)

    nonparametric_test = nonparametric

    def paired_plot(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("stats.paired_plot", **kwargs)

    paired_estimation_plot = paired_plot

    def bland_altman(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("stats.bland_altman", **kwargs)

    def passing_bablok(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("stats.passing_bablok", **kwargs)

    def weibull(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("stats.weibull", **kwargs)

    weibull_fit = weibull

    def kaplan_meier(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("stats.kaplan_meier", **kwargs)

    def roc(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("stats.roc", **kwargs)

    roc_analysis = roc
    roc_curve = roc

    def pca(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("stats.pca", **kwargs)

    def correlation(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("stats.correlation", **kwargs)

    correlation_matrix = correlation

    def clustering(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("stats.clustering", **kwargs)

    def confidence_ellipse(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("stats.confidence_ellipse", **kwargs)

    add_confidence_ellipse = confidence_ellipse

    def digitize(
        self, path: Union[str, os.PathLike[str]], **kwargs: Any
    ) -> Dict[str, Any]:
        return self.call("digitizer.extract", path=os.fspath(path), **kwargs)

    extract_digitizer = digitize
    digitize_image = digitize

    def merge_graphs(
        self,
        graphs: Sequence[Union[str, GraphProxy]],
        rows: int = 0,
        cols: int = 0,
        **kwargs: Any,
    ) -> GraphProxy:
        g_names = [_name_of(g) for g in graphs if g is not None]
        res = self.call(
            "graph.merge_graphs",
            graphs=g_names,
            rows=(int(rows) if rows and rows > 0 else None),
            cols=(int(cols) if cols and cols > 0 else None),
            **kwargs,
        )
        return GraphProxy(self, str(res["name"]))

    def grab_gui_diagnostic(
        self, path: Optional[Union[str, os.PathLike[str]]] = None, **kwargs: Any
    ) -> Dict[str, Any]:
        return self.call(
            "app.grab_gui_diagnostic",
            path=(os.fspath(path) if path is not None else ""),
            **kwargs,
        )


def _discover_torigraph_executable(
    exe_path: Optional[Union[str, os.PathLike[str]]] = None
) -> Path:
    if exe_path is not None:
        p = Path(exe_path).expanduser().resolve()
        if p.is_file():
            return p
        raise ToriGraphError(f"Specified ToriGraph executable does not exist: {p}")

    env_exe = os.environ.get("TORIGRAPH_EXE", "").strip()
    if env_exe:
        p = Path(env_exe).expanduser().resolve()
        if p.is_file():
            return p

    sdk_root = Path(__file__).resolve().parent.parent  # .../python
    repo_root = sdk_root.parent
    rel_candidates = [
        "build/scidavis/Release/ToriGraph.exe",
        "build/scidavis/ToriGraph.exe",
        "build/Release/ToriGraph.exe",
        "build/ToriGraph.exe",
        "build/scidavis/Release/scidavis.exe",
        "build/scidavis/scidavis.exe",
        "build/scidavis/ToriGraph",
        "build/scidavis/scidavis",
        "ToriGraph.exe",
    ]
    for rel in rel_candidates:
        cand = (repo_root / rel).resolve()
        if cand.is_file():
            return cand

    for bin_name in ("ToriGraph.exe", "ToriGraph", "scidavis.exe", "scidavis"):
        which = shutil.which(bin_name)
        if which:
            return Path(which).resolve()

    raise ToriGraphError(
        "Could not find ToriGraph executable. Pass exe_path=... or set TORIGRAPH_EXE."
    )


def connect(endpoint: Optional[str] = None, timeout: float = 30.0) -> Session:
    """Connect to a running ToriGraph JSON-RPC 2.0 server."""
    resolved = (endpoint or os.environ.get("TORIGRAPH_RPC_ENDPOINT", "")).strip()
    if not resolved:
        raise TransportError(
            "No ToriGraph RPC endpoint specified and TORIGRAPH_RPC_ENDPOINT is not set."
        )
    transport = _parse_and_connect_transport(resolved, timeout=timeout)
    session = Session(transport)
    session.ping()
    return session


def launch(
    exe_path: Optional[Union[str, os.PathLike[str]]] = None,
    headless: bool = True,
    language: str = "ja",
    timeout: float = 30.0,
    extra_args: Optional[Sequence[str]] = None,
) -> Session:
    """Launch a ToriGraph child process with loopback RPC enabled and connect a Session."""
    exe = _discover_torigraph_executable(exe_path)

    port_fd, port_file = tempfile.mkstemp(prefix="torigraph_rpc_port_", suffix=".txt")
    os.close(port_fd)
    try:
        os.remove(port_file)
    except OSError:
        pass

    err_fd, err_file = tempfile.mkstemp(prefix="torigraph_rpc_stderr_", suffix=".log")
    err_stream = os.fdopen(err_fd, "wb")

    cmd: List[str] = [os.fspath(exe)]
    if headless:
        cmd.append("--headless")
    cmd.extend(["--rpc-port", "0", "--rpc-port-file", port_file])
    if extra_args:
        cmd.extend(str(a) for a in extra_args)

    env = os.environ.copy()
    path_entries: List[str] = [os.fspath(exe.parent)]
    for anc in exe.parents:
        cand_deps = anc / ".deps" / "Library" / "bin"
        if cand_deps.is_dir():
            path_entries.append(os.fspath(cand_deps))
            break
    existing_path = env.get("PATH", "")
    if existing_path:
        path_entries.append(existing_path)
    env["PATH"] = os.pathsep.join(path_entries)

    try:
        proc = subprocess.Popen(
            cmd,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=err_stream,
            cwd=os.fspath(exe.parent),
            env=env,
        )
    except OSError as exc:
        err_stream.close()
        for f in (port_file, err_file):
            try:
                os.remove(f)
            except OSError:
                pass
        raise ToriGraphError(f"Failed to spawn '{exe}': {exc}") from exc
    finally:
        try:
            err_stream.close()
        except Exception:
            pass

    deadline = time.monotonic() + max(1.0, float(timeout))
    discovered_endpoint = ""
    while time.monotonic() <= deadline:
        if os.path.isfile(port_file):
            try:
                with open(port_file, "r", encoding="utf-8") as fh:
                    txt = fh.read().strip()
                if txt:
                    discovered_endpoint = txt.splitlines()[0].strip()
                    break
            except OSError:
                pass
        if proc.poll() is not None:
            stderr_tail = ""
            try:
                with open(err_file, "r", encoding="utf-8", errors="replace") as ef:
                    stderr_tail = ef.read().strip()
            except OSError:
                pass
            for f in (port_file, err_file):
                try:
                    os.remove(f)
                except OSError:
                    pass
            raise ToriGraphError(
                f"ToriGraph exited prematurely (code {proc.returncode}) before writing RPC port file."
                + (f"\nstderr:\n{stderr_tail}" if stderr_tail else "")
            )
        time.sleep(0.025)

    if not discovered_endpoint:
        try:
            proc.kill()
        except Exception:
            pass
        for f in (port_file, err_file):
            try:
                os.remove(f)
            except OSError:
                pass
        raise ToriGraphError(
            f"Timed out after {timeout:.1f}s waiting for ToriGraph RPC endpoint from '{exe}'."
        )

    try:
        transport = _parse_and_connect_transport(discovered_endpoint, timeout=timeout)
        session = Session(transport, process=proc, cleanup_files=[port_file, err_file])
        session.ping()
        if language:
            session.set_language(language)
        return session
    except Exception:
        try:
            proc.kill()
        except Exception:
            pass
        for f in (port_file, err_file):
            try:
                os.remove(f)
            except OSError:
                pass
        raise
