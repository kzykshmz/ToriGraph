"""Pure-Python 3.8+ JSON-RPC 2.0 client SDK for ToriGraph."""

from __future__ import annotations

import ctypes
import json
import math
import os
import posixpath
import queue
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
import warnings
import weakref
from pathlib import Path
from typing import Any, Callable, Dict, Iterable, List, Mapping, Optional, Sequence, Tuple, Union


class ToriGraphError(RuntimeError):
    """Base exception for ToriGraph SDK errors."""


class TransportError(ToriGraphError):
    """Raised when the underlying TCP or Named Pipe connection fails.

    Two subclasses tell the usual causes apart (both are still ``TransportError``):
    :class:`ToriGraphTimeoutError` (no answer in time -- ToriGraph may still be busy)
    and :class:`ToriGraphDisconnectedError` (connection lost, closed or invalidated).
    """


class ToriGraphTimeoutError(TransportError):
    """No answer arrived within ``rpc_timeout``.

    ToriGraph may still be working on the request (long fit, big import).  The
    session is invalidated so a late answer cannot be mistaken for the next one;
    call :meth:`Session.reconnect` to continue, or raise ``rpc_timeout=`` next time.
    """


class ToriGraphDisconnectedError(TransportError):
    """The connection was lost, closed by the peer, or the session was invalidated.

    Unlike :class:`ToriGraphTimeoutError` there is no pending work to wait for:
    call :meth:`Session.reconnect` (same endpoint) or connect / launch again.
    """


class BatchReadError(ToriGraphError):
    """A synchronous read (``get_*`` / ``info`` / ``to_dataframe`` ...) was attempted
    inside ``with app.batch():``, where calls are only recorded, not executed."""


class RpcError(ToriGraphError):
    """Raised when ToriGraph returns a JSON-RPC 2.0 error object.

    When the server lists valid choices (palette names, sample IDs, ...) they are in
    ``.available`` and the first 10 are also part of ``str(exc)``.
    """

    #: How many ``available`` candidates are spelled out in the exception message.
    MAX_SHOWN_CANDIDATES = 10

    def __init__(self, code: int, message: str, data: Any = None) -> None:
        available: Optional[List[str]] = (
            [str(x) for x in data["available"]]
            if isinstance(data, Mapping) and isinstance(data.get("available"), (list, tuple))
            else None
        )
        text = f"[RPC {code}] {message}"
        if available:
            shown = ", ".join(available[: self.MAX_SHOWN_CANDIDATES])
            more = len(available) - self.MAX_SHOWN_CANDIDATES
            text += f" (available: {shown}" + (f", ... +{more} more" if more > 0 else "") + ")"
        super().__init__(text)
        self.code = int(code)
        self.message = str(message)
        self.data = data
        self.available: Optional[List[str]] = available


ToriGraphRpcError = RpcError


class ToriGraphVersionWarning(UserWarning):
    """Emitted when the connected ToriGraph application and this SDK have different versions."""


#: Default time (seconds) to wait for the answer to one RPC call.  Separate from
#: the start-up / connect timeout of :func:`launch` and :func:`connect`; override
#: per session with ``rpc_timeout=`` or per call with ``Session.call(..., rpc_timeout=)``.
DEFAULT_RPC_TIMEOUT = 120.0


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


def _is_missing_scalar(value: Any) -> bool:
    """Detect None, NaN/Inf floats, and duck-typed pandas/numpy NA/NaT scalars without imports."""
    if value is None:
        return True
    if isinstance(value, float) and not math.isfinite(value):
        return True
    tname = type(value).__name__
    if tname in ("NAType", "NaTType"):
        return True
    tmod = getattr(type(value), "__module__", "")
    if tmod.startswith(("pandas", "numpy")) and str(value) in ("NaT", "<NA>"):
        return True
    return False


def _coerce_scalar(value: Any) -> Any:
    """Convert scalar values (including numpy scalars, NA/NaN/Inf, Path, dates) to JSON-safe Python types."""
    if value is None or isinstance(value, (bool, int, str)):
        return value
    if isinstance(value, float):
        return value if math.isfinite(value) else None
    if isinstance(value, os.PathLike):
        return os.fspath(value)
    if _is_missing_scalar(value):
        return None
    if hasattr(value, "item") and callable(value.item) and not isinstance(value, (bytes, bytearray)):
        try:
            # 0-D or scalar numpy/generic objects
            if not hasattr(value, "ndim") or int(getattr(value, "ndim", 0)) == 0:
                return _coerce_scalar(value.item())
        except Exception:
            pass
    if hasattr(value, "isoformat") and callable(value.isoformat):
        try:
            return value.isoformat()
        except Exception:
            pass
    return value


def _is_ndarray_like(obj: Any) -> bool:
    """Duck-typed check for numpy.ndarray (or compatible N-dimensional array) with zero imports."""
    if isinstance(obj, (str, bytes, bytearray, Mapping)):
        return False
    return (
        hasattr(obj, "ndim")
        and hasattr(obj, "shape")
        and (hasattr(obj, "tolist") or hasattr(obj, "__array_interface__"))
        and not hasattr(obj, "columns")
    )


def _is_polars_dataframe(obj: Any) -> bool:
    if isinstance(obj, Mapping):
        return False
    tmod = getattr(type(obj), "__module__", "")
    if tmod.startswith("polars") and hasattr(obj, "columns") and hasattr(obj, "to_dict"):
        return True
    return hasattr(obj, "columns") and hasattr(obj, "schema") and hasattr(obj, "to_dict") and hasattr(obj, "get_columns")


def _is_pandas_dataframe(obj: Any) -> bool:
    if isinstance(obj, Mapping) or _is_polars_dataframe(obj):
        return False
    return hasattr(obj, "columns") and hasattr(obj, "index") and hasattr(obj, "iloc")


def _is_dataframe_like(obj: Any) -> bool:
    return _is_pandas_dataframe(obj) or _is_polars_dataframe(obj)


def _is_1d_series_or_array(obj: Any) -> bool:
    if isinstance(obj, (str, bytes, bytearray, Mapping)) or _is_dataframe_like(obj):
        return False
    if hasattr(obj, "to_list") and callable(obj.to_list):
        return True
    if _is_ndarray_like(obj):
        return int(obj.ndim) == 1
    return False


def _extract_series_attrs(seq: Any) -> Dict[str, str]:
    """Extract optional 'name', 'units', 'long_name', 'comment' from a pandas/polars Series."""
    out: Dict[str, str] = {}
    sname = getattr(seq, "name", None)
    if isinstance(sname, str) and sname.strip():
        out["name"] = sname
    attrs = getattr(seq, "attrs", None)
    if isinstance(attrs, Mapping):
        for key in ("units", "unit", "long_name", "comment"):
            val = attrs.get(key)
            if isinstance(val, str) and val:
                out["units" if key == "unit" else key] = val
    return out


def _coerce_1d_sequence(seq: Any) -> List[Any]:
    """Convert any 1D iterable, numpy ndarray, pandas Series, or polars Series to a sanitized Python list."""
    if isinstance(seq, (str, bytes, bytearray)):
        raise TypeError(f"Expected 1D sequence of column values, got {type(seq).__name__}.")
    if hasattr(seq, "to_list") and callable(seq.to_list) and not _is_dataframe_like(seq):
        raw = seq.to_list()
    elif _is_ndarray_like(seq):
        ndim = int(seq.ndim)
        if ndim == 1:
            raw = seq.tolist()
        elif ndim == 2 and int(seq.shape[1]) == 1:
            raw = [row[0] for row in seq.tolist()]
        else:
            raise ValueError(f"Column values expect a 1D array, got shape {tuple(seq.shape)}.")
    elif hasattr(seq, "tolist") and callable(seq.tolist) and not _is_dataframe_like(seq):
        raw = seq.tolist()
    elif isinstance(seq, (list, tuple)):
        raw = seq
    else:
        raw = list(seq)
    return [_coerce_scalar(v) for v in raw]


def _coerce_tabular_input(
    data: Any,
    col_names: Optional[Sequence[str]] = None,
    *,
    include_index: Optional[bool] = None,
) -> Tuple[
    Dict[str, List[Any]],
    List[str],
    Dict[str, str],
    Dict[str, str],
    Dict[str, str],
    Dict[str, str],
]:
    """Coerce DataFrame / ndarray / Mapping into ``(cols_dict, col_order, units, long_names, comments, designations)``."""
    units: Dict[str, str] = {}
    long_names: Dict[str, str] = {}
    comments: Dict[str, str] = {}
    designations: Dict[str, str] = {}

    # 1. Polars DataFrame
    if _is_polars_dataframe(data):
        as_dict = data.to_dict(as_series=False)
        raw_cols = [str(c) for c in data.columns]
        cols_dict = {str(k): [_coerce_scalar(v) for v in vals] for k, vals in as_dict.items()}
        if col_names is not None:
            override = [str(c) for c in col_names]
            if len(override) == len(raw_cols) and set(override) != set(raw_cols):
                cols_dict = {override[i]: cols_dict[raw_cols[i]] for i in range(len(raw_cols))}
            ordered = override
        else:
            ordered = raw_cols
        return cols_dict, ordered, units, long_names, comments, designations

    # 2. Pandas DataFrame
    if _is_pandas_dataframe(data):
        cols_dict = {}
        ordered: List[str] = []
        idx = getattr(data, "index", None)
        idx_name = getattr(idx, "name", None)
        use_index = (
            bool(include_index)
            if include_index is not None
            else (idx is not None and idx_name is not None and bool(str(idx_name).strip()))
        )
        if use_index and idx is not None:
            iname = str(idx_name).strip() if (idx_name is not None and str(idx_name).strip()) else "Index"
            ordered.append(iname)
            cols_dict[iname] = _coerce_1d_sequence(idx)
            designations[iname] = "X"

        for col in data.columns:
            cname = str(col)
            ser = data[col]
            ordered.append(cname)
            cols_dict[cname] = _coerce_1d_sequence(ser)
            s_attrs = _extract_series_attrs(ser)
            if "units" in s_attrs:
                units[cname] = s_attrs["units"]
            if "long_name" in s_attrs:
                long_names[cname] = s_attrs["long_name"]
            if "comment" in s_attrs:
                comments[cname] = s_attrs["comment"]

        attrs = getattr(data, "attrs", None)
        if isinstance(attrs, Mapping):
            if isinstance(attrs.get("units"), Mapping):
                for k, v in attrs["units"].items():
                    if v is not None and str(v) != "":
                        units[str(k)] = str(v)
            if isinstance(attrs.get("long_names"), Mapping):
                for k, v in attrs["long_names"].items():
                    if v is not None and str(v) != "":
                        long_names[str(k)] = str(v)
            if isinstance(attrs.get("comments"), Mapping):
                for k, v in attrs["comments"].items():
                    if v is not None and str(v) != "":
                        comments[str(k)] = str(v)
            if isinstance(attrs.get("designations"), Mapping):
                for k, v in attrs["designations"].items():
                    if v is not None and str(v) != "":
                        designations[str(k)] = str(v)

        if col_names is not None:
            override = [str(c) for c in col_names]
            if len(override) == len(ordered) and set(override) != set(ordered):
                old_ordered = list(ordered)
                cols_dict = {override[i]: cols_dict[old_ordered[i]] for i in range(len(old_ordered))}
                units = {override[old_ordered.index(k)] if k in old_ordered else k: v for k, v in units.items()}
                long_names = {override[old_ordered.index(k)] if k in old_ordered else k: v for k, v in long_names.items()}
                comments = {override[old_ordered.index(k)] if k in old_ordered else k: v for k, v in comments.items()}
                designations = {override[old_ordered.index(k)] if k in old_ordered else k: v for k, v in designations.items()}
            ordered = override
        return cols_dict, ordered, units, long_names, comments, designations

    # 3. Mapping[str, Sequence | ndarray | Series]
    if isinstance(data, Mapping):
        cols_dict = {}
        for k, val in data.items():
            ck = str(k)
            cols_dict[ck] = _coerce_1d_sequence(val)
            s_attrs = _extract_series_attrs(val)
            if "units" in s_attrs:
                units[ck] = s_attrs["units"]
            if "long_name" in s_attrs:
                long_names[ck] = s_attrs["long_name"]
            if "comment" in s_attrs:
                comments[ck] = s_attrs["comment"]
        ordered = [str(c) for c in col_names] if col_names is not None else list(cols_dict.keys())
        return cols_dict, ordered, units, long_names, comments, designations

    # 4. NumPy ndarray (structured, 1D, or 2D)
    if _is_ndarray_like(data):
        dtype_names = getattr(getattr(data, "dtype", None), "names", None)
        if dtype_names:
            raw_fields = [str(fn) for fn in dtype_names]
            cols_dict = {fn: _coerce_1d_sequence(data[fn]) for fn in raw_fields}
            ordered = [str(c) for c in col_names] if col_names is not None else raw_fields
            return cols_dict, ordered, units, long_names, comments, designations

        ndim = int(data.ndim)
        if ndim == 1:
            cname = str(col_names[0]) if col_names else "Y"
            return {cname: _coerce_1d_sequence(data)}, [cname], units, long_names, comments, designations
        if ndim == 2:
            n_rows, n_cols = int(data.shape[0]), int(data.shape[1])
            raw_2d = data.tolist()
            if col_names is not None and len(col_names) == n_cols:
                names = [str(c) for c in col_names]
            elif n_cols == 1:
                names = ["Y"]
            elif n_cols == 2:
                names = ["X", "Y"]
            else:
                names = ["X"] + [f"Y{j}" for j in range(1, n_cols)]
            cols_dict = {
                names[j]: [_coerce_scalar(raw_2d[r][j]) for r in range(n_rows)]
                for j in range(n_cols)
            }
            if n_cols >= 2:
                designations[names[0]] = "X"
            return cols_dict, names, units, long_names, comments, designations
        raise ValueError(f"Tabular data expects a 1D or 2D array, got shape {tuple(data.shape)}.")

    raise TypeError(
        f"Unsupported tabular data type {type(data).__name__!r}; expected dict, DataFrame, or 1D/2D ndarray."
    )


def _coerce_2d_matrix(data: Any) -> List[List[Optional[float]]]:
    """Coerce a 2D nested sequence, numpy.ndarray, or DataFrame into row-major ``List[List[Optional[float]]]``."""
    def _to_cell(v: Any) -> Optional[float]:
        sv = _coerce_scalar(v)
        if sv is None:
            return None
        try:
            fv = float(sv)
            return fv if math.isfinite(fv) else None
        except (TypeError, ValueError) as exc:
            raise ValueError(f"Matrix cell must be numeric or None/NaN, got {v!r}.") from exc

    if _is_polars_dataframe(data):
        cols = [data[c].to_list() for c in data.columns]
        if not cols:
            return []
        n_r = len(cols[0])
        return [[_to_cell(cols[c][r]) for c in range(len(cols))] for r in range(n_r)]

    if _is_pandas_dataframe(data):
        raw_vals = data.to_numpy().tolist() if hasattr(data, "to_numpy") else [list(row) for _, row in data.iterrows()]
        return [[_to_cell(v) for v in row] for row in raw_vals]

    if _is_ndarray_like(data):
        ndim = int(data.ndim)
        if ndim != 2:
            raise ValueError(f"Matrix expects a 2D array, got shape {tuple(data.shape)}.")
        return [[_to_cell(v) for v in row] for row in data.tolist()]

    rows_list = list(data)
    return [[_to_cell(v) for v in row] for row in rows_list]


class BatchStepRef:
    """Lazy pipeline reference token resolving to ``$steps[i].<field>`` inside ``app.batch``."""

    def __init__(self, step_index: int, path: str = "") -> None:
        self._idx = int(step_index)
        self._path = str(path)

    def __getattr__(self, attr: str) -> "BatchStepRef":
        if attr.startswith("_"):
            raise AttributeError(attr)
        sub = f"{self._path}.{attr}" if self._path else attr
        return BatchStepRef(self._idx, sub)

    def __getitem__(self, key: Union[int, str]) -> "BatchStepRef":
        if isinstance(key, int):
            sub = f"{self._path}[{key}]"
        else:
            sub = f"{self._path}.{key}" if self._path else str(key)
        return BatchStepRef(self._idx, sub)

    def to_token(self) -> str:
        return f"$steps[{self._idx}].{self._path}" if self._path else f"$steps[{self._idx}]"

    def __str__(self) -> str:
        return self.to_token()

    def __repr__(self) -> str:
        return f"BatchStepRef({self.to_token()!r})"


_READ_ONLY_RPC_SUFFIXES = ("get_", "list_", "describe", "to_python")
_READ_ONLY_RPC_NAMES = frozenset(
    {
        "ping", "info", "inspect", "query", "supported_methods", "column_stats",
        "table.row_stats", "matrix.determinant", "app.check_updates",
        "app.info", "app.supported_methods", "app.describe_api", "app.list_windows", "app.window_info",
        "graph.inspect", "gadget.query", "workbook.list",
    }
)


def _coerce_designations_mapping(
    designations: Optional[Union[Sequence[str], Mapping[str, str]]],
    column_order: Sequence[str],
    total_cols: Optional[int] = None,
) -> Dict[str, str]:
    """Normalize dict or positional list/tuple designations into ``{col_name: role}``."""
    if designations is None:
        return {}
    if isinstance(designations, Mapping):
        return {str(k): str(v) for k, v in designations.items()}
    if isinstance(designations, (str, bytes)):
        raise TypeError("designations must be a mapping or a sequence of role strings, not a single string.")
    role_list = [str(d) for d in designations]
    cap = max(len(column_order), int(total_cols) if total_cols is not None else 0)
    if len(role_list) > cap:
        raise ValueError(
            f"designations has {len(role_list)} entries but the table has only {cap} columns."
        )
    return dict(zip(column_order, role_list))


def _is_read_only_rpc(method: str) -> bool:
    """True for RPC methods that only *return* data (so they cannot be batch-recorded)."""
    m = str(method)
    if m in _READ_ONLY_RPC_NAMES:
        return True
    tail = m.rsplit(".", 1)[-1]
    return tail in _READ_ONLY_RPC_NAMES or tail.startswith(_READ_ONLY_RPC_SUFFIXES)


class _BatchRecordedResult(dict):
    """Placeholder result dict returned by ``Session.call`` while recording inside ``with app.batch():``."""

    def __init__(self, step_index: int, method: str, params: Mapping[str, Any]) -> None:
        name_token = f"$steps[{step_index}].name"
        super().__init__(ok=True, name=name_token, _batch_step=step_index, _method=method)
        self._step_index = int(step_index)
        self._method = str(method)
        self._params = dict(params)
        if method.startswith("plot.") or method.startswith("graph."):
            super().__setitem__("graph", name_token)

    def __getitem__(self, key: str) -> Any:
        if dict.__contains__(self, key):
            return dict.__getitem__(self, key)
        return f"$steps[{self._step_index}].{key}"

    def get(self, key: str, default: Any = None) -> Any:
        if dict.__contains__(self, key):
            return dict.__getitem__(self, key)
        if key in ("name", "graph", "table", "matrix"):
            return f"$steps[{self._step_index}].{key}"
        return default


class BatchContext:
    """Context manager & builder for atomic single-round-trip ``app.batch`` pipelines."""

    def __init__(
        self,
        session: "Session",
        *,
        stop_on_error: bool = True,
        atomic: bool = True,
        defer_replot: bool = True,
        raise_on_error: bool = True,
    ) -> None:
        self.session = session
        self.stop_on_error = bool(stop_on_error)
        self.atomic = bool(atomic)
        self.defer_replot = bool(defer_replot)
        self.raise_on_error = bool(raise_on_error)
        self.commands: List[Dict[str, Any]] = []
        self.result: Optional[Dict[str, Any]] = None
        self.response: Optional[Dict[str, Any]] = None
        self.results: List[Any] = []
        self.ok: Optional[bool] = None
        self.rolled_back: bool = False
        self._prev_batch: Optional["BatchContext"] = None
        self._pending_proxies: List[Tuple[int, str, "weakref.ReferenceType[Any]"]] = []

    def _record_raw(self, method: str, params: Dict[str, Any]) -> _BatchRecordedResult:
        idx = len(self.commands)
        self.commands.append({"method": str(method), "params": params})
        return _BatchRecordedResult(idx, method, params)

    def _register_proxy(self, proxy: Any, token_name: str) -> None:
        if token_name.startswith("$steps[") and "]." in token_name:
            body = token_name[len("$steps[") :]
            idx_s, field = body.split("].", 1)
            if idx_s.isdigit():
                self._pending_proxies.append((int(idx_s), field, weakref.ref(proxy)))

    def add(
        self,
        method: str,
        params: Optional[Mapping[str, Any]] = None,
        /,
        **kwargs: Any,
    ) -> BatchStepRef:
        """Explicitly append a command to the batch and return a :class:`BatchStepRef`."""
        merged: Dict[str, Any] = {}
        if params is not None:
            merged.update(_sanitize_for_json(dict(params)))
        if kwargs:
            merged.update(_clean_kwargs(kwargs))
        idx = len(self.commands)
        self.commands.append({"method": str(method), "params": merged})
        return BatchStepRef(idx)

    call = add

    def execute(self) -> Dict[str, Any]:
        """Flush recorded commands to ToriGraph in a single ``app.batch`` RPC call."""
        if not self.commands:
            empty_res: Dict[str, Any] = {
                "ok": True,
                "completed": 0,
                "total": 0,
                "rolled_back": False,
                "results": [],
            }
            self.result = empty_res
            self.response = empty_res
            self.results = []
            self.ok = True
            self.rolled_back = False
            return empty_res

        prev = self.session._active_batch
        self.session._active_batch = None
        try:
            res = self.session.call(
                "app.batch",
                commands=list(self.commands),
                stop_on_error=self.stop_on_error,
                atomic=self.atomic,
                defer_replot=self.defer_replot,
            )
        finally:
            self.session._active_batch = prev

        self.result = res
        self.response = res
        step_results = res.get("results")
        self.results = list(step_results) if isinstance(step_results, list) else []
        self.ok = bool(res.get("ok", True))
        self.rolled_back = bool(res.get("rolled_back", False))

        # Patch recorded proxy .name attributes from server step results
        if self.ok and self.results:
            for step_idx, field, pref in self._pending_proxies:
                obj = pref()
                if obj is not None and 0 <= step_idx < len(self.results):
                    sres = self.results[step_idx]
                    if isinstance(sres, Mapping):
                        resolved = sres.get(field) or sres.get("name") or sres.get("graph")
                        if isinstance(resolved, str) and resolved:
                            obj.name = resolved

        if not self.ok and self.raise_on_error:
            err_info = res.get("error")
            if isinstance(err_info, Mapping):
                code = int(err_info.get("code", -32000))
                msg = str(err_info.get("message", "Batch step failed"))
            else:
                code = -32000
                msg = str(err_info or "Batch execution failed")
            raise RpcError(code, msg, data=res)

        return res

    def __enter__(self) -> "BatchContext":
        self._prev_batch = self.session._active_batch
        self.session._active_batch = self
        return self

    def __exit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None:
        self.session._active_batch = self._prev_batch
        self._prev_batch = None
        if exc_type is None and self.commands:
            self.execute()


def _sanitize_for_json(value: Any) -> Any:
    """Recursively convert NaN/Inf -> None, Path -> str, proxies -> name, and duck-typed arrays -> list."""
    if value is None or isinstance(value, (bool, int, str)):
        return value
    if isinstance(value, BatchStepRef):
        return value.to_token()
    if isinstance(value, float):
        return value if math.isfinite(value) else None
    if isinstance(value, os.PathLike):
        return os.fspath(value)
    if _is_missing_scalar(value):
        return None
    if isinstance(value, (TableProxy, MatrixProxy, GraphProxy)):
        return value.name
    if hasattr(value, "item") and callable(value.item) and not isinstance(value, (bytes, bytearray)):
        if not hasattr(value, "ndim") or int(getattr(value, "ndim", 0)) == 0:
            try:
                return _sanitize_for_json(value.item())
            except Exception:
                pass
    if hasattr(value, "isoformat") and callable(value.isoformat):
        try:
            return value.isoformat()
        except Exception:
            pass
    if isinstance(value, Mapping):
        return {str(k): _sanitize_for_json(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [_sanitize_for_json(v) for v in value]
    if hasattr(value, "to_list") and callable(value.to_list) and not _is_dataframe_like(value):
        return _sanitize_for_json(value.to_list())
    if hasattr(value, "tolist") and callable(value.tolist) and not _is_dataframe_like(value):
        return _sanitize_for_json(value.tolist())
    if isinstance(value, Iterable) and not isinstance(value, (bytes, bytearray)):
        return [_sanitize_for_json(v) for v in value]
    return value


def _clean_kwargs(kwargs: Dict[str, Any]) -> Dict[str, Any]:
    """Drop None-valued optional arguments and sanitize remaining values for JSON."""
    return {k: _sanitize_for_json(v) for k, v in kwargs.items() if v is not None}


def _name_of(obj: Union[str, "TableProxy", "MatrixProxy", "GraphProxy", BatchStepRef, None]) -> Optional[str]:
    if obj is None:
        return None
    if isinstance(obj, BatchStepRef):
        return obj.to_token()
    if isinstance(obj, str):
        return obj
    if hasattr(obj, "name"):
        return str(obj.name)
    return str(obj)


def _deprecated_alias(canonical: str, old: str) -> Callable[..., Any]:
    """Return a method ``old`` that warns (:class:`DeprecationWarning`) and forwards to ``canonical``.

    Older scripts keep working; new code should use the canonical name, which is the one
    listed in ``python/README.md`` and in ``app.describe_api()`` (``sdk_method``).
    The forward goes through ``getattr`` so a subclass override of ``canonical`` is honoured.
    """

    def _alias(self: Any, *args: Any, **kwargs: Any) -> Any:
        cls_name = type(self).__name__
        warnings.warn(
            f"{cls_name}.{old}() is a deprecated alias; use {cls_name}.{canonical}() instead.",
            DeprecationWarning,
            stacklevel=2,
        )
        return getattr(self, canonical)(*args, **kwargs)

    _alias.__name__ = old
    _alias.__qualname__ = old
    _alias.__doc__ = f"Deprecated alias of :meth:`{canonical}`."
    _alias._torigraph_alias_of = canonical  # type: ignore[attr-defined]
    return _alias


def _is_loopback_host(host: str) -> bool:
    h = str(host).strip().lower().strip("[]")
    return h in ("127.0.0.1", "localhost", "::1", "0:0:0:0:0:0:0:1") or h.startswith("127.")


def _local_tcp_token_candidate_paths(port: int) -> List[Path]:
    """Return candidate local filesystem paths for ToriGraph's ``tcp_<port>.token`` file."""
    fname = f"tcp_{int(port)}.token"
    candidates: List[Path] = []
    local_app_data = os.environ.get("LOCALAPPDATA", "").strip()
    if local_app_data:
        candidates.append(Path(local_app_data) / "ToriGraph" / "rpc" / fname)
    if os.name == "nt":
        candidates.append(Path.home() / "AppData" / "Local" / "ToriGraph" / "rpc" / fname)
    else:
        xdg = os.environ.get("XDG_DATA_HOME", "").strip()
        if xdg:
            candidates.append(Path(xdg) / "ToriGraph" / "rpc" / fname)
        candidates.append(Path.home() / ".local" / "share" / "ToriGraph" / "rpc" / fname)
        candidates.append(
            Path.home() / "Library" / "Application Support" / "ToriGraph" / "rpc" / fname
        )
    temp_dir = tempfile.gettempdir().strip()
    if temp_dir:
        candidates.append(Path(temp_dir) / "ToriGraph" / "rpc" / fname)
    seen: set[str] = set()
    unique: List[Path] = []
    for cand in candidates:
        key = os.path.normcase(os.fspath(cand))
        if key not in seen:
            seen.add(key)
            unique.append(cand)
    return unique


def _find_local_tcp_token_files(port: int) -> List[Path]:
    """Return existing local ``tcp_<port>.token`` files matching standard ToriGraph RPC dirs."""
    found: List[Path] = []
    for cand in _local_tcp_token_candidate_paths(port):
        try:
            if cand.is_file():
                found.append(cand)
        except OSError:
            continue
    return found


def _read_local_tcp_token_file(port: int) -> Optional[str]:
    for cand in _find_local_tcp_token_files(port):
        try:
            tok = cand.read_text(encoding="utf-8").strip()
            if tok:
                return tok
        except OSError:
            continue
    return None


def _resolve_tcp_auth_token(
    host: str,
    port: int,
    explicit_token: Optional[str] = None,
    query_token: Optional[str] = None,
) -> str:
    if explicit_token is not None and str(explicit_token).strip():
        return str(explicit_token).strip()
    if query_token is not None and str(query_token).strip():
        return str(query_token).strip()
    # Resolution order: explicit argument > endpoint query > TORIGRAPH_RPC_TOKEN > token file.
    # A stale `tcp_<port>.token` left behind by an earlier run must never override
    # a token the caller supplied on purpose.
    env_tok = os.environ.get("TORIGRAPH_RPC_TOKEN", "").strip()
    if env_tok:
        return env_tok
    if _is_loopback_host(host):
        file_tok = _read_local_tcp_token_file(port)
        if file_tok:
            return file_tok
    return ""


class _TcpTransport:
    """Newline-delimited UTF-8 JSON-RPC 2.0 transport over loopback TCP."""

    def __init__(
        self,
        host: str,
        port: int,
        timeout: float = 30.0,
        token: Optional[str] = None,
        *,
        _query_token: Optional[str] = None,
    ) -> None:
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

        auth_token = _resolve_tcp_auth_token(
            self.host, self.port, explicit_token=token, query_token=_query_token
        )
        auth_req = json.dumps(
            {
                "jsonrpc": "2.0",
                "id": 0,
                "method": "rpc.auth",
                "params": {"token": auth_token},
            },
            ensure_ascii=False,
        ).encode("utf-8")
        try:
            raw_auth_resp = self.request_line(auth_req, timeout=self.timeout)
            auth_resp = json.loads(raw_auth_resp.decode("utf-8"))
        except Exception:
            self.close()
            raise
        if isinstance(auth_resp, Mapping) and auth_resp.get("error") is not None:
            err = auth_resp["error"]
            err_code = int(err.get("code", -32001)) if isinstance(err, Mapping) else -32001
            # Legacy servers predating rpc.auth reply -32601 Method not found and stay open.
            if err_code != -32601:
                self.close()
                raw_msg = (
                    str(err.get("message", "Unauthorized: rpc.auth required"))
                    if isinstance(err, Mapping)
                    else str(err)
                )
                if err_code == -32001:
                    raw_msg = (
                        f"{raw_msg}. Recovery: pass token=..., set TORIGRAPH_RPC_TOKEN, "
                        f"verify the local tcp_{int(self.port)}.token file is readable, "
                        "or connect via a tokenless local pipe:// endpoint shown in Tools -> Python Console."
                    )
                err_data = err.get("data") if isinstance(err, Mapping) else None
                raise RpcError(err_code, raw_msg, err_data)

    @property
    def endpoint(self) -> str:
        return f"tcp://{self.host}:{self.port}"

    def _effective_timeout(self, timeout: Optional[float]) -> float:
        return max(0.05, float(self.timeout if timeout is None else timeout))

    def send_line(self, payload: bytes, timeout: Optional[float] = None) -> None:
        t = self._effective_timeout(timeout)
        try:
            self._sock.settimeout(t)
            self._sock.sendall(payload + b"\n")
        except socket.timeout as exc:
            self.close()
            raise TransportError(f"Timed out after {t:.1f}s sending to {self.endpoint}.") from exc
        except OSError as exc:
            self.close()
            raise TransportError(f"Socket I/O error on {self.endpoint}: {exc}") from exc

    def recv_line(self, timeout: Optional[float] = None) -> bytes:
        """Read the next non-empty response line.

        A timeout leaves the buffered socket reader unusable, so the transport is
        closed; the owning Session then refuses further calls instead of reading a
        late response as the answer to the next request.
        """
        t = self._effective_timeout(timeout)
        try:
            self._sock.settimeout(t)
            while True:
                line = self._reader.readline()
                if not line:
                    raise TransportError(f"Connection closed by ToriGraph ({self.endpoint}).")
                stripped = line.strip()
                if stripped:
                    return stripped
        except socket.timeout as exc:
            self.close()
            raise TransportError(
                f"Timed out after {t:.1f}s waiting for a response from {self.endpoint}."
            ) from exc
        except OSError as exc:
            self.close()
            raise TransportError(f"Socket I/O error on {self.endpoint}: {exc}") from exc

    def request_line(self, payload: bytes, timeout: Optional[float] = None) -> bytes:
        self.send_line(payload, timeout)
        return self.recv_line(timeout)

    def close(self) -> None:
        try:
            self._reader.close()
        except Exception:
            pass
        try:
            self._sock.close()
        except Exception:
            pass


def _unix_local_socket_candidates(pipe_name: str) -> List[str]:
    """Socket paths to try for a Unix ``pipe://<name>`` endpoint, most likely first.

    ``QLocalServer::listen("<name>")`` creates the socket under ``QDir::tempPath()``, which is
    ``$TMPDIR`` when it is set (macOS always sets it) and ``/tmp`` otherwise.  An absolute name
    (containing ``/``) is used as is.
    """
    if "/" in pipe_name:
        return [pipe_name]
    candidates = [posixpath.join(tempfile.gettempdir(), pipe_name)]
    legacy = f"/tmp/{pipe_name}"
    if posixpath.normpath(legacy) != posixpath.normpath(candidates[0]):
        candidates.append(legacy)
    return candidates


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
            candidates = _unix_local_socket_candidates(pipe_name)
            errors: List[str] = []
            last_exc: Optional[OSError] = None
            for path in candidates:
                sock: Optional[socket.socket] = None
                try:
                    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                    sock.settimeout(self.timeout)
                    sock.connect(path)
                    self._unix_sock = sock
                    break
                except OSError as exc:
                    if sock is not None:
                        try:
                            sock.close()
                        except OSError:
                            pass
                    errors.append(f"'{path}': {exc}")
                    last_exc = exc
            if self._unix_sock is None:
                raise TransportError(
                    "Failed to connect Unix domain socket " + "; ".join(errors)
                ) from last_exc

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
                f"Named pipe {op_name} timed out after {timeout_sec:.2f}s ({self._endpoint})."
            ) from exc
        if ok:
            return payload
        raise payload

    def _effective_timeout(self, timeout: Optional[float]) -> float:
        return max(0.05, float(self.timeout if timeout is None else timeout))

    def send_line(self, payload: bytes, timeout: Optional[float] = None) -> None:
        t = self._effective_timeout(timeout)
        msg = payload + b"\n"
        try:
            if self._win_handle is not None:
                fh = self._win_handle
                self._run_win_op(lambda: (fh.write(msg), fh.flush()), "write", t)
                return
            if self._unix_sock is not None:
                self._unix_sock.settimeout(t)
                self._unix_sock.sendall(msg)
                return
            raise TransportError("Transport is closed.")
        except TransportError:
            raise
        except OSError as exc:
            self.close()
            raise TransportError(f"Pipe I/O error on {self._endpoint}: {exc}") from exc

    def request_line(self, payload: bytes, timeout: Optional[float] = None) -> bytes:
        self.send_line(payload, timeout)
        return self.recv_line(timeout)

    def recv_line(self, timeout: Optional[float] = None) -> bytes:
        """Read the next non-empty response line within ``timeout`` seconds.

        On timeout the pipe is closed (a late response must never be read as the
        answer to a later request).
        """
        t = self._effective_timeout(timeout)
        deadline = time.monotonic() + t
        try:
            if self._win_handle is not None:
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
                            f"Timed out after {t:.1f}s waiting for a response from {self._endpoint}."
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
                    chunk = self._run_win_op(lambda fh=fh_cur, n=to_read: fh.read(n), "read", rem)
                    if not chunk:
                        raise TransportError(f"Named pipe closed ({self._endpoint}).")
                    self._buf.extend(chunk)
            elif self._unix_sock is not None:
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
                            f"Timed out after {t:.1f}s waiting for a response from {self._endpoint}."
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
            self.close()
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
    endpoint: str, timeout: float = 30.0, token: Optional[str] = None
) -> Union[_TcpTransport, _LocalPipeTransport]:
    ep = str(endpoint).strip()
    if not ep:
        raise TransportError("Empty ToriGraph RPC endpoint.")
    query_token: Optional[str] = None
    if "?" in ep:
        from urllib.parse import parse_qs

        ep_base, query_str = ep.split("?", 1)
        ep = ep_base.rstrip("/")
        qmap = parse_qs(query_str, keep_blank_values=False)
        tok_vals = qmap.get("token")
        if tok_vals and tok_vals[0].strip():
            query_token = tok_vals[0].strip()

    if ep.startswith("tcp://"):
        host_port = ep[len("tcp://") :]
        host, sep, port_s = host_port.rpartition(":")
        if not sep or not port_s.isdigit():
            raise TransportError(f"Invalid TCP endpoint '{endpoint}'.")
        return _TcpTransport(
            host or "127.0.0.1",
            int(port_s),
            timeout=timeout,
            token=token,
            _query_token=query_token,
        )
    if ep.startswith("pipe://"):
        return _LocalPipeTransport(ep[len("pipe://") :], timeout=timeout)
    if ep.startswith("\\\\.\\pipe\\"):
        return _LocalPipeTransport(ep, timeout=timeout)
    if ep.isdigit():
        return _TcpTransport(
            "127.0.0.1", int(ep), timeout=timeout, token=token, _query_token=query_token
        )
    if ":" in ep:
        host, _, port_s = ep.rpartition(":")
        if port_s.isdigit():
            return _TcpTransport(
                host or "127.0.0.1",
                int(port_s),
                timeout=timeout,
                token=token,
                _query_token=query_token,
            )
    return _LocalPipeTransport(ep, timeout=timeout)


class _NamespaceProxy:
    """Dynamic dotted namespace forwarding e.g. `app.analysis.fit_nonlinear(...)`."""

    def __init__(self, session: "Session", prefix: str) -> None:
        self._session = session
        self._prefix = prefix

    def __getattr__(self, method_suffix: str) -> Any:
        if not method_suffix or method_suffix.startswith("_"):
            raise AttributeError(f"{type(self).__name__} has no attribute {method_suffix!r}")
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


class _WorkbookDomain(_NamespaceProxy):
    """Callable `app.workbook(name) -> WorkbookProxy` AND dotted `app.workbook.create(...)`."""

    def __init__(self, session: "Session") -> None:
        super().__init__(session, "workbook")

    def __call__(self, name: str) -> "WorkbookProxy":
        return WorkbookProxy(self._session, name)


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
        if getattr(session, "_active_batch", None) is not None and self.name.startswith("$steps["):
            session._active_batch._register_proxy(self, self.name)  # type: ignore[union-attr]

    def __repr__(self) -> str:
        return f"TableProxy(name={self.name!r})"

    def info(self) -> Dict[str, Any]:
        return self.session.call("table.get_info", table=self.name)

    get_info = _deprecated_alias("info", "get_info")

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
        coerced_data: Optional[List[Any]] = None
        if data is not None:
            s_attrs = _extract_series_attrs(data)
            if name is None and "name" in s_attrs:
                name = s_attrs["name"]
            for mkey in ("units", "long_name", "comment"):
                if mkey in s_attrs and meta_kwargs.get(mkey) is None:
                    meta_kwargs[mkey] = s_attrs[mkey]
            coerced_data = _coerce_1d_sequence(data)
        return self.session.call(
            "table.add_column",
            table=self.name,
            name=name,
            plot_designation=plot_designation,
            mode=mode,
            data=coerced_data,
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
        s_attrs = _extract_series_attrs(seq)
        for mkey in ("units", "long_name", "comment"):
            if mkey in s_attrs and meta_kwargs.get(mkey) is None:
                meta_kwargs[mkey] = s_attrs[mkey]
        res = self.session.call(
            "table.set_column_data",
            table=self.name,
            col=col,
            data=_coerce_1d_sequence(seq),
            mode=mode,
            **meta_kwargs,
        )
        return res

    set_column_data = _deprecated_alias("set_column", "set_column_data")
    set_col = _deprecated_alias("set_column", "set_col")

    def get_column(
        self,
        col: Union[int, str],
        *,
        compact: bool = False,
        include_masked: bool = True,
        start_row: Optional[int] = None,
        end_row: Optional[int] = None,
    ) -> List[Any]:
        eff_inc_mask: Optional[bool] = (
            False if not include_masked else (None if compact else True)
        )
        res = self.session.call(
            "table.get_column_data",
            table=self.name,
            col=col,
            compact=(True if compact else None),
            include_masked=eff_inc_mask,
            start_row=start_row,
            end_row=end_row,
        )
        return list(res.get("data", res.get("values", [])))

    get_column_data = _deprecated_alias("get_column", "get_column_data")
    get_col = _deprecated_alias("get_column", "get_col")

    def set_data_dict(
        self,
        data: Any,
        meta: Optional[Mapping[str, Mapping[str, Any]]] = None,
        units: Optional[Mapping[str, str]] = None,
        long_names: Optional[Mapping[str, str]] = None,
        comments: Optional[Mapping[str, str]] = None,
        designations: Optional[Union[Sequence[str], Mapping[str, str]]] = None,
        column_order: Optional[Sequence[str]] = None,
        *,
        columns: Optional[Sequence[str]] = None,
        include_index: Optional[bool] = None,
    ) -> Dict[str, Any]:
        """Write ``{column_name: values}`` into this worksheet.

        A key that names an existing column overwrites that column; the other keys are added as
        new columns at the end. When no key names an existing column (e.g. a new table with the
        default column names), the columns are written from the left and renamed to the keys.
        """
        req_order = column_order if column_order is not None else columns
        (
            payload_data,
            ordered,
            inf_units,
            inf_long_names,
            inf_comments,
            inf_designations,
        ) = _coerce_tabular_input(data, col_names=req_order, include_index=include_index)

        merged_units = {**inf_units, **(dict(units) if units is not None else {})} or None
        merged_long_names = {**inf_long_names, **(dict(long_names) if long_names is not None else {})} or None
        merged_comments = {**inf_comments, **(dict(comments) if comments is not None else {})} or None
        explicit_desig = _coerce_designations_mapping(designations, ordered)
        merged_designations = {**inf_designations, **explicit_desig} or None

        return self.session.call(
            "table.set_data_dict",
            table=self.name,
            data=payload_data,
            column_order=ordered,
            meta=meta,
            units=merged_units,
            long_names=merged_long_names,
            comments=merged_comments,
            designations=merged_designations,
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

    to_dict = _deprecated_alias("get_data_dict", "to_dict")

    def to_dataframe(
        self,
        backend: str = "auto",
        *,
        columns: Optional[Sequence[Union[int, str]]] = None,
    ) -> Any:
        """Export worksheet columns to a ``pandas.DataFrame``, ``polars.DataFrame``, ``np.ndarray``, or ``dict``.

        When returning a ``pandas.DataFrame``, ``df.attrs['units']`` and ``df.attrs['long_names']``
        are populated so round-tripping back via ``create_table(df)`` / ``set_data_dict(df)`` is lossless.
        """
        b = str(backend).strip().lower()
        col_dict = self.get_data_dict(columns=columns)
        if b == "dict":
            return col_dict
        if b == "numpy":
            return self.to_numpy(columns=columns)

        info = self.info()
        cols_meta = info.get("columns", [])
        units_map = {
            str(c["name"]): str(c["units"])
            for c in cols_meta
            if "name" in c and c.get("units")
            and (columns is None or str(c["name"]) in col_dict)
        }
        long_names_map = {
            str(c["name"]): str(c["long_name"])
            for c in cols_meta
            if "name" in c and c.get("long_name")
            and (columns is None or str(c["name"]) in col_dict)
        }

        if b in ("auto", "pandas", "pd"):
            try:
                import pandas as pd  # type: ignore[import-not-found]

                df = pd.DataFrame(col_dict)
                df.attrs["units"] = units_map
                df.attrs["long_names"] = long_names_map
                df.attrs["table_name"] = self.name
                return df
            except ImportError:
                if b in ("pandas", "pd"):
                    raise

        if b in ("auto", "polars", "pl"):
            try:
                import polars as pl  # type: ignore[import-not-found]

                return pl.DataFrame(col_dict)
            except ImportError:
                if b in ("polars", "pl"):
                    raise

        raise ImportError(
            f"Requested DataFrame backend {backend!r} is not installed. "
            "Install 'pandas' or 'polars', or call tbl.get_data_dict()."
        )

    def to_numpy(
        self,
        columns: Optional[Sequence[Union[int, str]]] = None,
        *,
        dtype: Any = float,
    ) -> Any:
        """Export numeric worksheet columns as a 2D ``numpy.ndarray`` (converting invalid cells to ``np.nan``)."""
        import numpy as np  # type: ignore[import-not-found]

        col_dict = self.get_data_dict(columns=columns)
        if not col_dict:
            return np.empty((0, 0), dtype=dtype)
        arr_cols = [
            [(np.nan if v is None else v) for v in col_vals]
            for col_vals in col_dict.values()
        ]
        return np.column_stack(arr_cols).astype(dtype, copy=False)

    def set_cell(self, row: int, col: Union[int, str], value: Any) -> Dict[str, Any]:
        return self.session.call(
            "table.set_cell", table=self.name, row=int(row), col=col, value=_coerce_scalar(value)
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
        mode: Optional[str] = None,
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
            mode=mode,
            numeric_format=numeric_format,
            precision=precision,
        )

    set_column_meta = _deprecated_alias("set_meta", "set_column_meta")

    def get_meta(self, col: Union[int, str]) -> Dict[str, Any]:
        return self.session.call("table.get_column_meta", table=self.name, col=col)

    get_column_meta = _deprecated_alias("get_meta", "get_column_meta")

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

    def recalculate(self, col: Optional[Union[int, str]] = None) -> Dict[str, Any]:
        return self.session.call("table.recalculate", table=self.name, col=col)

    def mask_rows(
        self,
        rows: Optional[Union[int, Sequence[int]]] = None,
        col: Optional[Union[int, str]] = None,
        *,
        start_row: Optional[int] = None,
        end_row: Optional[int] = None,
    ) -> Dict[str, Any]:
        row_list = (
            None
            if rows is None
            else ([int(rows)] if isinstance(rows, int) else [int(r) for r in rows])
        )
        return self.session.call(
            "table.mask_rows",
            table=self.name,
            rows=row_list,
            col=col,
            start_row=start_row,
            end_row=end_row,
        )

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

    column_stats = _deprecated_alias("stats", "column_stats")

    def row_stats(
        self,
        rows: Optional[Sequence[int]] = None,
        *,
        start_row: Optional[int] = None,
        end_row: Optional[int] = None,
    ) -> Dict[str, Any]:
        return self.session.call(
            "table.row_stats",
            table=self.name,
            rows=([int(r) for r in rows] if rows is not None else None),
            start_row=start_row,
            end_row=end_row,
        )

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


class WorkbookProxy:
    """A workbook: several worksheets (tables) shown as sheet tabs of one window.

    Every sheet is an ordinary worksheet; ``sheets()`` returns them as TableProxy objects
    and graphs / formulas keep referring to the table names.
    """

    def __init__(self, session: "Session", name: str) -> None:
        self.session = session
        self.name = str(name)

    def __repr__(self) -> str:
        return f"WorkbookProxy(name={self.name!r})"

    def info(self) -> Dict[str, Any]:
        for wb in self.session.list_workbooks():
            if wb.get("name") == self.name:
                return wb
        raise ToriGraphError(f"Workbook {self.name!r} not found")

    def sheets(self) -> List["TableProxy"]:
        return [TableProxy(self.session, str(s["table"])) for s in self.info().get("sheets", [])]

    def add_sheet(
        self, name: Optional[str] = None, *, index: int = -1, activate: bool = True
    ) -> "TableProxy":
        res = self.session.call(
            "workbook.add_sheet", book=self.name, name=(name or ""), index=int(index),
            activate=bool(activate),
        )
        return TableProxy(self.session, str(res["table"]))

    def attach(self, table: Union[str, "TableProxy"], *, index: int = -1) -> Dict[str, Any]:
        return self.session.call(
            "workbook.attach_sheet", book=self.name, table=_name_of(table), index=int(index)
        )

    def set_current(self, table: Union[str, "TableProxy"]) -> Dict[str, Any]:
        return self.session.call("workbook.set_current", table=_name_of(table))

    def move_sheet(self, table: Union[str, "TableProxy"], index: int) -> Dict[str, Any]:
        return self.session.call("workbook.move_sheet", table=_name_of(table), index=int(index))

    def rename_sheet(self, table: Union[str, "TableProxy"], name: str) -> "TableProxy":
        res = self.session.call("workbook.rename_sheet", table=_name_of(table), name=str(name))
        return TableProxy(self.session, str(res["table"]))

    def rename(self, new_name: str) -> "WorkbookProxy":
        res = self.session.call("workbook.rename", book=self.name, new_name=str(new_name))
        self.name = str(res.get("name", new_name))
        return self

    def ungroup(self) -> Dict[str, Any]:
        return self.session.call("workbook.ungroup", book=self.name)


class MatrixProxy:
    """High-level proxy for a ToriGraph 2D Matrix window."""

    def __init__(self, session: "Session", name: str) -> None:
        self.session = session
        self.name = str(name)
        if getattr(session, "_active_batch", None) is not None and self.name.startswith("$steps["):
            session._active_batch._register_proxy(self, self.name)  # type: ignore[union-attr]

    def __repr__(self) -> str:
        return f"MatrixProxy(name={self.name!r})"

    def info(self) -> Dict[str, Any]:
        return self.session.call("matrix.get_info", matrix=self.name)

    get_info = _deprecated_alias("info", "get_info")

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

    def set_data(self, data: Any = None, *, rows_2d: Any = None) -> Dict[str, Any]:
        src = rows_2d if rows_2d is not None else data
        if src is None:
            raise ValueError("Provide 2D matrix 'data' (or 'rows_2d').")
        coerced = _coerce_2d_matrix(src)
        return self.session.call("matrix.set_data", matrix=self.name, data=coerced)

    def get_data(self) -> List[List[Optional[float]]]:
        res = self.session.call("matrix.get_data", matrix=self.name)
        return list(res.get("data", []))

    def to_numpy(self, *, dtype: Any = float) -> Any:
        """Return matrix cells as a 2D ``numpy.ndarray`` (converting ``None`` cells to ``np.nan``)."""
        import numpy as np  # type: ignore[import-not-found]

        grid = self.get_data()
        if not grid:
            return np.empty((0, 0), dtype=dtype)
        clean = [[(np.nan if v is None else v) for v in row] for row in grid]
        return np.asarray(clean, dtype=dtype)

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

    find_2d_peaks = _deprecated_alias("analyze_2d_peaks", "find_2d_peaks")

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
        if getattr(session, "_active_batch", None) is not None and self.name.startswith("$steps["):
            session._active_batch._register_proxy(self, self.name)  # type: ignore[union-attr]

    def __repr__(self) -> str:
        return f"GraphProxy(name={self.name!r})"

    def call(
        self,
        _method: Optional[str] = None,
        _params: Optional[Mapping[str, Any]] = None,
        /,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Invoke a JSON-RPC method defaulting ``graph=self.name`` when not already supplied."""
        has_graph = ("graph" in kwargs and kwargs["graph"] is not None) or (
            _params is not None and "graph" in _params and _params["graph"] is not None
        )
        if not has_graph:
            kwargs["graph"] = self.name
        return self.session.call(_method, _params, **kwargs)

    def info(self) -> Dict[str, Any]:
        return self.session.call("graph.get_info", graph=self.name)

    get_info = _deprecated_alias("info", "get_info")

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

    def auto_layout(
        self,
        *,
        pad_fraction: float = 0.05,
        allow_outside_legend: bool = True,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Deterministically resolve axis, clipping, panel, tick-collision, and legend/callout overlaps."""
        return self.session.call(
            "graph.auto_layout",
            graph=self.name,
            pad_fraction=float(pad_fraction),
            allow_outside_legend=bool(allow_outside_legend),
            **kwargs,
        )

    fix_layout = _deprecated_alias("auto_layout", "fix_layout")

    def set_title(
        self,
        title: str = "",
        *,
        font_size: Optional[float] = None,
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
        table: Union[str, TableProxy, Any],
        y_col: Union[int, str],
        x_col: Optional[Union[int, str]] = None,
        style: str = "line_symbol",
        right_axis: bool = False,
        layer: Optional[int] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self.session._ensure_table_ref(table)
        return self.session.call(
            "plot.add_curve",
            graph=self.name,
            table=tbl_ref,
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
        table_or_err_col: Optional[Union[str, int, TableProxy]] = None,
        curve: Optional[Union[int, str]] = None,
        err_col: Optional[Union[int, str]] = None,
        direction: str = "y",
        *,
        table: Optional[Union[str, TableProxy]] = None,
        layer: Optional[int] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        if err_col is not None:
            resolved_table = _name_of(table_or_err_col) if table_or_err_col is not None else _name_of(table)  # type: ignore[arg-type]
            resolved_curve = curve
            resolved_err: Union[int, str] = err_col
        else:
            if table_or_err_col is None or isinstance(table_or_err_col, TableProxy):
                raise ValueError("Specify 'err_col' column name or index.")
            resolved_err = table_or_err_col
            resolved_curve = curve
            resolved_table = _name_of(table)
        return self.session.call(
            "plot.add_error_bars",
            graph=self.name,
            table=resolved_table,
            curve=resolved_curve,
            err_col=resolved_err,
            direction=direction,
            layer=layer,
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
        x_min: Optional[float] = None,
        x_max: Optional[float] = None,
        y_min: Optional[float] = None,
        y_max: Optional[float] = None,
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

    add_inset_layer = _deprecated_alias("add_inset", "add_inset_layer")

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

    def fit_page_to_layers(self, margin_px: Optional[int] = None) -> Dict[str, Any]:
        return self.session.call(
            "graph.fit_page_to_layers", graph=self.name, margin_px=margin_px
        )

    def fit_layers_to_page(self) -> Dict[str, Any]:
        return self.session.call("graph.fit_layers_to_page", graph=self.name)

    def apply_style_preset(self, preset: str) -> Dict[str, Any]:
        return self.session.call(
            "graph.apply_style_preset", graph=self.name, preset=preset
        )

    def isij_style(self) -> Dict[str, Any]:
        """Apply the ISIJ paper preset (``isij_85mm``: 85 mm single-column figure)."""
        return self.apply_style_preset("isij_85mm")

    def lab_slide_style(self) -> Dict[str, Any]:
        """Apply the lab slide preset (``lab_slide_83mm``): 83.3 mm wide, 12.9 pt text.

        This is the recommended preset for slides.  ``to_pptx()`` / ``export_graphs_to_pptx()``
        place a graph carrying it at 140 % so the text ends up as 18 pt on the slide.
        """
        return self.apply_style_preset("lab_slide_83mm")

    def slide_style(self) -> Dict[str, Any]:
        """Apply the legacy large-slide preset (``slide_28pt``: 185 mm wide, 28 pt text).

        Prefer :meth:`lab_slide_style` (83.3 mm / 12.9 pt, pasted at 140 % -> 18 pt) for the
        lab's slide template; use this one only when a full-width 28 pt figure is wanted.
        """
        return self.apply_style_preset("slide_28pt")

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
        *,
        reset: Optional[bool] = None,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.sync_axis_color",
            graph=self.name,
            axis=axis,
            curve=curve,
            reset=reset,
            layer=layer,
        )

    def step_zoom_history(
        self,
        delta: Union[int, str] = -1,
        layer: Optional[int] = None,
        *,
        direction: Optional[str] = None,
    ) -> Dict[str, Any]:
        if direction is not None:
            resolved_delta = 1 if str(direction).strip().lower() in ("forward", "next", "redo", "+1") else -1
            return self.session.call(
                "graph.step_zoom_history",
                graph=self.name,
                delta=resolved_delta,
                direction=direction,
                layer=layer,
            )
        if isinstance(delta, str):
            resolved_delta = 1 if delta.strip().lower() in ("forward", "next", "redo", "+1") else -1
            return self.session.call(
                "graph.step_zoom_history",
                graph=self.name,
                delta=resolved_delta,
                direction=delta,
                layer=layer,
            )
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
        enabled: Union[bool, float] = True,
        ratio: float = 1.0,
        *,
        keep: Optional[bool] = None,
    ) -> Dict[str, Any]:
        if isinstance(enabled, (int, float)) and not isinstance(enabled, bool):
            eff_ratio = float(enabled)
            use_keep = True if keep is None else bool(keep)
        else:
            eff_ratio = float(ratio)
            use_keep = bool(enabled) if keep is None else bool(keep)
        return self.session.call(
            "graph.set_aspect_ratio", graph=self.name, keep=use_keep, ratio=eff_ratio
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
        categories: Optional[Sequence[str]] = None,
        font_family: Optional[str] = None,
        font_size: Optional[float] = None,
        title_font_size: Optional[float] = None,
        numbers_font_size: Optional[float] = None,
        tick_font_size: Optional[float] = None,
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
            categories=([str(c) for c in _coerce_1d_sequence(categories)] if categories is not None else None),
            font_family=font_family,
            font_size=font_size,
            title_font_size=title_font_size,
            numbers_font_size=numbers_font_size,
            tick_font_size=tick_font_size,
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

    def set_category_labels(
        self,
        axis_or_labels: Optional[Union[str, Sequence[Any]]] = None,
        labels_arg: Optional[Sequence[Any]] = None,
        *,
        axis: Optional[str] = None,
        labels: Optional[Sequence[Any]] = None,
        layer: Optional[int] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Assign explicit text category labels on an axis (`('x', labels)` or `(labels, axis='x')`)."""
        resolved_axis = "x"
        resolved_labels: Optional[Sequence[Any]] = labels

        if isinstance(axis_or_labels, str):
            resolved_axis = axis_or_labels
            if labels_arg is not None:
                resolved_labels = labels_arg
        elif axis_or_labels is not None:
            resolved_labels = axis_or_labels
            if isinstance(labels_arg, str):
                resolved_axis = labels_arg

        if axis is not None:
            resolved_axis = axis
        if resolved_labels is None:
            raise ValueError("Provide category 'labels' sequence.")

        str_labels = ["" if v is None else str(v) for v in _coerce_1d_sequence(resolved_labels)]
        return self.session.call(
            "graph.set_category_labels",
            graph=self.name,
            axis=resolved_axis,
            labels=str_labels,
            layer=layer,
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
        log_before: Optional[bool] = None,
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
            log_before=log_before,
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

    def add_shaded_band(
        self,
        x: Optional[Sequence[float]] = None,
        y_low: Optional[Sequence[float]] = None,
        y_high: Optional[Sequence[float]] = None,
        *,
        label: Optional[str] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.add_shaded_band",
            graph=self.name,
            x=(_coerce_1d_sequence(x) if x is not None else None),
            y_low=(_coerce_1d_sequence(y_low) if y_low is not None else None),
            y_high=(_coerce_1d_sequence(y_high) if y_high is not None else None),
            label=label,
            **kwargs,
        )

    def add_error_band(
        self,
        curve: Union[int, str],
        err: Union[int, str],
        *,
        err_minus: Optional[Union[int, str]] = None,
        table: Optional[str] = None,
        alpha: Optional[float] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Shade Y - err ... Y + err of a data curve (asymmetric with ``err_minus``)."""
        return self.session.call(
            "graph.add_error_band",
            graph=self.name,
            curve=curve,
            err=err,
            err_minus=err_minus,
            table=table,
            alpha=alpha,
            **kwargs,
        )

    def rebuild_density2d(self, **kwargs: Any) -> Dict[str, Any]:
        """Re-count a 2D density graph from its recorded source columns and bin settings."""
        return self.session.call("graph.rebuild_density2d", graph=self.name, **kwargs)

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
        symbol_style: Optional[str] = None,
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
        eff_sym = symbol_type if symbol_type is not None else symbol_style
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
            symbol_type=eff_sym,
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
        from_index_or_curve: Optional[Union[int, str]] = None,
        to_index_or_direction: Union[int, str] = "up",
        layer: Optional[int] = None,
        *,
        curve: Optional[Union[int, str]] = None,
        direction: Optional[str] = None,
        from_index: Optional[int] = None,
        to_index: Optional[int] = None,
    ) -> Dict[str, Any]:
        target = (
            curve
            if curve is not None
            else (from_index if from_index is not None else from_index_or_curve)
        )
        if target is None:
            raise ValueError("Provide 'curve' (or 'from_index') to move.")
        if to_index is not None or (
            direction is None
            and isinstance(to_index_or_direction, int)
            and not isinstance(to_index_or_direction, bool)
            and isinstance(target, int)
            and not isinstance(target, bool)
        ):
            dest_idx = int(to_index if to_index is not None else to_index_or_direction)
            return self.session.call(
                "graph.move_curve",
                graph=self.name,
                layer=layer,
                from_index=int(target),
                to_index=dest_idx,
            )
        eff_dir = direction if direction is not None else str(to_index_or_direction)
        return self.session.call(
            "graph.move_curve",
            graph=self.name,
            layer=layer,
            curve=target,
            direction=eff_dir,
        )

    def set_stack_offsets(
        self,
        mode: Union[int, str] = 1,
        y_offset_pct: float = 15.0,
        x_offset_pct: float = 0.0,
        normalize: bool = False,
        layer: Optional[int] = None,
        *,
        y_offset: Optional[float] = None,
        x_offset: Optional[float] = None,
        auto_waterfall: Optional[bool] = None,
        y_gap_pct: Optional[float] = None,
        x_gap_pct: Optional[float] = None,
        reset: bool = False,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        eff_y = y_offset if y_offset is not None else (y_offset_pct if y_gap_pct is None else y_gap_pct)
        eff_x = x_offset if x_offset is not None else (x_offset_pct if x_gap_pct is None else x_gap_pct)
        return self.session.call(
            "graph.set_stack_offsets",
            graph=self.name,
            layer=layer,
            mode=mode,
            y_gap_pct=eff_y,
            x_gap_pct=eff_x,
            y_offset=y_offset,
            x_offset=x_offset,
            auto_waterfall=auto_waterfall,
            normalize=normalize,
            reset=reset,
            **kwargs,
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
        columns: Optional[int] = None,
        font_size: Optional[float] = None,
        frame: Optional[Union[bool, int]] = None,
        framed: Optional[Union[bool, int]] = None,
        text: Optional[str] = None,
        layer: Optional[int] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.session.call(
            "graph.set_legend",
            graph=self.name,
            position=position,
            visible=visible,
            columns=columns,
            font_size=font_size,
            frame=frame,
            framed=framed,
            text=text,
            layer=layer,
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
        y_min: Optional[float] = None,
        y_max: Optional[float] = None,
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
        *,
        format: Optional[str] = None,
        quality: Optional[int] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Export this graph to an image or vector file (RPC ``graph.export``).

        The format follows the extension of ``path`` (``.png``, ``.tif``, ``.jpg``,
        ``.pdf``, ``.svg``, ``.eps``, ``.emf``) unless ``format`` is given.

        Args:
            path: output file; missing folders are created.
            dpi: resolution for raster formats (36-2400, default 300).
            transparent: transparent background for PNG/TIFF.
            format: force the format (e.g. ``"png"``) regardless of the extension.
            quality: JPEG quality 1-100 (default 100).

        Returns:
            dict with ``ok``, absolute ``path`` and file size ``bytes``.
        """
        return self.session.call(
            "graph.export",
            graph=self.name,
            path=os.fspath(path),
            dpi=dpi,
            transparent=transparent,
            format=format,
            quality=quality,
            **kwargs,
        )

    def export_ole(
        self,
        emf_path: Optional[Union[str, os.PathLike[str]]] = None,
        *,
        dpi: int = 300,
        include_base64: Optional[bool] = None,
        skip_project_stream: Optional[bool] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Export embedded ToriGraph OLE payload metadata + vector EMF preview."""
        return self.session.call(
            "graph.export_ole",
            graph=self.name,
            emf_path=(os.fspath(emf_path) if emf_path is not None else None),
            dpi=int(dpi),
            include_base64=include_base64,
            skip_project_stream=skip_project_stream,
            **kwargs,
        )

    def to_python(
        self,
        output_path: Optional[Union[str, os.PathLike[str]]] = None,
        *,
        standalone: bool = True,
        embed_data: str = "inline",
        path: Optional[Union[str, os.PathLike[str]]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Generate a reproducible Python script dict for this graph (`graph.to_python`)."""
        out_p = output_path if output_path is not None else path
        out_str = os.fspath(out_p) if out_p is not None else None
        res = self.session.call(
            "graph.to_python",
            graph=self.name,
            standalone=bool(standalone),
            embed_data=str(embed_data),
            output_path=out_str,
            **kwargs,
        )
        script_txt = str(res.get("script") or res.get("code") or "")
        if out_str and script_txt and not os.path.isfile(out_str):
            Path(out_str).parent.mkdir(parents=True, exist_ok=True)
            Path(out_str).write_text(script_txt, encoding="utf-8")
        return res

    def to_python_script(
        self,
        output_path: Optional[Union[str, os.PathLike[str]]] = None,
        *,
        standalone: bool = True,
        embed_data: str = "inline",
        path: Optional[Union[str, os.PathLike[str]]] = None,
        **kwargs: Any,
    ) -> str:
        """Return a standalone or console-ready ``import torigraph as tg`` Python script string."""
        res = self.to_python(
            output_path=output_path,
            standalone=standalone,
            embed_data=embed_data,
            path=path,
            **kwargs,
        )
        return str(res.get("script") or res.get("code") or "")

    def to_pptx(
        self,
        path: Union[str, os.PathLike[str]],
        *,
        per_slide: int = 1,
        deck: Optional[Union[str, os.PathLike[str]]] = None,
        slide: Optional[int] = None,
        box: Optional[Sequence[float]] = None,
        replace: Optional[Union[bool, str, Sequence[Any], Mapping[str, Any]]] = None,
        dpi: int = 300,
        overwrite: bool = False,
        titles: Optional[Union[str, Sequence[str]]] = None,
        preset: Optional[str] = None,
        widescreen: bool = True,
        scale: Optional[Union[float, int, str]] = None,
        embed_scope: str = "graph",
        allow_whole_project: bool = False,
    ) -> Dict[str, Any]:
        """Embed this graph into a PowerPoint ``.pptx`` deck as an editable OLE object.

        Same as ``session.to_pptx(path, [graph], ...)``; see :meth:`Session.to_pptx`.

        Args:
            path: output ``.pptx``.
            deck: existing deck to start from (``deck == path`` needs ``overwrite=True``).
            slide: 1-based slide to place on (a new slide when omitted).
            box: ``(left_cm, top_cm, width_cm)`` placement box.
            replace: update an already embedded graph (``True``) or replace a named shape.
            preset: style preset applied before export (e.g. ``"lab_slide_83mm"``).
            scale: place at natural size x scale (``1.4`` or ``"140%"``).
            embed_scope: ``"graph"`` (graph and its tables) or ``"project"``.
            allow_whole_project: allow falling back to a whole-project snapshot when
                per-graph OLE export is unavailable (default ``False``).

        Returns:
            dict describing the written deck and each placement.
        """
        return self.session.to_pptx(
            path,
            [self],
            per_slide=per_slide,
            deck=deck,
            slide=slide,
            box=box,
            replace=replace,
            dpi=dpi,
            overwrite=overwrite,
            titles=titles,
            preset=preset,
            widescreen=widescreen,
            scale=scale,
            embed_scope=embed_scope,
            allow_whole_project=allow_whole_project,
        )

    def copy_image(self, dpi: int = 300) -> Dict[str, Any]:
        return self.session.call("graph.copy_image", graph=self.name, dpi=dpi)

    def copy_ole(
        self,
        *,
        preset: Optional[str] = None,
        active_layer_only: bool = False,
    ) -> Dict[str, Any]:
        params: Dict[str, Any] = {
            "graph": self.name,
            "active_layer_only": bool(active_layer_only),
        }
        if preset is not None:
            params["preset"] = str(preset)
        return self.session.call("graph.copy_ole", **params)

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

    set_options = _deprecated_alias("set_3d_options", "set_options")

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

    clear_gadget = _deprecated_alias("clear_gadgets", "clear_gadget")

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

    offset_yield = _deprecated_alias("analyze_tensile", "offset_yield")

    def analyze_work_hardening(
        self,
        curve: Optional[Union[int, str]] = None,
        *,
        smooth_window: int = 11,
        create_plot: bool = True,
        plot_mode: Optional[str] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Compute Kocks-Mecking work-hardening rate (d sigma_t / d eps_t) & Considere necking point."""
        return self.session.analyze_work_hardening(
            graph=self.name,
            curve=curve,
            smooth_window=smooth_window,
            create_plot=create_plot,
            plot_mode=plot_mode,
            **kwargs,
        )

    kocks_mecking = _deprecated_alias("analyze_work_hardening", "kocks_mecking")

    def analyze_xrd(
        self,
        curve: Optional[Union[int, str]] = None,
        *,
        wavelength: float = 1.540598,
        xray_energy_kev: Optional[float] = None,
        instr_fwhm: float = 0.05,
        scherrer_k: float = 0.90,
        crystal_system: str = "fcc",
        c_over_a: Optional[float] = None,
        strip_kalpha2: bool = False,
        add_top_d_axis: bool = True,
        create_wh_plot: bool = True,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Run Bragg d-spacing, Scherrer crystallite size, and Williamson-Hall microstrain analysis."""
        return self.session.analyze_xrd(
            graph=self.name,
            curve=curve,
            wavelength=wavelength,
            xray_energy_kev=xray_energy_kev,
            instr_fwhm=instr_fwhm,
            scherrer_k=scherrer_k,
            crystal_system=crystal_system,
            c_over_a=c_over_a,
            strip_kalpha2=strip_kalpha2,
            add_top_d_axis=add_top_d_axis,
            create_wh_plot=create_wh_plot,
            **kwargs,
        )

    xrd_analyze = _deprecated_alias("analyze_xrd", "xrd_analyze")

    def bragg_convert(
        self,
        *,
        wavelength: float = 1.540598,
        xray_energy_kev: Optional[float] = None,
        mode: str = "d_spacing",
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Convert XRD 2-theta axis/curve to Bragg d-spacing or scattering vector Q."""
        return self.session.bragg_convert(
            graph=self.name,
            wavelength=wavelength,
            xray_energy_kev=xray_energy_kev,
            mode=mode,
            **kwargs,
        )


class Session:
    """Connected ToriGraph JSON-RPC 2.0 Automation Session."""

    def __init__(
        self,
        transport: Union[_TcpTransport, _LocalPipeTransport],
        process: Optional[subprocess.Popen[bytes]] = None,
        cleanup_files: Optional[Sequence[str]] = None,
        rpc_timeout: Optional[float] = None,
    ) -> None:
        self._transport = transport
        self._process = process
        self._cleanup_files = list(cleanup_files or [])
        self._next_id = 1
        self._closed = False
        #: Seconds to wait for each RPC answer (``None`` -> DEFAULT_RPC_TIMEOUT).
        self.rpc_timeout: float = float(DEFAULT_RPC_TIMEOUT if rpc_timeout is None else rpc_timeout)
        #: Number of stale responses (answers to earlier, timed-out requests) discarded.
        self.discarded_responses = 0
        self._broken_reason: Optional[str] = None
        self._active_batch: Optional[BatchContext] = None
        self._slide_preset_graphs: set[str] = set()
        #: (endpoint, connect timeout, token) used by :meth:`reconnect`; set by connect()/launch().
        self._reconnect_info: Optional[Tuple[str, float, Optional[str]]] = None
        self._finalizer: Optional[weakref.finalize] = self._make_finalizer()

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
        self.workbook = _WorkbookDomain(self)

    def _make_finalizer(self) -> Optional[weakref.finalize]:
        if self._process is None and not self._cleanup_files:
            return None
        return weakref.finalize(
            self,
            _cleanup_spawned_session,
            self._process,
            self._transport,
            list(self._cleanup_files),
        )

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

    @property
    def usable(self) -> bool:
        """False once the session was closed or its connection was invalidated."""
        return not self._closed and self._broken_reason is None

    def reconnect(self, timeout: Optional[float] = None) -> "Session":
        """Open a fresh connection to the same endpoint after a timeout or a lost connection.

        Works for sessions made by :func:`connect` / :func:`connect_or_launch` /
        :func:`launch` (the launched process must still be running).  Windows, tables
        and graphs in ToriGraph are untouched -- only the transport is replaced, so
        existing proxies keep working.  A request that timed out may still be running
        in ToriGraph; its late answer is dropped.  Raises :class:`ToriGraphDisconnectedError`
        when the endpoint cannot be reached.  Returns ``self``.
        """
        if self._closed:
            raise ToriGraphDisconnectedError("Session is already closed; connect or launch again.")
        if self._reconnect_info is None:
            raise ToriGraphDisconnectedError(
                "This session was built from a custom transport and cannot reconnect; "
                "connect or launch again."
            )
        proc = self._process
        if proc is not None and proc.poll() is not None:
            raise ToriGraphDisconnectedError(
                f"The launched ToriGraph process has exited (code {proc.returncode}); "
                "launch a new session."
            )
        endpoint, connect_timeout, token = self._reconnect_info
        if timeout is not None:
            connect_timeout = float(timeout)
        try:
            self._transport.close()
        except Exception:
            pass
        try:
            transport = _parse_and_connect_transport(endpoint, timeout=connect_timeout, token=token)
        except TransportError as exc:
            if isinstance(exc, ToriGraphDisconnectedError):
                raise
            raise ToriGraphDisconnectedError(f"Reconnect to {endpoint} failed: {exc}") from exc
        if self._finalizer is not None:
            self._finalizer.detach()
        self._transport = transport
        self._broken_reason = None
        self._active_batch = None
        self._finalizer = self._make_finalizer()
        self.call("ping", rpc_timeout=max(1.0, connect_timeout))
        return self

    def _invalidate(self, reason: str) -> None:
        """Mark the connection unusable (the request/response stream can no longer be trusted)."""
        if self._broken_reason is None:
            self._broken_reason = reason
        try:
            self._transport.close()
        except Exception:
            pass

    def call(
        self,
        _method: Optional[str] = None,
        _params: Optional[Mapping[str, Any]] = None,
        /,
        *,
        rpc_timeout: Optional[float] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Invoke any ToriGraph JSON-RPC 2.0 method and return its result dictionary.

        ``rpc_timeout`` (seconds) overrides :attr:`rpc_timeout` for this call only;
        it is not sent to ToriGraph.  Responses are matched by JSON-RPC ``id``: an
        answer to an earlier request (e.g. one that arrived after that request
        timed out) is discarded.  If the answer does not arrive in time, or the
        stream is out of sync, the session is invalidated and later calls raise
        :class:`TransportError` -- reconnect (or relaunch) to continue.
        """
        if self._closed:
            raise ToriGraphDisconnectedError("Session is already closed.")
        if self._broken_reason is not None:
            raise ToriGraphDisconnectedError(
                f"Session is no longer usable ({self._broken_reason}). "
                "Call session.reconnect() or connect / launch again."
            )

        if _method is None:
            kw_method = kwargs.pop("method", None)
            if not kw_method:
                raise ValueError("Specify JSON-RPC 'method' name.")
            _method = str(kw_method)

        merged: Dict[str, Any] = {}
        if _params is not None:
            merged.update(_sanitize_for_json(dict(_params)))
        if kwargs:
            merged.update(_clean_kwargs(kwargs))

        if self._active_batch is not None and _method != "app.batch":
            if _is_read_only_rpc(_method):
                raise BatchReadError(
                    f"'{_method}' reads data from ToriGraph, but inside `with app.batch():` calls are "
                    "only recorded and run together when the block exits, so there is no value to "
                    "return yet. Do the read before or after the `with` block (or reference a "
                    "previous step with BatchStepRef / `$steps[i].field`)."
                )
            return self._active_batch._record_raw(_method, merged)

        for attempt in range(4):
            req_id = self._next_id
            self._next_id += 1
            req_obj = {
                "jsonrpc": "2.0",
                "id": req_id,
                "method": str(_method),
                "params": merged,
            }
            raw_req = json.dumps(req_obj, ensure_ascii=False, allow_nan=False).encode("utf-8")
            resp = self._exchange(raw_req, req_id, str(_method), rpc_timeout)

            if "error" in resp and resp["error"] is not None:
                err = resp["error"]
                if isinstance(err, Mapping):
                    code = int(err.get("code", -32000))
                    if code == -32003 and attempt < 3:
                        time.sleep(0.05 * (attempt + 1))
                        continue
                    raise RpcError(
                        code,
                        str(err.get("message", "Unknown RPC error")),
                        err.get("data"),
                    )
                raise RpcError(-32000, str(err))
            break

        result = resp.get("result")
        if _method == "graph.apply_style_preset":
            gname = (
                (result.get("graph") if isinstance(result, Mapping) else None)
                or merged.get("graph")
            )
            preset_nm = str(merged.get("preset") or "").strip().lower()
            if isinstance(gname, str) and gname:
                if preset_nm in ("lab_slide_83mm", "lab_slide"):
                    self._slide_preset_graphs.add(gname)
                else:
                    self._slide_preset_graphs.discard(gname)
        elif _method == "app.close_window":
            closed_nm = (
                merged.get("name")
                or (result.get("name") if isinstance(result, Mapping) else None)
            )
            if isinstance(closed_nm, str) and closed_nm:
                self._slide_preset_graphs.discard(closed_nm)
        elif _method == "app.new_project" or (
            _method == "app.open_project" and not merged.get("append")
        ):
            self._slide_preset_graphs.clear()
        return result if isinstance(result, dict) else {"value": result}

    def _exchange(
        self, raw_req: bytes, req_id: int, method: str, rpc_timeout: Optional[float]
    ) -> Dict[str, Any]:
        """Send one request and return the response object whose ``id`` equals ``req_id``."""
        timeout = float(self.rpc_timeout if rpc_timeout is None else rpc_timeout)
        if not timeout > 0:
            raise ValueError("rpc_timeout must be a positive number of seconds.")
        deadline = time.monotonic() + timeout
        try:
            self._transport.send_line(raw_req, timeout)
        except TransportError as exc:
            self._invalidate(f"sending '{method}' failed: {exc}")
            if isinstance(exc, (ToriGraphTimeoutError, ToriGraphDisconnectedError)):
                raise
            if "timed out" in str(exc).lower():
                raise ToriGraphTimeoutError(str(exc)) from exc
            raise ToriGraphDisconnectedError(str(exc)) from exc
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                self._invalidate(f"no answer to '{method}' within {timeout:.1f}s")
                raise ToriGraphTimeoutError(
                    f"Timed out after {timeout:.1f}s waiting for the answer to '{method}'. "
                    "Pass rpc_timeout=... (per call) or set session.rpc_timeout for long operations; "
                    "session.reconnect() restores the connection."
                )
            try:
                raw_resp = self._transport.recv_line(remaining)
            except TransportError as exc:
                self._invalidate(f"no answer to '{method}': {exc}")
                # TCP says "Timed out ...", the named pipe "... timed out after ..."
                if "timed out" in str(exc).lower():
                    raise ToriGraphTimeoutError(
                        f"Timed out after {timeout:.1f}s waiting for the answer to '{method}'. "
                        "Pass rpc_timeout=... (per call) or set session.rpc_timeout for long "
                        "operations. This session can no longer be used; call "
                        "session.reconnect() to continue."
                    ) from exc
                if isinstance(exc, (ToriGraphTimeoutError, ToriGraphDisconnectedError)):
                    raise
                raise ToriGraphDisconnectedError(str(exc)) from exc
            try:
                resp = json.loads(raw_resp.decode("utf-8"))
            except Exception as exc:
                self._invalidate("malformed response")
                raise TransportError(f"Malformed JSON response from ToriGraph: {exc}") from exc
            if not isinstance(resp, dict):
                self._invalidate("unexpected response type")
                raise TransportError(
                    f"Unexpected JSON-RPC response (expected an object): {type(resp).__name__}"
                )
            resp_id = resp.get("id")
            if isinstance(resp_id, bool):
                resp_id = None
            if isinstance(resp_id, (int, float)) and resp_id == req_id:
                return resp
            if resp_id is None and resp.get("error") is not None:
                # Server could not read our request id (parse error / invalid request):
                # by JSON-RPC 2.0 the error carries id null and answers this request.
                return resp
            if isinstance(resp_id, (int, float)) and resp_id < req_id:
                # Late answer to an earlier request: never hand it to this caller.
                self.discarded_responses += 1
                continue
            self._invalidate(f"response id {resp_id!r} does not match request id {req_id}")
            raise TransportError(
                f"ToriGraph answered with id {resp_id!r} while waiting for id {req_id} "
                f"('{method}'); the session was invalidated."
            )

    def batch(
        self,
        commands: Optional[Sequence[Mapping[str, Any]]] = None,
        *,
        stop_on_error: bool = True,
        atomic: bool = True,
        defer_replot: bool = True,
        raise_on_error: bool = True,
    ) -> Any:
        """Execute a command list via ``app.batch`` OR enter a recording ``with app.batch() as b:`` block.

        When an atomic batch fails and the workspace is rebuilt from its pre-batch snapshot, the
        result has ``rolled_back=True`` and ``undo_history_cleared=True``: the data is back, but the
        Undo history from before the batch is gone.
        """
        if commands is not None:
            return self.call(
                "app.batch",
                commands=[dict(c) for c in commands],
                stop_on_error=bool(stop_on_error),
                atomic=bool(atomic),
                defer_replot=bool(defer_replot),
            )
        return BatchContext(
            self,
            stop_on_error=stop_on_error,
            atomic=atomic,
            defer_replot=defer_replot,
            raise_on_error=raise_on_error,
        )

    def _ensure_table_ref(
        self,
        obj: Any,
        *,
        name: Optional[str] = None,
    ) -> Optional[str]:
        """Resolve a table name/proxy OR materialize an inline DataFrame/ndarray/dict into a worksheet."""
        if obj is None:
            return None
        if isinstance(obj, BatchStepRef):
            return obj.to_token()
        if isinstance(obj, (str, TableProxy)):
            return _name_of(obj)
        if _is_dataframe_like(obj) or isinstance(obj, Mapping) or _is_ndarray_like(obj):
            return self.create_table(obj, name=name).name
        return _name_of(obj)

    # ------------------------------------------------------------------
    # 1. App / Project / Window / Action / Sample Datasets / Batch
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

    def list_sample_datasets(
        self, category: Optional[str] = None
    ) -> List[Dict[str, Any]]:
        """List built-in self-contained materials science sample datasets."""
        res = self.call("app.list_sample_datasets", category=category)
        return list(res.get("datasets", res.get("samples", [])))

    def load_sample_dataset(
        self,
        dataset_id: Optional[str] = None,
        *,
        id: Optional[str] = None,  # noqa: A002
        name: Optional[str] = None,
        auto_plot: bool = True,
        plot: Optional[bool] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Load a built-in materials science sample dataset (and optionally auto-plot it)."""
        resolved_id = dataset_id or id or name
        if not resolved_id:
            raise ValueError("Specify sample dataset 'dataset_id' (or 'id').")
        do_plot = auto_plot if plot is None else bool(plot)
        return self.call(
            "app.load_sample_dataset",
            id=str(resolved_id),
            auto_plot=bool(do_plot),
            **kwargs,
        )

    def new_project(self, *, force: Optional[bool] = None) -> Dict[str, Any]:
        return self.call("app.new_project", force=force)

    def open_project(
        self,
        path: Union[str, os.PathLike[str]],
        append: bool = False,
        *,
        force: Optional[bool] = None,
    ) -> Dict[str, Any]:
        return self.call(
            "app.open_project", path=os.fspath(path), append=append, force=force
        )

    def save_project(
        self,
        path: Optional[Union[str, os.PathLike[str]]] = None,
        compress: bool = False,
        *,
        snapshot: bool = False,
    ) -> Dict[str, Any]:
        return self.call(
            "app.save_project",
            path=os.fspath(path) if path is not None else None,
            compress=compress,
            snapshot=(True if snapshot else None),
        )

    def import_csv(
        self,
        path: Union[str, os.PathLike[str]],
        separator: Optional[str] = None,
        skip_lines: int = 0,
        header: Optional[bool] = None,
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
        self, path: Union[str, os.PathLike[str]], codec: Optional[str] = ""
    ) -> Dict[str, Any]:
        return self.call("app.import_opj", path=os.fspath(path), codec=(codec or ""))

    def import_xlsx(
        self,
        path: Union[str, os.PathLike[str]],
        sheet: Optional[str] = None,
        *,
        workbook: bool = True,
    ) -> List[TableProxy]:
        """Import an Excel workbook (.xlsx/.xlsm); one worksheet per sheet (or only `sheet`).

        With ``workbook=True`` (default) two or more sheets become the sheet tabs of one
        workbook named after the file; ``workbook=False`` keeps separate worksheet windows.
        """
        res = self.call(
            "app.import_xlsx", path=os.fspath(path), sheet=(sheet or ""), workbook=bool(workbook)
        )
        return [TableProxy(self, str(n)) for n in res.get("tables", [])]

    def new_workbook(
        self,
        name: Optional[str] = None,
        sheets: int = 1,
        *,
        tables: Optional[Sequence[Union[str, "TableProxy"]]] = None,
    ) -> "WorkbookProxy":
        """Create a workbook with ``sheets`` new empty sheets, or group existing ``tables``."""
        res = self.call(
            "workbook.create",
            name=(name or ""),
            sheets=int(sheets),
            tables=[_name_of(t) for t in (tables or [])],
        )
        return WorkbookProxy(self, str(res["name"]))

    def list_workbooks(self) -> List[Dict[str, Any]]:
        return list(self.call("workbook.list").get("workbooks", []))

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
        self,
        name: Optional[Union[str, TableProxy, MatrixProxy, GraphProxy]] = None,
        new_name: Optional[str] = None,
        *,
        old_name: Optional[Union[str, TableProxy, MatrixProxy, GraphProxy]] = None,
    ) -> Dict[str, Any]:
        target = name if name is not None else old_name
        if target is None or new_name is None:
            raise ValueError("Provide window 'name' (or 'old_name') and 'new_name'.")
        res = self.call("app.rename_window", name=_name_of(target), new_name=new_name)
        if isinstance(target, (TableProxy, MatrixProxy, GraphProxy)) and res.get("name"):
            target.name = str(res["name"])
            if self._active_batch is not None and target.name.startswith("$steps["):
                self._active_batch._register_proxy(target, target.name)
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

    def set_language(
        self,
        language: Optional[str] = None,
        *,
        lang: Optional[str] = None,
    ) -> Dict[str, Any]:
        eff_lang = lang if lang is not None else language
        if eff_lang is None:
            raise ValueError("Provide target UI 'language' (e.g. 'ja' or 'en').")
        return self.call("app.set_language", language=str(eff_lang))

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
        table: Optional[Union[str, TableProxy, GraphProxy]] = None,
        columns: Optional[Union[str, TableProxy, Sequence[Union[int, str]]]] = None,
        x_column: Optional[Union[int, str, Sequence[Union[int, str]]]] = None,
        graph: Optional[Union[str, GraphProxy]] = None,
        layer: Optional[int] = None,
        right_y: bool = False,
        replace: bool = False,
        *,
        cols: Optional[Sequence[Union[int, str]]] = None,
    ) -> GraphProxy:
        # Support both (table, columns, ..., graph=g) and describe_api (graph, table, cols)
        if isinstance(table, GraphProxy) or (
            graph is None
            and isinstance(columns, (TableProxy, str))
            and (x_column is not None or cols is not None)
            and not isinstance(columns, (list, tuple))
        ):
            eff_graph: Optional[Union[str, GraphProxy]] = table  # type: ignore[assignment]
            eff_table: Union[str, TableProxy] = columns  # type: ignore[assignment]
            raw_cols = cols if cols is not None else x_column
            eff_x_col: Optional[Union[int, str]] = None
        else:
            eff_graph = graph
            eff_table = table  # type: ignore[assignment]
            raw_cols = cols if cols is not None else columns
            eff_x_col = x_column if isinstance(x_column, (int, str)) else None

        col_list = (
            [raw_cols]
            if isinstance(raw_cols, (str, int))
            else [c for c in (raw_cols or [])]
        )
        res = self.call(
            "graph.drop_columns",
            table=_name_of(eff_table),
            columns=col_list,
            x_column=(eff_x_col if eff_x_col is not None else ""),
            graph=(_name_of(eff_graph) if eff_graph is not None else ""),
            layer=(int(layer) if layer is not None else None),
            right_y=bool(right_y),
            replace=bool(replace),
        )
        g_name = res.get("graph") or res.get("name") or (_name_of(eff_graph) if eff_graph is not None else "")
        return GraphProxy(self, str(g_name))

    def set_focus_hud(self, enabled: Optional[bool] = True) -> Dict[str, Any]:
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

    def list_actions(
        self,
        filter: Optional[str] = None,  # noqa: A002
        *,
        filter_text: Optional[str] = None,
    ) -> List[Dict[str, Any]]:
        eff_filter = filter_text if filter_text is not None else filter
        res = self.call("app.list_actions", filter=eff_filter)
        return list(res.get("actions", []))

    def trigger_action(
        self, name: str, checked: Optional[bool] = None, **kwargs: Any
    ) -> Dict[str, Any]:
        return self.call("app.trigger_action", name=name, checked=checked, **kwargs)

    # ------------------------------------------------------------------
    # 2 & 3. Table & Matrix Creation (with NumPy / Pandas / Polars Adapter)
    # ------------------------------------------------------------------
    def create_table(
        self,
        rows: Any = 30,
        cols: Optional[int] = None,
        name: Optional[str] = None,
        *,
        col_names: Optional[Sequence[str]] = None,
        columns: Optional[Sequence[str]] = None,
        designations: Optional[Union[Sequence[str], Mapping[str, str]]] = None,
        data: Optional[Any] = None,
        units: Optional[Mapping[str, str]] = None,
        long_names: Optional[Mapping[str, str]] = None,
        comments: Optional[Mapping[str, str]] = None,
        include_index: Optional[bool] = None,
        **kwargs: Any,
    ) -> TableProxy:
        eff_col_names = col_names if col_names is not None else columns

        # Allow passing DataFrame / ndarray / dict directly as first positional argument
        if not isinstance(rows, (int, str)) or isinstance(rows, bool):
            if _is_dataframe_like(rows) or isinstance(rows, Mapping) or _is_ndarray_like(rows):
                if data is not None:
                    raise ValueError("Specify tabular 'data' either positionally or via data=..., not both.")
                data = rows
                rows = 30

        if isinstance(rows, str):
            if name is None:
                name = rows
            rows = 30

        coerced_payload: Optional[Dict[str, List[Any]]] = None
        inf_units: Dict[str, str] = {}
        inf_long_names: Dict[str, str] = {}
        inf_comments: Dict[str, str] = {}
        inf_desig: Dict[str, str] = {}

        if data is not None:
            (
                coerced_payload,
                ordered_cols,
                inf_units,
                inf_long_names,
                inf_comments,
                inf_desig,
            ) = _coerce_tabular_input(
                data, col_names=eff_col_names, include_index=include_index
            )
            eff_col_names = ordered_cols
            inferred_rows = max((len(v) for v in coerced_payload.values()), default=1)
            rows = max(1, inferred_rows) if cols is None else max(int(rows), inferred_rows)

        resolved_cols = (
            int(cols)
            if cols is not None
            else (max(1, len(eff_col_names)) if eff_col_names is not None else 2)
        )
        if eff_col_names is not None and resolved_cols > 0 and len(eff_col_names) > resolved_cols:
            resolved_cols = len(eff_col_names)

        eff_designations: Any = None
        if designations is not None:
            eff_designations = (
                dict(designations)
                if isinstance(designations, Mapping)
                else list(designations)
            )
        elif inf_desig:
            eff_designations = dict(inf_desig)

        # list/tuple designations ["X", "Y", "yErr"] apply to the columns in order; they
        # must reach table.set_data_dict as a {column: role} dict or the server resets
        # every column to X/Y.  Validated before the table is created.
        desig_order = (
            list(eff_col_names)
            if eff_col_names is not None
            else (list(coerced_payload.keys()) if coerced_payload is not None else [])
        )
        explicit_desig = _coerce_designations_mapping(
            designations, desig_order, total_cols=resolved_cols
        )

        res = self.call(
            "table.create",
            rows=int(rows),
            cols=resolved_cols,
            name=name,
            col_names=(list(eff_col_names) if eff_col_names is not None else None),
            designations=eff_designations,
            **kwargs,
        )
        tbl = TableProxy(self, res["name"])
        if coerced_payload is not None:
            merged_units = {**inf_units, **(dict(units) if units is not None else {})} or None
            merged_long = {**inf_long_names, **(dict(long_names) if long_names is not None else {})} or None
            merged_comm = {**inf_comments, **(dict(comments) if comments is not None else {})} or None
            merged_desig = {**inf_desig, **explicit_desig} or None
            self.call(
                "table.set_data_dict",
                table=tbl.name,
                data=coerced_payload,
                column_order=list(eff_col_names) if eff_col_names is not None else list(coerced_payload.keys()),
                units=merged_units,
                long_names=merged_long,
                comments=merged_comm,
                designations=merged_desig,
            )
        elif units or long_names or comments:
            names_for_meta = list(eff_col_names) if eff_col_names is not None else []
            all_meta_cols = list(
                dict.fromkeys(
                    names_for_meta
                    + list((units or {}).keys())
                    + list((long_names or {}).keys())
                    + list((comments or {}).keys())
                )
            )
            for cname in all_meta_cols:
                tbl.set_meta(
                    cname,
                    units=(units or {}).get(cname),
                    long_name=(long_names or {}).get(cname),
                    comment=(comments or {}).get(cname),
                )
        return tbl

    def create_matrix(
        self,
        rows: Any = 32,
        cols: int = 32,
        name: Optional[str] = None,
        x_min: Optional[float] = 0.0,
        x_max: Optional[float] = 1.0,
        y_min: Optional[float] = 0.0,
        y_max: Optional[float] = 1.0,
        *,
        values: Optional[Any] = None,
        data: Optional[Any] = None,
        **kwargs: Any,
    ) -> MatrixProxy:
        grid_in = values if values is not None else data
        if not isinstance(rows, (int, str)) or isinstance(rows, bool):
            if _is_ndarray_like(rows) or _is_dataframe_like(rows) or isinstance(rows, (list, tuple)):
                grid_in = rows
                rows = 32

        if isinstance(rows, str):
            if name is None:
                name = rows
            rows = 32

        coerced_grid: Optional[List[List[Optional[float]]]] = None
        if grid_in is not None:
            coerced_grid = _coerce_2d_matrix(grid_in)
            if len(coerced_grid) > 0:
                rows = len(coerced_grid)
                cols = len(coerced_grid[0]) if len(coerced_grid[0]) > 0 else 1

        res = self.call(
            "matrix.create",
            rows=int(rows),
            cols=int(cols),
            name=name,
            x_min=(0.0 if x_min is None else x_min),
            x_max=(1.0 if x_max is None else x_max),
            y_min=(0.0 if y_min is None else y_min),
            y_max=(1.0 if y_max is None else y_max),
            **kwargs,
        )
        mat = MatrixProxy(self, res["name"])
        if coerced_grid is not None:
            self.call("matrix.set_data", matrix=mat.name, data=coerced_grid)
        return mat

    # ------------------------------------------------------------------
    # 4. 2D / 3D / Matrix / Specialty Plotting
    # ------------------------------------------------------------------
    def plot_xy(
        self,
        table: Optional[Any] = None,
        x_col: Optional[Any] = None,
        y_cols: Optional[Any] = None,
        y_err_col: Optional[Union[int, str]] = None,
        x_err_col: Optional[Union[int, str]] = None,
        style: str = "line_symbol",
        graph_name: Optional[str] = None,
        *,
        x: Optional[Any] = None,
        y: Optional[Any] = None,
        y_err: Optional[Any] = None,
        y_err_cols: Optional[Union[int, str, Sequence[Union[int, str]]]] = None,
        title: Optional[str] = None,
        xlabel: Optional[str] = None,
        ylabel: Optional[str] = None,
        xlim: Optional[Sequence[float]] = None,
        ylim: Optional[Sequence[float]] = None,
        color: Optional[str] = None,
        lw: Optional[float] = None,
        ls: Optional[str] = None,
        marker: Optional[str] = None,
        ms: Optional[int] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        """Create a 2D XY graph (RPC ``plot.xy``).

        Two ways to give the data:

        * Worksheet columns: ``plot_xy(table, x_col, y_cols)`` where ``table`` is a
          :class:`TableProxy`, a worksheet name, a ``pandas``/``polars`` DataFrame,
          a ``dict`` of columns or a 2-D array (DataFrame / dict / array are first
          copied into a new worksheet).  ``y_cols`` may be one column or a list.
        * Plain arrays: ``plot_xy(x=[...], y=[...])`` (or positionally
          ``plot_xy(x_values, y_values)``).  ``y`` may also be a list of arrays for
          several curves sharing ``x``; ``y_err`` gives error-bar values (one array,
          or one per curve).  A new worksheet is created for the arrays.

        Args:
            y_err_col, x_err_col, y_err_cols: error-bar columns (worksheet mode).
            style: ``"line"``, ``"scatter"``, ``"line_symbol"`` (default), ``"spline"``,
                ``"vertical_bars"``, ``"horizontal_bars"``, ``"area"``,
                ``"vertical_steps"``, ``"horizontal_steps"``, ``"drop_lines"``.
            graph_name: name of the new graph window.
            title, xlabel, ylabel, xlim, ylim: graph title, axis titles and ranges.
            color, lw, ls, marker, ms: style of the new curves.
            **kwargs: further RPC parameters (``app.describe_api("plot.xy")``).

        Returns:
            :class:`GraphProxy` of the new graph.
        """
        if graph_name is None and "name" in kwargs:
            graph_name = kwargs.pop("name")
        kwargs.update(
            title=title, xlabel=xlabel, ylabel=ylabel, xlim=xlim, ylim=ylim,
            color=color, lw=lw, ls=ls, marker=marker, ms=ms,
        )

        # Plain-array mode.  Positional ``plot_xy(x_values, y_values)`` is kept for
        # compatibility; the second positional slot is named ``x_col`` for the
        # worksheet mode, so it is re-read here explicitly as the Y values.
        def _is_values(obj: Any) -> bool:
            return obj is not None and not isinstance(obj, (int, str, bool)) and (
                _is_1d_series_or_array(obj)
                or (_is_ndarray_like(obj) and int(obj.ndim) == 2)
                or isinstance(obj, (list, tuple))
            )

        array_mode = x is not None or y is not None
        table_is_flat_values = _is_1d_series_or_array(table) or (
            isinstance(table, (list, tuple))
            and len(table) > 0
            and not isinstance(table[0], (list, tuple))
        )
        if not array_mode and table_is_flat_values:
            if not _is_values(x_col):
                raise TypeError(
                    "plot_xy: X values were given without Y values. Use "
                    "plot_xy(x=[...], y=[...]) or plot_xy(table, x_col, y_cols)."
                )
            x, y = table, x_col
            table, x_col = None, None
            array_mode = True
        if array_mode:
            if table is not None or x_col is not None:
                raise TypeError(
                    "plot_xy: give either worksheet columns (table, x_col, y_cols) or "
                    "arrays (x=..., y=...), not both."
                )
            if y_cols is not None:
                raise TypeError(
                    "plot_xy: the third positional argument (y_cols) is not used with "
                    "array data; pass error bars as y_err=... and the style as style=..."
                )
            if x is None or y is None:
                raise TypeError("plot_xy: array mode needs both x=... and y=....")
            x_attrs = _extract_series_attrs(x)
            xn = x_attrs.get("name", "X")
            if _is_ndarray_like(y) and int(y.ndim) == 2:
                y_list = [y[:, i] for i in range(int(y.shape[1]))]
            elif isinstance(y, (list, tuple)) and len(y) > 0 and _is_values(y[0]):
                y_list = list(y)
            else:
                y_list = [y]
            if y_err is None:
                err_list: List[Any] = []
            elif isinstance(y_err, (list, tuple)) and len(y_err) > 0 and _is_values(y_err[0]):
                err_list = list(y_err)
            else:
                err_list = [y_err]
            if err_list and len(err_list) != len(y_list):
                raise ValueError(
                    f"plot_xy: {len(err_list)} y_err arrays for {len(y_list)} y arrays."
                )
            cols: Dict[str, Any] = {xn: x}
            designations: Dict[str, str] = {xn: "X"}
            y_names: List[str] = []
            err_names: List[str] = []
            for i, ys in enumerate(y_list):
                base = _extract_series_attrs(ys).get("name") or ("Y" if len(y_list) == 1 else f"Y{i + 1}")
                yn = base
                k = 2
                while yn in cols:
                    yn = f"{base}_{k}"
                    k += 1
                cols[yn] = ys
                designations[yn] = "Y"
                y_names.append(yn)
                if err_list:
                    en = f"{yn}_err"
                    while en in cols:
                        en += "_"
                    cols[en] = err_list[i]
                    designations[en] = "yErr"
                    err_names.append(en)
            tbl_auto = self.create_table(cols, designations=designations)
            eff_table_ref: Optional[str] = tbl_auto.name
            eff_x_col: Optional[Any] = xn
            eff_y_cols: Optional[Any] = y_names
            if err_names:
                y_err_cols = err_names
        elif _is_dataframe_like(table) or isinstance(table, Mapping) or _is_ndarray_like(table):
            tbl_auto = self.create_table(table)
            eff_table_ref = tbl_auto.name
            eff_x_col = x_col
            eff_y_cols = y_cols
        else:
            eff_table_ref = _name_of(table)
            eff_x_col = x_col
            eff_y_cols = y_cols

        norm_y_errs: Optional[List[Union[int, str]]] = (
            None
            if y_err_cols is None
            else ([y_err_cols] if isinstance(y_err_cols, (int, str)) else list(y_err_cols))
        )
        res = self.call(
            "plot.xy",
            table=eff_table_ref,
            x_col=eff_x_col,
            y_cols=eff_y_cols,
            y_err_col=y_err_col,
            y_err_cols=norm_y_errs,
            x_err_col=x_err_col,
            style=style,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_bars(
        self,
        table: Union[str, TableProxy, Any],
        x_col: Optional[Union[int, str]] = None,
        y_cols: Optional[Union[int, str, Sequence[Union[int, str]]]] = None,
        y_err_col: Optional[Union[int, str]] = None,
        *,
        y_err_cols: Optional[Union[int, str, Sequence[Union[int, str]]]] = None,
        horizontal: bool = False,
        stacked: bool = False,
        percent_100: bool = False,
        bar_gap: Optional[int] = None,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        """Create a single, grouped, stacked, or 100%-normalized categorical/numeric bar chart."""
        if stacked or percent_100:
            norm_y: Optional[Sequence[Union[int, str]]] = (
                None
                if y_cols is None
                else ([y_cols] if isinstance(y_cols, (int, str)) else list(y_cols))
            )
            return self.plot_stacked_bars(
                table,
                x_col=x_col,
                y_cols=norm_y,
                horizontal=horizontal,
                bar_gap=(25 if bar_gap is None else int(bar_gap)),
                graph_name=graph_name,
                percent_100=percent_100,
                **kwargs,
            )
        bar_style = "horizontal_bars" if horizontal else "vertical_bars"
        return self.plot_xy(
            table,
            x_col=x_col,
            y_cols=y_cols,
            y_err_col=y_err_col,
            y_err_cols=y_err_cols,
            style=bar_style,
            bar_gap=bar_gap,
            graph_name=graph_name,
            **kwargs,
        )

    def plot_stacked_bars(
        self,
        table: Union[str, TableProxy, Any],
        x_col: Optional[Union[int, str]] = None,
        y_cols: Optional[Sequence[Union[int, str]]] = None,
        horizontal: bool = False,
        bar_gap: int = 25,
        graph_name: Optional[str] = None,
        *,
        percent_100: bool = False,
        **kwargs: Any,
    ) -> GraphProxy:
        tbl_ref = self._ensure_table_ref(table)
        res = self.call(
            "plot.stacked_bars",
            table=tbl_ref,
            x_col=x_col,
            y_cols=(
                [y_cols]
                if isinstance(y_cols, (int, str))
                else (list(y_cols) if y_cols is not None else None)
            ),
            horizontal=bool(horizontal),
            percent_100=bool(percent_100),
            bar_gap=bar_gap,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_polar(
        self,
        table: Union[str, TableProxy, Any],
        theta_col: Union[int, str] = 0,
        r_cols: Optional[Union[int, str, Sequence[Union[int, str]]]] = None,
        *,
        theta_in_radians: bool = False,
        degrees: Optional[bool] = None,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        """Create a 2D polar / radar coordinate plot (`plot.polar`)."""
        tbl_ref = self._ensure_table_ref(table)
        in_rad = (not bool(degrees)) if degrees is not None else bool(theta_in_radians)
        norm_r: Optional[List[Union[int, str]]] = None
        if r_cols is not None:
            norm_r = [r_cols] if isinstance(r_cols, (int, str)) else list(r_cols)
        res = self.call(
            "plot.polar",
            table=tbl_ref,
            theta_col=theta_col,
            r_cols=norm_r,
            theta_in_radians=in_rad,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_double_y(
        self,
        table: Union[str, TableProxy, Any],
        x_col: Optional[Union[int, str]] = None,
        y1_col: Optional[Union[int, str]] = None,
        y2_col: Optional[Union[int, str]] = None,
        graph_name: Optional[str] = None,
        *,
        y_left_col: Optional[Union[int, str]] = None,
        y_right_col: Optional[Union[int, str]] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        tbl_ref = self._ensure_table_ref(table)
        eff_y1 = y1_col if y1_col is not None else y_left_col
        eff_y2 = y2_col if y2_col is not None else y_right_col
        res = self.call(
            "plot.double_y",
            table=tbl_ref,
            x_col=x_col,
            y1_col=eff_y1,
            y2_col=eff_y2,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_waterfall(
        self,
        table: Union[str, TableProxy, Any],
        x_col: Optional[Union[int, str]] = None,
        y_cols: Optional[Sequence[Union[int, str]]] = None,
        y_offset_pct: float = 15.0,
        x_offset_pct: float = 0.0,
        normalize: bool = False,
        graph_name: Optional[str] = None,
        *,
        offset_x: Optional[float] = None,
        offset_y: Optional[float] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        tbl_ref = self._ensure_table_ref(table)
        eff_x_off = offset_x if offset_x is not None else x_offset_pct
        eff_y_off = offset_y if offset_y is not None else y_offset_pct
        res = self.call(
            "plot.waterfall",
            table=tbl_ref,
            x_col=x_col,
            y_cols=y_cols,
            y_offset_pct=eff_y_off,
            x_offset_pct=eff_x_off,
            normalize=normalize,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_histogram(
        self,
        table: Union[str, TableProxy, Any],
        cols: Optional[Union[int, str, Sequence[Union[int, str]]]] = None,
        graph_name: Optional[str] = None,
        *,
        stacked: Optional[bool] = None,
        bins: Optional[int] = None,
        bin_size: Optional[float] = None,
        bin_min: Optional[float] = None,
        bin_max: Optional[float] = None,
        overlay_pdf: Optional[Union[bool, str]] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        tbl_ref = self._ensure_table_ref(table)
        key = "cols" if isinstance(cols, (list, tuple)) else "col"
        params: Dict[str, Any] = {"table": tbl_ref, **kwargs}
        if graph_name is not None:
            params["graph_name"] = graph_name
        if cols is not None:
            params[key] = cols
        if stacked is not None:
            params["stacked"] = stacked
        if bins is not None:
            params["bins"] = bins
        if bin_size is not None:
            params["bin_size"] = bin_size
        if bin_min is not None:
            params["bin_min"] = bin_min
        if bin_max is not None:
            params["bin_max"] = bin_max
        if overlay_pdf is not None:
            params["overlay_pdf"] = overlay_pdf
        res = self.call("plot.histogram", params)
        return GraphProxy(self, res["name"])

    def plot_box(
        self,
        table: Union[str, TableProxy, Any],
        cols: Optional[Sequence[Union[int, str]]] = None,
        graph_name: Optional[str] = None,
        *,
        notched: Optional[bool] = None,
        whisker_mode: Optional[str] = None,
        show_points: Optional[bool] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        tbl_ref = self._ensure_table_ref(table)
        res = self.call(
            "plot.box",
            table=tbl_ref,
            cols=cols,
            notched=notched,
            whisker_mode=whisker_mode,
            show_points=show_points,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_violin(
        self,
        table: Union[str, TableProxy, Any],
        cols: Optional[Sequence[Union[int, str]]] = None,
        show_points: bool = True,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        tbl_ref = self._ensure_table_ref(table)
        res = self.call(
            "plot.violin",
            table=tbl_ref,
            cols=cols,
            show_points=show_points,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_pareto(
        self,
        table: Union[str, TableProxy, Any],
        col: Optional[Union[int, str]] = None,
        value_col: Optional[Union[int, str]] = None,
        *,
        category_col: Optional[Union[int, str]] = None,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        tbl_ref = self._ensure_table_ref(table)
        if value_col is not None:
            eff_cat = category_col if category_col is not None else col
            eff_val: Optional[Union[int, str]] = value_col
        else:
            eff_cat = category_col
            eff_val = col
        res = self.call(
            "plot.pareto",
            table=tbl_ref,
            col=eff_val,
            value_col=eff_val,
            category_col=eff_cat,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_qq(
        self,
        table: Union[str, TableProxy, Any],
        col: Optional[Union[int, str]] = None,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        tbl_ref = self._ensure_table_ref(table)
        res = self.call(
            "plot.qq",
            table=tbl_ref,
            col=col,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_pie(
        self,
        table: Union[str, TableProxy, Any],
        col: Optional[Union[int, str]] = None,
        graph_name: Optional[str] = None,
        *,
        label_col: Optional[Union[int, str]] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        tbl_ref = self._ensure_table_ref(table)
        res = self.call(
            "plot.pie",
            table=tbl_ref,
            col=col,
            label_col=label_col,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_vectors(
        self,
        table: Union[str, TableProxy, Any],
        cols: Optional[Union[int, str, Sequence[Union[int, str]]]] = None,
        y_col: Optional[Union[int, str]] = None,
        u_col: Optional[Union[int, str]] = None,
        v_col: Optional[Union[int, str]] = None,
        *,
        x_col: Optional[Union[int, str]] = None,
        mode: str = "xyxy",
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        tbl_ref = self._ensure_table_ref(table)
        eff_mode = mode
        resolved_cols: Optional[List[Union[int, str]]] = None
        if isinstance(cols, (list, tuple)):
            resolved_cols = list(cols)
            if isinstance(y_col, str) and u_col is None and v_col is None and y_col.lower() in ("xyxy", "xyam"):
                eff_mode = y_col
        elif cols is not None and y_col is not None and u_col is not None and v_col is not None:
            resolved_cols = [cols, y_col, u_col, v_col]
        elif x_col is not None and y_col is not None and u_col is not None and v_col is not None:
            resolved_cols = [x_col, y_col, u_col, v_col]

        res = self.call(
            "plot.vectors",
            table=tbl_ref,
            cols=resolved_cols,
            mode=eff_mode,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_ternary(
        self,
        table: Union[str, TableProxy, Any],
        a_col: Union[int, str] = 0,
        b_col: Union[int, str] = 1,
        c_col: Union[int, str] = 2,
        *,
        cols: Optional[Sequence[Union[int, str]]] = None,
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        tbl_ref = self._ensure_table_ref(table)
        chosen_cols = list(cols) if cols is not None else [a_col, b_col, c_col]
        res = self.call(
            "plot.ternary",
            table=tbl_ref,
            cols=chosen_cols,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    def plot_function(
        self,
        formula: Union[str, Sequence[str]],
        x_min: float = 0.0,
        x_max: float = 10.0,
        points: int = 200,
        graph_name: Optional[str] = None,
        *,
        type: Optional[str] = None,  # noqa: A002
        variable: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        res = self.call(
            "plot.function",
            formula=(list(formula) if isinstance(formula, (list, tuple)) else formula),
            x_min=x_min,
            x_max=x_max,
            points=points,
            type=type,
            variable=variable,
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

    def plot_heatmap(
        self,
        matrix: Union[str, MatrixProxy],
        style: str = "colormap",
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        return self.plot_matrix(matrix, style=style, graph_name=graph_name, **kwargs)

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
        table: Union[str, TableProxy, Any],
        x_col: Union[int, str] = 0,
        y_col: Union[int, str] = 1,
        z_col: Union[int, str] = 2,
        style: str = "scatter",
        graph_name: Optional[str] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        tbl_ref = self._ensure_table_ref(table)
        res = self.call(
            "plot.3d_xyz",
            table=tbl_ref,
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
        *,
        z_min: Optional[float] = None,
        z_max: Optional[float] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        """Create a 3D surface ``z = f(x, y)`` over ``[x_min, x_max] x [y_min, y_max]``.

        ``z_min`` / ``z_max`` set the Z range; the surface is clipped there. A bound left at
        ``None`` is the range of the function on the surface grid (widened a little when the
        function is constant), so e.g. ``50*x*y`` on ``[0, 1]^2`` gets Z ``0..50`` instead of
        being cut flat.
        """
        res = self.call(
            "plot.3d_function",
            formula=formula,
            x_min=x_min,
            x_max=x_max,
            y_min=y_min,
            y_max=y_max,
            z_min=z_min,
            z_max=z_max,
            graph_name=graph_name,
            **kwargs,
        )
        return GraphProxy(self, res["name"])

    # ------------------------------------------------------------------
    # 5. Graph Layout, Code Export & PowerPoint Native OLE Deck (`to_pptx`)
    # ------------------------------------------------------------------
    def auto_layout(
        self,
        graph: Optional[Union[str, GraphProxy]] = None,
        *,
        pad_fraction: float = 0.05,
        allow_outside_legend: bool = True,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.call(
            "graph.auto_layout",
            graph=_name_of(graph),
            pad_fraction=float(pad_fraction),
            allow_outside_legend=bool(allow_outside_legend),
            **kwargs,
        )

    fix_layout = _deprecated_alias("auto_layout", "fix_layout")

    def set_category_labels(
        self,
        graph: Union[str, GraphProxy],
        axis_or_labels: Union[str, Sequence[Any]] = "x",
        labels: Optional[Sequence[Any]] = None,
        *,
        layer: Optional[int] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return GraphProxy(self, str(_name_of(graph))).set_category_labels(
            axis_or_labels, labels, layer=layer, **kwargs
        )

    def export_ole(
        self,
        graph: Optional[Union[str, GraphProxy]] = None,
        emf_path: Optional[Union[str, os.PathLike[str]]] = None,
        *,
        dpi: int = 300,
        include_base64: Optional[bool] = None,
        skip_project_stream: Optional[bool] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.call(
            "graph.export_ole",
            graph=_name_of(graph),
            emf_path=(os.fspath(emf_path) if emf_path is not None else None),
            dpi=int(dpi),
            include_base64=include_base64,
            skip_project_stream=skip_project_stream,
            **kwargs,
        )

    def to_python(
        self,
        graph: Optional[Union[str, GraphProxy]] = None,
        output_path: Optional[Union[str, os.PathLike[str]]] = None,
        *,
        standalone: bool = True,
        embed_data: str = "inline",
        path: Optional[Union[str, os.PathLike[str]]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        out_p = output_path if output_path is not None else path
        out_str = os.fspath(out_p) if out_p is not None else None
        res = self.call(
            "graph.to_python",
            graph=_name_of(graph),
            standalone=bool(standalone),
            embed_data=str(embed_data),
            output_path=out_str,
            **kwargs,
        )
        script_txt = str(res.get("script") or res.get("code") or "")
        if out_str and script_txt and not os.path.isfile(out_str):
            Path(out_str).parent.mkdir(parents=True, exist_ok=True)
            Path(out_str).write_text(script_txt, encoding="utf-8")
        return res

    def to_python_script(
        self,
        graph: Optional[Union[str, GraphProxy]] = None,
        output_path: Optional[Union[str, os.PathLike[str]]] = None,
        *,
        standalone: bool = True,
        embed_data: str = "inline",
        path: Optional[Union[str, os.PathLike[str]]] = None,
        **kwargs: Any,
    ) -> str:
        res = self.to_python(
            graph=graph,
            output_path=output_path,
            standalone=standalone,
            embed_data=embed_data,
            path=path,
            **kwargs,
        )
        return str(res.get("script") or res.get("code") or "")

    export_python_script = _deprecated_alias("to_python_script", "export_python_script")

    def to_pptx(
        self,
        path: Union[str, os.PathLike[str]],
        graphs: Optional[Union[str, GraphProxy, Sequence[Union[str, GraphProxy]]]] = None,
        *,
        per_slide: int = 1,
        deck: Optional[Union[str, os.PathLike[str]]] = None,
        slide: Optional[int] = None,
        box: Optional[Union[Sequence[float], Sequence[Sequence[float]]]] = None,
        replace: Optional[Union[bool, str, Sequence[Any], Mapping[str, Any]]] = None,
        dpi: int = 300,
        overwrite: bool = False,
        titles: Optional[Union[str, Sequence[str]]] = None,
        preset: Optional[str] = None,
        widescreen: bool = True,
        scale: Optional[Union[float, int, str]] = None,
        embed_scope: str = "graph",
        allow_whole_project: bool = False,
    ) -> Dict[str, Any]:
        """Export graphs into a PowerPoint ``.pptx`` deck as editable native ``ToriGraph.Graph.1`` OLE objects.

        ``scale`` (e.g. ``1.4`` or ``"140%"``) places each graph at its natural size x scale;
        the result reports ``effective_scale`` and the effective axis-title / tick-label pt of
        every placement. ``embed_scope="graph"`` (default) embeds only the graph and the tables
        it plots; ``"project"`` embeds the whole project. Unless ``allow_whole_project=True``,
        failing to obtain a per-graph project raises :class:`ToriGraphError` instead of silently
        embedding the whole project. An existing ``path`` is never overwritten unless
        ``overwrite=True``.

        Uses OpenXML (``python-pptx``) + Win32 ``ole32.dll`` ``StgCreateDocfile`` with zero COM
        process activation (``POWERPNT.EXE`` is never spawned or touched) and zero clipboard use.
        """
        from .pptx import export_graphs_to_pptx

        return export_graphs_to_pptx(
            self,
            path=path,
            graphs=graphs,
            per_slide=per_slide,
            deck=deck,
            slide=slide,
            box=box,
            replace=replace,
            dpi=dpi,
            overwrite=overwrite,
            titles=titles,
            preset=preset,
            widescreen=widescreen,
            scale=scale,
            embed_scope=embed_scope,
            allow_whole_project=allow_whole_project,
        )

    # ------------------------------------------------------------------
    # 6. Curve Fitting & Peak Analysis
    # ------------------------------------------------------------------
    def list_fit_models(self, category: Optional[str] = None) -> List[Dict[str, Any]]:
        res = self.call("analysis.list_fit_models", category=category)
        return list(res.get("models", []))

    def fit_linear(
        self,
        *,
        graph: Optional[Union[str, GraphProxy]] = None,
        curve: Optional[Union[int, str]] = None,
        table: Optional[Union[str, TableProxy]] = None,
        x_col: Optional[Union[int, str]] = None,
        y_col: Optional[Union[int, str]] = None,
        x_min: Optional[float] = None,
        x_max: Optional[float] = None,
        plot_curve: Optional[bool] = None,
        fit_label: Optional[str] = None,
        weighting: Optional[str] = None,
        scale_errors: Optional[bool] = None,
        apparent: Optional[bool] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Straight-line least-squares fit ``y = a + b x`` (RPC ``analysis.fit_linear``).

        Give the data either as a plotted curve (``graph`` + optional ``curve`` index or
        title; the active graph when omitted) or as worksheet columns (``table`` +
        ``x_col`` + ``y_col``; a graph is created when needed).

        Args:
            x_min, x_max: fit only the points with ``x_min <= x <= x_max`` (give both).
            plot_curve: draw the fitted line on the graph (default ``True``).
            fit_label: name of the fit curve / result entry.
            weighting: ``"none"`` (default), ``"instrumental"`` (1/error-bar^2),
                ``"statistical"``, ``"proportional"`` or ``"dataset"``.
            scale_errors: scale parameter errors by sqrt(reduced chi^2) (default ``True``).
            apparent: report the "apparent" fit statistics.
            **kwargs: further RPC parameters (see ``app.describe_api("analysis.fit_linear")``).

        Returns:
            dict with ``slope``, ``intercept``, ``slope_se``, ``intercept_se``, ``r2``,
            ``adj_r2``, ``pearson_r``, ``p_value`` and ``n``.
        """
        return self.call(
            "analysis.fit_linear",
            graph=_name_of(graph),
            curve=curve,
            table=_name_of(table),
            x_col=x_col,
            y_col=y_col,
            x_min=x_min,
            x_max=x_max,
            plot_curve=plot_curve,
            fit_label=fit_label,
            weighting=weighting,
            scale_errors=scale_errors,
            apparent=apparent,
            **kwargs,
        )

    def fit_york(
        self,
        table: Optional[Union[str, TableProxy]] = None,
        x_col: Optional[Union[int, str]] = None,
        y_col: Optional[Union[int, str]] = None,
        x_err_col: Optional[Union[int, str]] = None,
        y_err_col: Optional[Union[int, str]] = None,
        *,
        graph: Optional[Union[str, GraphProxy]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.call(
            "analysis.fit_york",
            table=_name_of(table),
            x_col=x_col,
            y_col=y_col,
            x_err_col=x_err_col,
            y_err_col=y_err_col,
            graph=_name_of(graph),
            **kwargs,
        )

    def fit_polynomial(self, order: int = 2, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.fit_polynomial", order=order, **kwargs)

    def fit_nonlinear(
        self,
        model: Optional[str] = None,
        *,
        formula: Optional[str] = None,
        param_names: Optional[Sequence[str]] = None,
        graph: Optional[Union[str, GraphProxy]] = None,
        curve: Optional[Union[int, str]] = None,
        table: Optional[Union[str, TableProxy]] = None,
        x_col: Optional[Union[int, str]] = None,
        y_col: Optional[Union[int, str]] = None,
        init_params: Optional[Mapping[str, float]] = None,
        bounds: Optional[Mapping[str, Sequence[Optional[float]]]] = None,
        fixed: Optional[Sequence[str]] = None,
        x_min: Optional[float] = None,
        x_max: Optional[float] = None,
        max_iter: Optional[int] = None,
        tolerance: Optional[float] = None,
        points: Optional[int] = None,
        weighting: Optional[str] = None,
        weight_col: Optional[Union[int, str]] = None,
        algorithm: Optional[str] = None,
        conf_band: Optional[bool] = None,
        plot_curve: Optional[bool] = None,
        fit_label: Optional[str] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Nonlinear least-squares fit (RPC ``analysis.fit_nonlinear``).

        Give exactly one of ``model`` (built-in: ``"Gauss"``, ``"GaussAmp"``,
        ``"Lorentz"``, ``"PseudoVoigt1"``, ``"Boltzmann"``, ``"Arrhenius"``,
        ``"Hollomon"``, ``"Voce"``, ...; ``app.list_fit_models()`` lists them) or
        ``formula`` (muParser expression in ``x``, e.g. ``"a*exp(-x/t)+c"``, with
        ``param_names`` when they cannot be inferred).

        Args:
            graph, curve: plotted curve to fit (active graph when omitted).
            table, x_col, y_col: fit worksheet columns instead of a plotted curve.
            init_params: initial values ``{"a": 1.0, ...}``.
            bounds: ``{"a": [lower, upper], ...}`` (``None`` for an open side).
            fixed: names of parameters held at their initial value.
            x_min, x_max: fit range in x.
            max_iter: iteration limit (default 1000); tolerance: default 1e-7.
            points: number of points of the drawn fit curve (default 200).
            weighting, weight_col: weighting method (``"none"``, ``"instrumental"``,
                ``"statistical"``, ``"proportional"``, ``"dataset"``) and, for ``"dataset"``,
                the weight column.
            algorithm: Levenberg-Marquardt (default), ``"lm_unscaled"`` or ``"simplex"``.
            conf_band: also draw the confidence band.
            plot_curve: draw the fitted curve (default ``True``).
            **kwargs: further RPC parameters (``app.describe_api("analysis.fit_nonlinear")``).

        Returns:
            dict with ``params`` / ``param_names`` / ``param_values``, ``errors``, ``chi2``,
            ``chi2_dof``, ``r2``, ``adj_r2``, ``aic``, ``bic``, ``converged`` and
            ``convergence_message``.
        """
        return self.call(
            "analysis.fit_nonlinear",
            model=model,
            formula=formula,
            param_names=(list(param_names) if param_names is not None else None),
            graph=_name_of(graph),
            curve=curve,
            table=_name_of(table),
            x_col=x_col,
            y_col=y_col,
            init_params=init_params,
            bounds=bounds,
            fixed=(list(fixed) if fixed is not None else None),
            x_min=x_min,
            x_max=x_max,
            max_iter=max_iter,
            tolerance=tolerance,
            points=points,
            weighting=weighting,
            weight_col=weight_col,
            algorithm=algorithm,
            conf_band=conf_band,
            plot_curve=plot_curve,
            fit_label=fit_label,
            **kwargs,
        )

    def fit_global(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.fit_global", **kwargs)

    def fit_piecewise(self, segments: int = 2, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.fit_piecewise", segments=segments, **kwargs)

    def fit_robust(self, method: str = "bisquare", **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.fit_robust", method=method, **kwargs)

    def fit_quantile(self, tau: float = 0.5, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.fit_quantile", tau=tau, **kwargs)

    def fit_logistic(
        self,
        table: Optional[Union[str, TableProxy]] = None,
        x_col: Optional[Union[int, str]] = None,
        y_col: Optional[Union[int, str]] = None,
        *,
        x_cols: Optional[Sequence[Union[int, str]]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        eff_x_cols = (
            list(x_cols)
            if x_cols is not None
            else ([x_col] if x_col is not None else None)
        )
        return self.call(
            "analysis.fit_logistic",
            table=_name_of(table),
            x_col=x_col,
            x_cols=eff_x_cols,
            y_col=y_col,
            **kwargs,
        )

    def fit_mlr(
        self,
        table: Optional[Union[str, TableProxy]] = None,
        x_cols: Optional[Sequence[Union[int, str]]] = None,
        y_col: Optional[Union[int, str]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.call(
            "analysis.fit_mlr",
            table=_name_of(table),
            x_cols=(list(x_cols) if x_cols is not None else None),
            y_col=y_col,
            **kwargs,
        )

    def fit_implicit_odr(
        self,
        shape: Optional[Union[str, TableProxy]] = None,
        x_col: Optional[Union[int, str]] = None,
        y_col: Optional[Union[int, str]] = None,
        *,
        table: Optional[Union[str, TableProxy]] = None,
        model: Optional[str] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        # Support both (shape="ellipse", graph=...) and (table, x_col, y_col, *, model="circle")
        eff_table = table
        eff_shape: Optional[str] = None
        if isinstance(shape, TableProxy) or (x_col is not None and y_col is not None and table is None):
            eff_table = shape
        elif isinstance(shape, str):
            eff_shape = shape
        chosen_model = (
            model
            if model is not None
            else (
                eff_shape
                if eff_shape is not None
                else ("custom" if kwargs.get("formula") else "circle")
            )
        )
        return self.call(
            "analysis.fit_implicit_odr",
            table=_name_of(eff_table),
            x_col=x_col,
            y_col=y_col,
            model=chosen_model,
            **kwargs,
        )

    def fit_surface_3d(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.fit_surface_3d", **kwargs)

    def compare_models(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.compare_models", **kwargs)

    def compare_datasets(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.compare_datasets", **kwargs)

    def analyze_peaks(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("analysis.analyze_peaks", **kwargs)

    def batch_peaks(
        self,
        table: Optional[Union[str, TableProxy]] = None,
        x_col: Optional[Union[int, str]] = None,
        y_cols: Optional[Sequence[Union[int, str]]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.call(
            "analysis.batch_peaks",
            table=_name_of(table),
            x_col=x_col,
            y_cols=(list(y_cols) if y_cols is not None else None),
            **kwargs,
        )

    # ------------------------------------------------------------------
    # 7. Gadgets, Signal Processing, Curve Math, MatSci, Statistics & Digitizer
    # ------------------------------------------------------------------
    def attach_gadget(
        self,
        graph: Optional[Union[str, GraphProxy]] = None,
        gadget_type: Optional[str] = None,
        *,
        type: Optional[str] = None,  # noqa: A002
        **kwargs: Any,
    ) -> Dict[str, Any]:
        eff_type = type if type is not None else (gadget_type if gadget_type is not None else "roi_stats")
        return self.call("gadget.attach", graph=_name_of(graph), type=eff_type, **kwargs)

    def set_gadget_roi(
        self,
        graph: Optional[Union[str, GraphProxy]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.call("gadget.set_roi", graph=_name_of(graph), **kwargs)

    def query_gadget(
        self,
        graph: Optional[Union[str, GraphProxy]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.call("gadget.query", graph=_name_of(graph), **kwargs)

    def clear_gadgets(
        self,
        graph: Optional[Union[str, GraphProxy]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.call("gadget.clear", graph=_name_of(graph), **kwargs)

    clear_gadget = _deprecated_alias("clear_gadgets", "clear_gadget")

    def smooth(
        self,
        method: str = "savgol",
        *,
        graph: Optional[Union[str, GraphProxy]] = None,
        curve: Optional[Union[int, str]] = None,
        table: Optional[Union[str, TableProxy]] = None,
        x_col: Optional[Union[int, str]] = None,
        y_col: Optional[Union[int, str]] = None,
        window: Optional[int] = None,
        poly_order: Optional[int] = None,
        span: Optional[float] = None,
        lambda_: Optional[float] = None,
        sigma: Optional[float] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Smooth a curve and add the smoothed curve (RPC ``signal.smooth``).

        Args:
            method: ``"savgol"`` (Savitzky-Golay, default), ``"average"`` (moving
                average), ``"median"``, ``"fft"``, ``"lowess"``, ``"whittaker"`` or
                ``"gaussian"``.
            graph, curve: curve to smooth (active graph when omitted).
            table, x_col, y_col: smooth worksheet columns instead.
            window: window length in points (default ``min(7, n)``).
            poly_order: Savitzky-Golay polynomial order (default 2).
            span: LOWESS span as a fraction of the points (default 0.2).
            lambda_: Whittaker smoothing parameter (sent as ``lambda``; default 100).
            sigma: Gaussian kernel width in points.
            **kwargs: further RPC parameters (``app.describe_api("signal.smooth")``).
        """
        if lambda_ is not None and "lambda" not in kwargs:
            kwargs["lambda"] = lambda_
        return self.call(
            "signal.smooth",
            method=method,
            graph=_name_of(graph),
            curve=curve,
            table=_name_of(table),
            x_col=x_col,
            y_col=y_col,
            window=window,
            poly_order=poly_order,
            span=span,
            sigma=sigma,
            **kwargs,
        )

    def savgol_derivative(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.savgol_derivative", **kwargs)

    def fft(
        self,
        *,
        graph: Optional[Union[str, GraphProxy]] = None,
        curve: Optional[Union[int, str]] = None,
        table: Optional[Union[str, TableProxy]] = None,
        real_col: Optional[Union[int, str]] = None,
        imag_col: Optional[Union[int, str]] = None,
        sampling: Optional[float] = None,
        inverse: Optional[bool] = None,
        normalize: Optional[bool] = None,
        shift: Optional[bool] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """1-D forward or inverse FFT (RPC ``signal.fft``).

        Args:
            graph, curve: transform a plotted curve (active graph when omitted).
            table, real_col, imag_col: transform worksheet columns instead.
            sampling: sampling interval (``dt``); taken from the X spacing when omitted.
            inverse: inverse transform (default ``False``).
            normalize: normalise the amplitudes (default ``True``).
            shift: put zero frequency in the centre (default ``False``).
            **kwargs: further RPC parameters (``app.describe_api("signal.fft")``).
        """
        return self.call(
            "signal.fft",
            graph=_name_of(graph),
            curve=curve,
            table=_name_of(table),
            real_col=real_col,
            imag_col=imag_col,
            sampling=sampling,
            inverse=inverse,
            normalize=normalize,
            shift=shift,
            **kwargs,
        )

    def fft_filter(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.fft_filter", **kwargs)

    def iir_filter(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.iir_filter", **kwargs)

    def stft(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.stft", **kwargs)

    def envelope(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.envelope", **kwargs)

    hilbert_envelope = _deprecated_alias("envelope", "hilbert_envelope")

    def wavelet_cwt(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.wavelet_cwt", **kwargs)

    def wavelet_denoise(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.wavelet_denoise", **kwargs)

    def welch_psd(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.welch_psd", **kwargs)

    def coherence(
        self,
        table: Optional[Union[str, TableProxy]] = None,
        x_col: Optional[Union[int, str]] = None,
        y1_col: Optional[Union[int, str]] = None,
        y2_col: Optional[Union[int, str]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.call(
            "signal.coherence",
            table=_name_of(table),
            x_col=x_col,
            y1_col=y1_col,
            y2_col=y2_col,
            **kwargs,
        )

    def decimate(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.decimate", **kwargs)

    def fsd(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("signal.fsd", **kwargs)

    fourier_self_deconvolution = _deprecated_alias("fsd", "fourier_self_deconvolution")

    def curve_eval(
        self,
        graph_or_op: Optional[Union[str, GraphProxy]] = None,
        curve_a: Optional[Union[int, str]] = None,
        curve_b: Optional[Union[int, str]] = None,
        *,
        graph: Optional[Union[str, GraphProxy]] = None,
        op: Optional[str] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        # Support both curve_eval(op="-", graph=..., curve_a=..., curve_b=...)
        # and curve_math(g, 'Sample', 'Blank', op='sub')
        if curve_a is not None or curve_b is not None or isinstance(graph_or_op, GraphProxy):
            eff_graph = graph if graph is not None else graph_or_op
            eff_op = op if op is not None else "-"
        elif isinstance(graph_or_op, str) and graph_or_op in ("+", "-", "*", "/", "add", "sub", "mul", "div", "subtract", "multiply", "divide"):
            eff_graph = graph
            eff_op = op if op is not None else graph_or_op
        else:
            eff_graph = graph if graph is not None else graph_or_op
            eff_op = op if op is not None else "-"
        return self.call(
            "curvemath.eval",
            graph=_name_of(eff_graph),
            curve_a=curve_a,
            curve_b=curve_b,
            op=eff_op,
            **kwargs,
        )

    curve_math = _deprecated_alias("curve_eval", "curve_math")
    eval_curves = _deprecated_alias("curve_eval", "eval_curves")

    def average_curves(
        self,
        graph: Optional[Union[str, GraphProxy]] = None,
        *,
        curves: Optional[Sequence[Union[int, str]]] = None,
        points: Optional[int] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.call(
            "curvemath.average_curves",
            graph=_name_of(graph),
            curves=(list(curves) if curves is not None else None),
            points=points,
            **kwargs,
        )

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

    subtract_straight_line = _deprecated_alias("subtract_line", "subtract_straight_line")

    def intersections(
        self,
        graph: Optional[Union[str, GraphProxy]] = None,
        curve_a: Optional[Union[int, str]] = None,
        curve_b: Optional[Union[int, str]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.call(
            "curvemath.intersections",
            graph=_name_of(graph),
            curve_a=curve_a,
            curve_b=curve_b,
            **kwargs,
        )

    curve_intersections = _deprecated_alias("intersections", "curve_intersections")
    find_intersections = _deprecated_alias("intersections", "find_intersections")

    def lookup_xy(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("curvemath.lookup_xy", **kwargs)

    def polygon_area(self, **kwargs: Any) -> Dict[str, Any]:
        return self.call("curvemath.polygon_area", **kwargs)

    def fill_between(
        self,
        graph: Optional[Union[str, GraphProxy]] = None,
        curve_a: Optional[Union[int, str]] = None,
        curve_b: Optional[Union[int, str]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.call(
            "curvemath.fill_between",
            graph=_name_of(graph),
            curve_a=curve_a,
            curve_b=curve_b,
            **kwargs,
        )

    # ------------------------------------------------------------------
    # Materials Science & Electronic Structure Workbench (`curvemath.*`)
    # ------------------------------------------------------------------
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

    offset_yield = _deprecated_alias("analyze_tensile", "offset_yield")

    def analyze_work_hardening(
        self,
        graph: Optional[Union[str, GraphProxy]] = None,
        curve: Optional[Union[int, str]] = None,
        *,
        table: Optional[Union[str, TableProxy]] = None,
        smooth_window: int = 11,
        create_plot: bool = True,
        plot_mode: Optional[str] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Compute Kocks-Mecking work-hardening rate (d sigma_t / d eps_t) & Considere necking criterion."""
        return self.call(
            "curvemath.kocks_mecking",
            graph=_name_of(graph),
            curve=curve,
            table=_name_of(table),
            smooth_window=int(smooth_window),
            create_plot=bool(create_plot),
            plot_mode=plot_mode,
            **kwargs,
        )

    kocks_mecking = _deprecated_alias("analyze_work_hardening", "kocks_mecking")

    def analyze_kissinger(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        rate_col: Optional[Union[int, str]] = None,
        temp_col: Optional[Union[int, str, Sequence[Union[int, str]]]] = None,
        *,
        heating_rates: Optional[Sequence[Any]] = None,
        peak_temps: Optional[Sequence[float]] = None,
        rate_in_k_per_hour: bool = False,
        temp_in_celsius: bool = True,
        top_axis_in_celsius: bool = True,
        temp_min: Optional[float] = None,
        temp_max: Optional[float] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Run Kissinger thermal desorption / DSC activation energy analysis (`curvemath.kissinger`)."""
        eff_rates = heating_rates
        eff_table_ref: Optional[str] = None
        if isinstance(table, (list, tuple)) and eff_rates is None:
            eff_rates = table
        elif table is not None:
            eff_table_ref = self._ensure_table_ref(table)

        return self.call(
            "curvemath.kissinger",
            table=eff_table_ref,
            rate_col=rate_col,
            temp_col=temp_col,
            heating_rates=(list(eff_rates) if eff_rates is not None else None),
            peak_temps=(list(peak_temps) if peak_temps is not None else None),
            rate_in_k_per_hour=bool(rate_in_k_per_hour),
            temp_in_celsius=bool(temp_in_celsius),
            top_axis_in_celsius=bool(top_axis_in_celsius),
            temp_min=temp_min,
            temp_max=temp_max,
            **kwargs,
        )

    kissinger = _deprecated_alias("analyze_kissinger", "kissinger")
    kissinger_plot = _deprecated_alias("analyze_kissinger", "kissinger_plot")

    def analyze_xrd(
        self,
        graph: Optional[Union[str, GraphProxy]] = None,
        curve: Optional[Union[int, str]] = None,
        *,
        wavelength: float = 1.540598,
        xray_energy_kev: Optional[float] = None,
        instr_fwhm: float = 0.05,
        scherrer_k: float = 0.90,
        crystal_system: str = "fcc",
        c_over_a: Optional[float] = None,
        strip_kalpha2: bool = False,
        add_top_d_axis: bool = True,
        create_wh_plot: bool = True,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Run XRD Bragg d-spacing, Scherrer crystallite size, and Williamson-Hall microstrain analysis."""
        return self.call(
            "curvemath.xrd_analyze",
            graph=_name_of(graph),
            curve=curve,
            wavelength=float(wavelength),
            xray_energy_kev=xray_energy_kev,
            instr_fwhm=float(instr_fwhm),
            scherrer_k=float(scherrer_k),
            crystal_system=str(crystal_system),
            c_over_a=c_over_a,
            strip_kalpha2=bool(strip_kalpha2),
            add_top_d_axis=bool(add_top_d_axis),
            create_wh_plot=bool(create_wh_plot),
            **kwargs,
        )

    xrd_analyze = _deprecated_alias("analyze_xrd", "xrd_analyze")

    def bragg_convert(
        self,
        graph: Optional[Union[str, GraphProxy]] = None,
        *,
        wavelength: float = 1.540598,
        xray_energy_kev: Optional[float] = None,
        mode: str = "d_spacing",
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Convert XRD 2-theta axis/curve to Bragg d-spacing (`mode='d_spacing'`) or Q (`mode='q'`)."""
        return self.call(
            "curvemath.bragg_convert",
            graph=_name_of(graph),
            wavelength=float(wavelength),
            xray_energy_kev=xray_energy_kev,
            mode=str(mode),
            **kwargs,
        )

    def plot_vasp_dos(
        self,
        table: Optional[Union[str, TableProxy, os.PathLike[str], Any]] = None,
        energy_col: Optional[Union[int, str]] = None,
        dos_cols: Optional[Union[int, str, Sequence[Union[int, str]]]] = None,
        *,
        doscar_path: Optional[Union[str, os.PathLike[str]]] = None,
        down_dos_cols: Optional[Union[int, str, Sequence[Union[int, str]]]] = None,
        fermi_energy: float = 0.0,
        vertical_energy: bool = False,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Plot VASP / DFT total or projected Density of States (`curvemath.vasp_dos`)."""
        eff_doscar: Optional[str] = os.fspath(doscar_path) if doscar_path is not None else None
        eff_table: Optional[str] = None
        if table is not None:
            if eff_doscar is None and (
                isinstance(table, os.PathLike)
                or (
                    isinstance(table, str)
                    and (os.path.isfile(table) or "/" in table or "\\" in table)
                )
            ):
                eff_doscar = os.fspath(table)
            else:
                eff_table = self._ensure_table_ref(table)

        norm_dos: Optional[List[Union[int, str]]] = None
        if dos_cols is not None:
            norm_dos = [dos_cols] if isinstance(dos_cols, (int, str)) else list(dos_cols)
        norm_down: Optional[List[Union[int, str]]] = None
        if down_dos_cols is not None:
            norm_down = [down_dos_cols] if isinstance(down_dos_cols, (int, str)) else list(down_dos_cols)

        return self.call(
            "curvemath.vasp_dos",
            table=eff_table,
            doscar_path=eff_doscar,
            energy_col=energy_col,
            dos_cols=norm_dos,
            down_dos_cols=norm_down,
            fermi_energy=float(fermi_energy),
            vertical_energy=bool(vertical_energy),
            **kwargs,
        )

    vasp_dos = _deprecated_alias("plot_vasp_dos", "vasp_dos")
    plot_dos = _deprecated_alias("plot_vasp_dos", "plot_dos")

    def plot_vasp_bands(
        self,
        table: Optional[Union[str, TableProxy, os.PathLike[str], Any]] = None,
        k_col: Optional[Union[int, str]] = None,
        band_cols: Optional[Union[int, str, Sequence[Union[int, str]]]] = None,
        *,
        eigenval_path: Optional[Union[str, os.PathLike[str]]] = None,
        k_ticks: Optional[Sequence[Any]] = None,
        k_labels: Optional[Sequence[str]] = None,
        fermi_energy: float = 0.0,
        e_min: float = -6.0,
        e_max: float = 6.0,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Plot VASP / DFT electronic band structure with high-symmetry k-ticks (`curvemath.vasp_bands`)."""
        eff_eigenval: Optional[str] = os.fspath(eigenval_path) if eigenval_path is not None else None
        eff_table: Optional[str] = None
        if table is not None:
            if eff_eigenval is None and (
                isinstance(table, os.PathLike)
                or (
                    isinstance(table, str)
                    and (os.path.isfile(table) or "/" in table or "\\" in table)
                )
            ):
                eff_eigenval = os.fspath(table)
            else:
                eff_table = self._ensure_table_ref(table)

        norm_bands: Optional[List[Union[int, str]]] = None
        if band_cols is not None:
            norm_bands = [band_cols] if isinstance(band_cols, (int, str)) else list(band_cols)

        eff_kticks: Optional[List[float]] = None
        eff_klabels: Optional[List[str]] = [str(lbl) for lbl in k_labels] if k_labels is not None else None
        if k_ticks is not None:
            kt_list = list(k_ticks)
            if kt_list and isinstance(kt_list[0], (list, tuple)) and len(kt_list[0]) >= 2:
                eff_kticks = [float(pair[0]) for pair in kt_list]
                if eff_klabels is None:
                    eff_klabels = [str(pair[1]) for pair in kt_list]
            else:
                eff_kticks = [float(x) for x in _coerce_1d_sequence(kt_list)]

        return self.call(
            "curvemath.vasp_bands",
            table=eff_table,
            eigenval_path=eff_eigenval,
            k_col=k_col,
            band_cols=norm_bands,
            k_ticks=eff_kticks,
            k_labels=eff_klabels,
            fermi_energy=float(fermi_energy),
            e_min=float(e_min),
            e_max=float(e_max),
            **kwargs,
        )

    vasp_bands = _deprecated_alias("plot_vasp_bands", "vasp_bands")
    plot_bands = _deprecated_alias("plot_vasp_bands", "plot_bands")

    def plot_vasp_band_dos(
        self,
        band_table: Optional[Union[str, TableProxy, Any]] = None,
        dos_table: Optional[Union[str, TableProxy, Any]] = None,
        *,
        eigenval_path: Optional[Union[str, Path]] = None,
        doscar_path: Optional[Union[str, Path]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Plot coupled 2-panel Band Structure + vertical-energy DOS figure."""
        params: Dict[str, Any] = dict(kwargs)
        if band_table is not None:
            params["band_table"] = self._ensure_table_ref(band_table)
        elif "table" in params and params["table"] is not None:
            params["table"] = self._ensure_table_ref(params["table"])
        if dos_table is not None:
            params["dos_table"] = self._ensure_table_ref(dos_table)
        if eigenval_path is not None:
            params["eigenval_path"] = str(Path(eigenval_path).resolve())
        if doscar_path is not None:
            params["doscar_path"] = str(Path(doscar_path).resolve())
        return self.call("curvemath.vasp_band_dos", **params)

    vasp_band_dos = _deprecated_alias("plot_vasp_band_dos", "vasp_band_dos")
    plot_band_dos = _deprecated_alias("plot_vasp_band_dos", "plot_band_dos")

    def arrhenius(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        temp_col: Optional[Union[int, str]] = None,
        rate_col: Optional[Union[int, str]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        return self.call(
            "curvemath.arrhenius",
            table=tbl_ref,
            temp_col=temp_col,
            rate_col=rate_col,
            **kwargs,
        )

    arrhenius_plot = _deprecated_alias("arrhenius", "arrhenius_plot")

    def tauc(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        x_col: Optional[Union[int, str]] = None,
        alpha_col: Optional[Union[int, str]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        return self.call(
            "curvemath.tauc",
            table=tbl_ref,
            x_col=x_col,
            alpha_col=alpha_col,
            **kwargs,
        )

    tauc_bandgap = _deprecated_alias("tauc", "tauc_bandgap")

    def eis_fit(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        freq_col: Optional[Union[int, str]] = None,
        z_re_col: Optional[Union[int, str]] = None,
        z_im_col: Optional[Union[int, str]] = None,
        *,
        z_real_col: Optional[Union[int, str]] = None,
        z_imag_col: Optional[Union[int, str]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        eff_re = z_real_col if z_real_col is not None else z_re_col
        eff_im = z_imag_col if z_imag_col is not None else z_im_col
        return self.call(
            "curvemath.eis_fit",
            table=tbl_ref,
            freq_col=freq_col,
            z_real_col=eff_re,
            z_imag_col=eff_im,
            **kwargs,
        )

    # ------------------------------------------------------------------
    # Statistics (`stats.*`) & Digitizer
    # ------------------------------------------------------------------
    def normality(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        col: Optional[Union[int, str]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        return self.call("stats.normality", table=tbl_ref, col=col, **kwargs)

    normality_test = _deprecated_alias("normality", "normality_test")

    def outlier(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        col: Optional[Union[int, str]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        return self.call("stats.outlier", table=tbl_ref, col=col, **kwargs)

    outlier_test = _deprecated_alias("outlier", "outlier_test")

    def anova_oneway(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        cols: Optional[Sequence[Union[int, str]]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        return self.call(
            "stats.anova_oneway",
            table=tbl_ref,
            cols=(list(cols) if cols is not None else None),
            **kwargs,
        )

    anova_one_way = _deprecated_alias("anova_oneway", "anova_one_way")

    def anova_twoway(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        factor_a: Optional[Union[int, str]] = None,
        factor_b: Optional[Union[int, str]] = None,
        response: Optional[Union[int, str]] = None,
        *,
        factor_a_col: Optional[Union[int, str]] = None,
        factor_b_col: Optional[Union[int, str]] = None,
        response_col: Optional[Union[int, str]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        eff_a = factor_a if factor_a is not None else factor_a_col
        eff_b = factor_b if factor_b is not None else factor_b_col
        eff_r = response if response is not None else response_col
        return self.call(
            "stats.anova_twoway",
            table=tbl_ref,
            factor_a=eff_a,
            factor_b=eff_b,
            response=eff_r,
            **kwargs,
        )

    anova_two_way = _deprecated_alias("anova_twoway", "anova_two_way")

    def nonparametric(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        cols: Optional[Sequence[Union[int, str]]] = None,
        *,
        test: Optional[str] = None,
        graph: Optional[Union[str, GraphProxy]] = None,
        add_brackets: Optional[bool] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        return self.call(
            "stats.nonparametric",
            table=tbl_ref,
            cols=(list(cols) if cols is not None else None),
            test=test,
            graph=_name_of(graph),
            add_brackets=add_brackets,
            **kwargs,
        )

    nonparametric_test = _deprecated_alias("nonparametric", "nonparametric_test")

    def paired_plot(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        col_a: Optional[Union[int, str]] = None,
        col_b: Optional[Union[int, str]] = None,
        *,
        before_col: Optional[Union[int, str]] = None,
        after_col: Optional[Union[int, str]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        eff_a = before_col if before_col is not None else col_a
        eff_b = after_col if after_col is not None else col_b
        return self.call(
            "stats.paired_plot",
            table=tbl_ref,
            before_col=eff_a,
            after_col=eff_b,
            col_a=eff_a,
            col_b=eff_b,
            **kwargs,
        )

    paired_estimation_plot = _deprecated_alias("paired_plot", "paired_estimation_plot")

    def bland_altman(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        col_a: Optional[Union[int, str]] = None,
        col_b: Optional[Union[int, str]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        return self.call(
            "stats.bland_altman",
            table=tbl_ref,
            col_a=col_a,
            col_b=col_b,
            **kwargs,
        )

    def passing_bablok(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        x_col: Optional[Union[int, str]] = None,
        y_col: Optional[Union[int, str]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        return self.call(
            "stats.passing_bablok",
            table=tbl_ref,
            x_col=x_col,
            y_col=y_col,
            **kwargs,
        )

    def weibull(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        col: Optional[Union[int, str]] = None,
        *,
        censor_col: Optional[Union[int, str]] = None,
        three_param: Optional[bool] = None,
        mle: Optional[bool] = None,
        plot: Optional[bool] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        return self.call(
            "stats.weibull",
            table=tbl_ref,
            col=col,
            censor_col=censor_col,
            three_param=three_param,
            mle=mle,
            plot=plot,
            **kwargs,
        )

    weibull_fit = _deprecated_alias("weibull", "weibull_fit")

    def probability_plot(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        col: Optional[Union[int, str]] = None,
        *,
        distribution: str = "lognormal",
        position: Optional[str] = None,
        return_period: Optional[float] = None,
        plot: Optional[bool] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        """Lognormal or Gumbel (extreme value) probability plot with a straight-line fit."""
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        return self.call(
            "stats.probability_plot",
            table=tbl_ref,
            col=col,
            distribution=distribution,
            position=position,
            return_period=return_period,
            plot=plot,
            **kwargs,
        )

    def plot_color_scatter(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        x: Optional[Union[int, str]] = None,
        y: Optional[Union[int, str]] = None,
        z: Optional[Union[int, str]] = None,
        *,
        size: Optional[Union[int, str]] = None,
        palette: Optional[str] = None,
        reverse: Optional[bool] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        """Scatter plot coloured by a Z column (Viridis by default) with a colour bar."""
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        res = self.call(
            "plot.color_scatter",
            table=tbl_ref,
            x=x,
            y=y,
            z=z,
            size=size,
            palette=palette,
            reverse=reverse,
            **kwargs,
        )
        return GraphProxy(self, res["graph"])

    def _graph_or_result(self, res: Dict[str, Any], *, always_graph: bool = False) -> Any:
        """Wrap a plot reply into a :class:`GraphProxy` (``proxy.result`` keeps the full reply).

        Analysis-only replies (``plot=False``) have no graph window; they stay a ``dict``
        unless ``always_graph`` is set.
        """
        gname = res.get("graph") if hasattr(res, "get") else None
        if not (isinstance(gname, str) and gname):
            gname = res.get("name") if (always_graph and hasattr(res, "get")) else None
        if isinstance(gname, str) and gname:
            proxy = GraphProxy(self, gname)
            proxy.result = res  # type: ignore[attr-defined]
            return proxy
        return res

    def plot_histogram_cumulative(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        col: Optional[Union[int, str]] = None,
        *,
        bin_width: Optional[float] = None,
        begin: Optional[float] = None,
        **kwargs: Any,
    ) -> GraphProxy:
        """Histogram with the cumulative percentage on the right axis.

        Returns a :class:`GraphProxy` like every other ``plot_*``; the server reply
        (bin table, ...) is available as ``proxy.result``.
        """
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        res = self.call(
            "plot.histogram_cumulative",
            table=tbl_ref,
            col=col,
            bin_width=bin_width,
            begin=begin,
            **kwargs,
        )
        return self._graph_or_result(res, always_graph=True)

    def plot_density2d(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        x: Optional[Union[int, str]] = None,
        y: Optional[Union[int, str]] = None,
        *,
        bins_x: Optional[int] = None,
        bins_y: Optional[int] = None,
        width_x: Optional[float] = None,
        width_y: Optional[float] = None,
        log: Optional[bool] = None,
        palette: Optional[str] = None,
        plot: Optional[bool] = None,
        **kwargs: Any,
    ) -> Any:
        """2D density (2D histogram) of two columns as a heat map; the count matrix is kept.

        Returns a :class:`GraphProxy` when a graph is plotted (``proxy.result`` holds the
        server reply); with ``plot=False`` only the analysis ``dict`` is returned.
        """
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        res = self.call(
            "plot.density2d",
            table=tbl_ref,
            x=x,
            y=y,
            bins_x=bins_x,
            bins_y=bins_y,
            width_x=width_x,
            width_y=width_y,
            log=log,
            palette=palette,
            plot=plot,
            **kwargs,
        )
        return self._graph_or_result(res)

    def plot_pole_figure(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        alpha: Optional[Union[int, str]] = None,
        beta: Optional[Union[int, str]] = None,
        intensity: Optional[Union[int, str]] = None,
        *,
        matrix: Optional[str] = None,
        projection: Optional[str] = None,
        mrd: Optional[bool] = None,
        alpha_from_rim: Optional[bool] = None,
        beta_zero_top: Optional[bool] = None,
        clockwise: Optional[bool] = None,
        rd: Optional[str] = None,
        grid: Optional[int] = None,
        levels: Optional[int] = None,
        palette: Optional[str] = None,
        label: Optional[str] = None,
        plot: Optional[bool] = None,
        **kwargs: Any,
    ) -> Any:
        """XRD pole figure (Schmidt equal-area or Wulff equal-angle) with contours and MRD.

        Returns a :class:`GraphProxy` when a graph is plotted (``proxy.result`` holds the
        server reply); with ``plot=False`` only the analysis ``dict`` is returned.
        """
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        res = self.call(
            "plot.pole_figure",
            table=tbl_ref,
            alpha=alpha,
            beta=beta,
            intensity=intensity,
            matrix=matrix,
            projection=projection,
            mrd=mrd,
            alpha_from_rim=alpha_from_rim,
            beta_zero_top=beta_zero_top,
            clockwise=clockwise,
            rd=rd,
            grid=grid,
            levels=levels,
            palette=palette,
            label=label,
            plot=plot,
            **kwargs,
        )
        return self._graph_or_result(res)

    def kaplan_meier(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        time_col: Optional[Union[int, str]] = None,
        event_col: Optional[Union[int, str]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        return self.call(
            "stats.kaplan_meier",
            table=tbl_ref,
            time_col=time_col,
            event_col=event_col,
            **kwargs,
        )

    def roc(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        state_col: Optional[Union[int, str]] = None,
        score_cols: Optional[Union[int, str, Sequence[Union[int, str]]]] = None,
        *,
        score_col: Optional[Union[int, str]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        eff_score_col = score_col
        eff_score_cols: Optional[List[Union[int, str]]] = None
        if score_cols is not None:
            if isinstance(score_cols, (int, str)):
                eff_score_col = eff_score_col if eff_score_col is not None else score_cols
            else:
                eff_score_cols = list(score_cols)
                if eff_score_col is None and len(eff_score_cols) == 1:
                    eff_score_col = eff_score_cols[0]
        return self.call(
            "stats.roc",
            table=tbl_ref,
            state_col=state_col,
            score_col=eff_score_col,
            score_cols=eff_score_cols,
            **kwargs,
        )

    roc_analysis = _deprecated_alias("roc", "roc_analysis")
    roc_curve = _deprecated_alias("roc", "roc_curve")

    def pca(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        cols: Optional[Sequence[Union[int, str]]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        return self.call(
            "stats.pca",
            table=tbl_ref,
            cols=(list(cols) if cols is not None else None),
            **kwargs,
        )

    def correlation(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        cols: Optional[Sequence[Union[int, str]]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        return self.call(
            "stats.correlation",
            table=tbl_ref,
            cols=(list(cols) if cols is not None else None),
            **kwargs,
        )

    correlation_matrix = _deprecated_alias("correlation", "correlation_matrix")

    def clustering(
        self,
        table: Optional[Union[str, TableProxy, Any]] = None,
        cols: Optional[Sequence[Union[int, str]]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        tbl_ref = self._ensure_table_ref(table) if table is not None else None
        return self.call(
            "stats.clustering",
            table=tbl_ref,
            cols=(list(cols) if cols is not None else None),
            **kwargs,
        )

    def confidence_ellipse(
        self,
        graph: Optional[Union[str, GraphProxy]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        return self.call("stats.confidence_ellipse", graph=_name_of(graph), **kwargs)

    add_confidence_ellipse = _deprecated_alias("confidence_ellipse", "add_confidence_ellipse")

    def digitize(
        self,
        path: Optional[Union[str, os.PathLike[str]]] = None,
        *,
        image_path: Optional[Union[str, os.PathLike[str]]] = None,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        eff_path = path if path is not None else image_path
        if eff_path is None:
            raise ValueError("Provide 'path' (or 'image_path') to image file.")
        return self.call("digitizer.extract", path=os.fspath(eff_path), **kwargs)

    extract_digitizer = _deprecated_alias("digitize", "extract_digitizer")
    digitize_image = _deprecated_alias("digitize", "digitize_image")

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

    if sys.platform == "win32":
        # Check Windows OLE ServerExecutable registry entry (written by installer & ToriGraph)
        try:
            import winreg

            for root in (winreg.HKEY_CURRENT_USER, winreg.HKEY_LOCAL_MACHINE):
                subkey = (
                    r"Software\Classes\CLSID"
                    r"\{8E6B9541-3C74-4D28-A915-7D84E5A12001}\LocalServer32"
                )
                try:
                    with winreg.OpenKey(root, subkey) as k:
                        val, _ = winreg.QueryValueEx(k, "ServerExecutable")
                        if val:
                            p = Path(str(val).strip('"')).resolve()
                            if p.is_file():
                                return p
                except OSError:
                    continue
        except Exception:
            pass

        # Check standard Windows installer locations (per-user and per-machine)
        win_dirs = [
            os.environ.get("LOCALAPPDATA", ""),
            os.environ.get("ProgramFiles", ""),
            os.environ.get("ProgramFiles(x86)", ""),
        ]
        for base in win_dirs:
            if not base:
                continue
            for rel in (
                "Programs/ToriGraph/ToriGraph.exe",
                "ToriGraph/ToriGraph.exe",
            ):
                cand = (Path(base) / rel).resolve()
                if cand.is_file():
                    return cand

    raise ToriGraphError(
        "Could not find ToriGraph executable. Pass exe_path=... or set TORIGRAPH_EXE."
    )


def _sdk_version() -> str:
    try:
        from . import __version__ as sdk_version  # local import: avoids a circular import

        return str(sdk_version)
    except Exception:
        return ""


def _version_tuple(text: str) -> Tuple[int, ...]:
    parts: List[int] = []
    for tok in str(text).strip().lstrip("vV").split("-")[0].split("."):
        if not tok.isdigit():
            break
        parts.append(int(tok))
    return tuple(parts)


def check_app_version(session: "Session", *, warn: bool = True) -> Dict[str, Any]:
    """Compare the connected ToriGraph version with this SDK's ``__version__``.

    Returns ``{"app_version", "sdk_version", "match"}``.  When the versions differ
    (or the application does not report one) a :class:`ToriGraphVersionWarning`
    is emitted if ``warn`` is true.  An RPC error from ``app.info`` is not raised.
    """
    sdk_ver = _sdk_version()
    app_ver = ""
    try:
        info = session.call("app.info", rpc_timeout=min(30.0, session.rpc_timeout))
        app_ver = str(info.get("version", ""))
    except RpcError:
        app_ver = ""
    match = bool(app_ver) and _version_tuple(app_ver) == _version_tuple(sdk_ver)
    if warn and not match:
        import warnings

        if app_ver:
            msg = (
                f"ToriGraph application version {app_ver} differs from the torigraph Python SDK "
                f"version {sdk_ver}. Some methods may be missing or behave differently; install "
                "the SDK that ships with the application (scripts/install-python-sdk.ps1)."
            )
        else:
            msg = (
                f"Could not read the ToriGraph application version (SDK {sdk_ver}); "
                "the application may be older than this SDK."
            )
        warnings.warn(msg, ToriGraphVersionWarning, stacklevel=3)
    return {"app_version": app_ver, "sdk_version": sdk_ver, "match": match}


def connect(
    endpoint: Optional[str] = None,
    timeout: float = 30.0,
    *,
    token: Optional[str] = None,
    rpc_timeout: Optional[float] = None,
    check_version: bool = True,
) -> Session:
    """Connect to a running ToriGraph JSON-RPC 2.0 server.

    ``timeout`` bounds opening the connection; ``rpc_timeout`` is the default wait
    for each RPC answer (``None`` -> :data:`DEFAULT_RPC_TIMEOUT`).  With
    ``check_version`` a :class:`ToriGraphVersionWarning` is issued when the
    application version differs from the SDK version.
    """
    resolved = (endpoint or os.environ.get("TORIGRAPH_RPC_ENDPOINT", "")).strip()
    if not resolved:
        raise TransportError(
            "No ToriGraph RPC endpoint specified and TORIGRAPH_RPC_ENDPOINT is not set. "
            "Pass the pipe://... / tcp://... endpoint shown in ToriGraph's Python console, "
            "or use tg.connect_or_launch() to start a headless ToriGraph."
        )
    transport = _parse_and_connect_transport(resolved, timeout=timeout, token=token)
    session = Session(transport, rpc_timeout=rpc_timeout)
    session._reconnect_info = (resolved, float(timeout), token)
    session.call("ping", rpc_timeout=max(1.0, float(timeout)))
    if check_version:
        check_app_version(session)
    return session


def connect_or_launch(
    endpoint: Optional[str] = None,
    *,
    token: Optional[str] = None,
    exe_path: Optional[Union[str, os.PathLike[str]]] = None,
    headless: bool = True,
    language: Optional[str] = None,
    timeout: float = 30.0,
    extra_args: Optional[Sequence[str]] = None,
    rpc_timeout: Optional[float] = None,
    check_version: bool = True,
) -> Session:
    """Connect to ``endpoint`` / ``TORIGRAPH_RPC_ENDPOINT`` if available, else launch headless ``ToriGraph``.

    ``language`` is used only when a new process is launched (see :func:`launch`);
    an existing session's UI language is never changed.
    """
    resolved = (endpoint or os.environ.get("TORIGRAPH_RPC_ENDPOINT", "")).strip()
    if resolved:
        return connect(
            resolved,
            timeout=timeout,
            token=token,
            rpc_timeout=rpc_timeout,
            check_version=check_version,
        )
    return launch(
        exe_path=exe_path,
        headless=headless,
        language=language,
        timeout=timeout,
        extra_args=extra_args,
        rpc_timeout=rpc_timeout,
        check_version=check_version,
    )


def launch(
    exe_path: Optional[Union[str, os.PathLike[str]]] = None,
    headless: bool = True,
    language: Optional[str] = None,
    timeout: float = 30.0,
    extra_args: Optional[Sequence[str]] = None,
    rpc_timeout: Optional[float] = None,
    check_version: bool = True,
) -> Session:
    """Launch a ToriGraph child process with loopback RPC enabled and connect a Session.

    ``timeout`` is the start-up wait (process start until the RPC endpoint answers);
    ``rpc_timeout`` is the default wait for each later RPC answer (``None`` ->
    :data:`DEFAULT_RPC_TIMEOUT`; override per call with ``call(..., rpc_timeout=)``).

    ``language`` (``"ja"`` / ``"en"``) switches the UI language of the launched
    process.  The default ``None`` leaves it as the user's ToriGraph setting
    (Japanese on a fresh install).  Pass it explicitly when the script depends on
    the language of generated labels.  Note that ToriGraph remembers the language
    it last used, so an explicit value can carry over to the next GUI start.
    """
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

    spawned_token_files: List[str] = []
    try:
        spawned_token: Optional[str] = None
        ep_clean = discovered_endpoint.split("?", 1)[0].strip()
        if ep_clean.startswith("tcp://") and ":" in ep_clean[6:]:
            port_part = ep_clean.rsplit(":", 1)[-1]
            if port_part.isdigit():
                port_num = int(port_part)
                spawned_token = _read_local_tcp_token_file(port_num)
                spawned_token_files = [
                    os.fspath(p) for p in _find_local_tcp_token_files(port_num)
                ]
        transport = _parse_and_connect_transport(
            discovered_endpoint, timeout=timeout, token=spawned_token
        )
        session = Session(
            transport,
            process=proc,
            cleanup_files=[port_file, err_file, *spawned_token_files],
            rpc_timeout=rpc_timeout,
        )
        session._reconnect_info = (discovered_endpoint, float(timeout), spawned_token)
        session.call("ping", rpc_timeout=max(1.0, float(timeout)))
        if check_version:
            check_app_version(session)
        if language:
            session.set_language(language)
        return session
    except Exception:
        try:
            proc.kill()
        except Exception:
            pass
        try:
            proc.wait(timeout=1.0)
        except Exception:
            pass
        for f in (port_file, err_file, *spawned_token_files):
            try:
                os.remove(f)
            except OSError:
                pass
        raise
