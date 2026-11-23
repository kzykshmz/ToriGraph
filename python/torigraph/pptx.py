"""Zero-COM Native OLE PowerPoint (.pptx) Deck Builder for ToriGraph.

Generates and updates editable ``ToriGraph.Graph.1`` (CLSID ``{8E6B9541-3C74-4D28-A915-7D84E5A12001}``)
OLE objects inside OpenXML ``.pptx`` presentations using ``python-pptx`` and Win32 ``ole32.dll``
``StgCreateDocfile`` Structured Storage (with a pure-Python MS-CFB fallback on non-Windows hosts).

Safety invariants:
- ``POWERPNT.EXE`` / ``win32com`` is **never** spawned, attached, or touched.
- The OS system clipboard is **never** opened or modified.
- Importing this module uses 100% Python standard library; ``python-pptx`` is imported lazily.
"""

from __future__ import annotations

import base64
import ctypes
import json
import math
import os
import re
import struct
import tempfile
import warnings
from pathlib import Path
from typing import TYPE_CHECKING, Any, Dict, List, Mapping, Optional, Sequence, Tuple, Union

from .client import BatchReadError

if TYPE_CHECKING:
    from .client import GraphProxy, Session

TORIGRAPH_OLE_CLSID_STR = "{8E6B9541-3C74-4D28-A915-7D84E5A12001}"
TORIGRAPH_OLE_PROG_ID = "ToriGraph.Graph.1"
TORIGRAPH_ALT_PREFIX = "torigraph:"
TORIGRAPH_SHAPE_PREFIX = "ToriGraphOLE:"

EMU_PER_INCH = 914400
EMU_PER_CM = 360000
EMU_PER_MM = 36000
EMU_PER_HM = 360  # 1/100 mm (HIMETRIC)

P_NS = "http://schemas.openxmlformats.org/presentationml/2006/main"
A_NS = "http://schemas.openxmlformats.org/drawingml/2006/main"
R_NS = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
REL_OLE = "http://schemas.openxmlformats.org/officeDocument/2006/relationships/oleObject"
REL_IMAGE = "http://schemas.openxmlformats.org/officeDocument/2006/relationships/image"
CT_OLE_OBJECT = "application/vnd.openxmlformats-officedocument.oleObject"
CT_EMF = "image/x-emf"

_VALID_PER_SLIDE: Dict[int, Tuple[int, int]] = {
    1: (1, 1),
    2: (1, 2),
    3: (1, 3),
    4: (2, 2),
    6: (2, 3),
}


class _GUID(ctypes.Structure):
    _fields_ = [
        ("Data1", ctypes.c_uint32),
        ("Data2", ctypes.c_uint16),
        ("Data3", ctypes.c_uint16),
        ("Data4", ctypes.c_ubyte * 8),
    ]


_CLSID_TORIGRAPH = _GUID(
    0x8E6B9541,
    0x3C74,
    0x4D28,
    (ctypes.c_ubyte * 8)(0xA9, 0x15, 0x7D, 0x84, 0xE5, 0xA1, 0x20, 0x01),
)

# Little-endian binary bytes of {8E6B9541-3C74-4D28-A915-7D84E5A12001}
_CLSID_TORIGRAPH_BYTES = struct.pack(
    "<IHH8B",
    0x8E6B9541,
    0x3C74,
    0x4D28,
    0xA9,
    0x15,
    0x7D,
    0x84,
    0xE5,
    0xA1,
    0x20,
    0x01,
)


def _import_pptx() -> Tuple[Any, Any, Any, Any]:
    """Lazily import python-pptx components."""
    try:
        import pptx  # type: ignore[import-not-found]
        from pptx.opc.package import Part  # type: ignore[import-not-found]
        from pptx.opc.packuri import PackURI  # type: ignore[import-not-found]
        from pptx.oxml import parse_xml  # type: ignore[import-not-found]

        return pptx, Part, PackURI, parse_xml
    except ImportError as exc:
        raise ImportError(
            "Exporting PowerPoint decks requires 'python-pptx'. Install it via: pip install python-pptx"
        ) from exc


def parse_emf_dimensions(
    emf_bytes: bytes,
    fallback_width_mm: float = 85.0,
    fallback_height_mm: float = 85.0,
) -> Tuple[int, int, int, int]:
    """Extract ``(width_hm, height_hm, width_px, height_px)`` from an EMF ENHMETAHEADER."""
    w_hm_fb = max(500, int(round(max(10.0, float(fallback_width_mm)) * 100.0)))
    h_hm_fb = max(500, int(round(max(10.0, float(fallback_height_mm)) * 100.0)))
    w_px_fb = max(64, int(round(w_hm_fb * 96.0 / 2540.0)))
    h_px_fb = max(64, int(round(h_hm_fb * 96.0 / 2540.0)))

    if len(emf_bytes) >= 44:
        rec_type, _, b_l, b_t, b_r, b_b, f_l, f_t, f_r, f_b = struct.unpack(
            "<IIiiiiiiii", emf_bytes[:40]
        )
        sig = emf_bytes[40:44]
        if rec_type == 1 and sig == b" EMF":
            w_hm = f_r - f_l
            h_hm = f_b - f_t
            w_px = b_r - b_l
            h_px = b_b - b_t
            if w_hm > 0 and h_hm > 0:
                if w_px <= 0 or h_px <= 0:
                    w_px = max(64, int(round(w_hm * 96.0 / 2540.0)))
                    h_px = max(64, int(round(h_hm * 96.0 / 2540.0)))
                return int(w_hm), int(h_hm), int(w_px), int(h_px)
            if w_px > 0 and h_px > 0:
                return (
                    max(500, int(round(w_px * 2540.0 / 96.0))),
                    max(500, int(round(h_px * 2540.0 / 96.0))),
                    int(w_px),
                    int(h_px),
                )
    return w_hm_fb, h_hm_fb, w_px_fb, h_px_fb


def _build_comp_obj_stream(user_type: str = "ToriGraph Graph", prog_id: str = TORIGRAPH_OLE_PROG_ID) -> bytes:
    """Build a standard OLE2 \\x01CompObj stream (used by non-Windows CFB fallback)."""
    # Header: version/byte-order (0xFFFE0001), OS version (0x00000A03), unused (-1), CLSID (16B)
    ut_bytes = user_type.encode("ascii", errors="replace") + b"\x00"
    fmt_bytes = b"Embed Source\x00"
    prog_bytes = prog_id.encode("ascii", errors="replace") + b"\x00"
    parts = [
        struct.pack("<HHII", 0x0001, 0xFFFE, 0x00000A03, 0xFFFFFFFF),
        _CLSID_TORIGRAPH_BYTES,
        struct.pack("<I", len(ut_bytes)),
        ut_bytes,
        struct.pack("<I", len(fmt_bytes)),
        fmt_bytes,
        struct.pack("<I", len(prog_bytes)),
        prog_bytes,
        # Unicode marker (0x71B239F4) + UTF-16LE userType, clipFormat, progId
        struct.pack("<I", 0x71B239F4),
    ]
    for s in (user_type, "Embed Source", prog_id):
        u16 = (s + "\x00").encode("utf-16-le")
        parts.append(struct.pack("<I", len(u16)))
        parts.append(u16)
    return b"".join(parts)


def _cfb_name_key(name: str) -> Tuple[int, str]:
    """MS-CFB red-black directory tree comparison key: (utf16_code_unit_length, uppercase_name)."""
    return (len(name.encode("utf-16-le")) // 2, name.upper())


def _build_cfb_pure_python(streams: Sequence[Tuple[str, bytes]], root_clsid: bytes) -> bytes:
    """Pure-Python MS-CFB v3 (512-byte sector, regular-stream) builder for non-Windows hosts."""
    SECTOR = 512
    ENDOFCHAIN = 0xFFFFFFFE
    FREESECT = 0xFFFFFFFF
    FATSECT = 0xFFFFFFFD

    # Sort streams by MS-CFB red-black ordering and build balanced BST over indices 1..N
    sorted_streams = sorted(streams, key=lambda kv: _cfb_name_key(kv[0]))
    n_streams = len(sorted_streams)

    # Force regular (>= 4096 cutoff bypassed by setting miniStreamCutoffSize = 0 or padding?
    # Note: MS-CFB spec fixes miniStreamCutoffSize == 4096! So streams < 4096 MUST live in MiniStream!)
    MINI_CUTOFF = 4096
    MINI_SECTOR = 64

    mini_chunks = bytearray()
    mini_fat: List[int] = []
    regular_data = bytearray()
    fat: List[int] = []

    stream_meta: List[Tuple[str, int, int]] = []  # (name, start_sect, byte_len)

    # Allocate regular streams (>= 4096 bytes) first
    for name, data in sorted_streams:
        if len(data) >= MINI_CUTOFF:
            start_sec = len(fat)
            n_sec = max(1, math.ceil(len(data) / SECTOR))
            padded = data + b"\x00" * (n_sec * SECTOR - len(data))
            regular_data.extend(padded)
            for k in range(n_sec):
                fat.append((start_sec + k + 1) if k + 1 < n_sec else ENDOFCHAIN)
            stream_meta.append((name, start_sec, len(data)))
        else:
            if len(data) == 0:
                stream_meta.append((name, ENDOFCHAIN, 0))
            else:
                m_start = len(mini_fat)
                n_msec = math.ceil(len(data) / MINI_SECTOR)
                padded_m = data + b"\x00" * (n_msec * MINI_SECTOR - len(data))
                mini_chunks.extend(padded_m)
                for k in range(n_msec):
                    mini_fat.append((m_start + k + 1) if k + 1 < n_msec else ENDOFCHAIN)
                stream_meta.append((name, m_start, len(data)))

    # Place mini_chunks into regular sectors as Root Entry's stream
    if mini_chunks:
        root_start_sec = len(fat)
        root_size = len(mini_chunks)
        n_rsec = math.ceil(root_size / SECTOR)
        regular_data.extend(mini_chunks + b"\x00" * (n_rsec * SECTOR - root_size))
        for k in range(n_rsec):
            fat.append((root_start_sec + k + 1) if k + 1 < n_rsec else ENDOFCHAIN)
    else:
        root_start_sec = ENDOFCHAIN
        root_size = 0

    # MiniFAT sectors
    if mini_fat:
        entries_per_sec = SECTOR // 4  # 128
        n_mfat_sec = math.ceil(len(mini_fat) / entries_per_sec)
        padded_mfat = mini_fat + [FREESECT] * (n_mfat_sec * entries_per_sec - len(mini_fat))
        mfat_start_sec = len(fat)
        regular_data.extend(struct.pack(f"<{len(padded_mfat)}I", *padded_mfat))
        for k in range(n_mfat_sec):
            fat.append((mfat_start_sec + k + 1) if k + 1 < n_mfat_sec else ENDOFCHAIN)
    else:
        mfat_start_sec = ENDOFCHAIN
        n_mfat_sec = 0

    # Build balanced BST over 1..n_streams for Directory Entries
    def _build_bst(lo: int, hi: int) -> Tuple[int, Dict[int, Tuple[int, int]]]:
        if lo > hi:
            return 0xFFFFFFFF, {}
        mid = (lo + hi) // 2
        left_id, left_map = _build_bst(lo, mid - 1)
        right_id, right_map = _build_bst(mid + 1, hi)
        merged = {**left_map, **right_map, mid: (left_id, right_id)}
        return mid, merged

    root_child_id, tree_links = _build_bst(1, n_streams)

    def _pack_dir_entry(
        name: str,
        obj_type: int,
        color: int,
        left_sib: int,
        right_sib: int,
        child_id: int,
        clsid: bytes,
        start_sec: int,
        size: int,
    ) -> bytes:
        u16 = (name + "\x00").encode("utf-16-le")
        if len(u16) > 64:
            raise ValueError(f"CFB stream name too long: {name!r}")
        name_buf = u16 + b"\x00" * (64 - len(u16))
        return struct.pack(
            "<64sHBBIII16sIQQIII",
            name_buf,
            len(u16),
            obj_type,
            color,
            left_sib,
            right_sib,
            child_id,
            clsid,
            0,  # state bits
            0,  # creation time
            0,  # modified time
            start_sec,
            size & 0xFFFFFFFF,
            0,
        )

    dir_bytes = bytearray()
    # Entry 0: Root Entry
    dir_bytes.extend(
        _pack_dir_entry(
            "Root Entry",
            5,
            0,  # black
            0xFFFFFFFF,
            0xFFFFFFFF,
            root_child_id,
            root_clsid,
            root_start_sec,
            root_size,
        )
    )
    for idx1, (sname, sstart, ssize) in enumerate(stream_meta, start=1):
        lsib, rsib = tree_links[idx1]
        dir_bytes.extend(
            _pack_dir_entry(
                sname,
                2,
                1,  # red/black
                lsib,
                rsib,
                0xFFFFFFFF,
                b"\x00" * 16,
                sstart,
                ssize,
            )
        )
    # Pad directory entries to multiple of 4 (512B / 128B = 4 entries per sector)
    while (len(dir_bytes) % SECTOR) != 0:
        dir_bytes.extend(b"\x00" * 64 + struct.pack("<HBBIII", 0, 0, 0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF) + b"\x00" * 48)

    dir_start_sec = len(fat)
    n_dir_sec = len(dir_bytes) // SECTOR
    regular_data.extend(dir_bytes)
    for k in range(n_dir_sec):
        fat.append((dir_start_sec + k + 1) if k + 1 < n_dir_sec else ENDOFCHAIN)

    # Allocate FAT sectors (supports up to 109 FAT sectors = ~7 MB OLE object without DIFAT extension)
    entries_per_fat = SECTOR // 4
    n_fat_sec = 1
    while (len(fat) + n_fat_sec) > n_fat_sec * entries_per_fat:
        n_fat_sec += 1
    if n_fat_sec > 109:
        raise ValueError("OLE payload exceeds single-header DIFAT limit on non-Windows fallback.")

    fat_start_sec = len(fat)
    for _ in range(n_fat_sec):
        fat.append(FATSECT)
    padded_fat = fat + [FREESECT] * (n_fat_sec * entries_per_fat - len(fat))
    regular_data.extend(struct.pack(f"<{len(padded_fat)}I", *padded_fat))

    difat = [fat_start_sec + k for k in range(n_fat_sec)] + [FREESECT] * (109 - n_fat_sec)
    header = struct.pack(
        "<8s16sHHHHH6s9I109I",
        b"\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1",
        b"\x00" * 16,
        0x003E,  # minor version
        0x0003,  # major version 3 (512-byte sectors)
        0xFFFE,  # little-endian
        9,       # sector shift (2^9 = 512)
        6,       # mini sector shift (2^6 = 64)
        b"\x00" * 6,  # reserved
        0,            # num dir sectors (0 for v3)
        n_fat_sec,
        dir_start_sec,
        0,            # transaction sig
        MINI_CUTOFF,
        mfat_start_sec,
        n_mfat_sec,
        ENDOFCHAIN,   # first DIFAT sector
        0,            # num DIFAT sectors
        *difat,
    )
    return header + bytes(regular_data)


def build_ole_compound_file(
    out_bin_path: Union[str, os.PathLike[str]],
    project_bytes: bytes,
    emf_bytes: bytes,
    target_window: str,
    width_hm: int,
    height_hm: int,
    width_px: int,
    height_px: int,
) -> Path:
    """Write a native ``ToriGraph.Graph.1`` OLE2 Compound File at ``out_bin_path``.

    Contains:
    - Root Storage CLSID ``{8E6B9541-3C74-4D28-A915-7D84E5A12001}``
    - ``\\x01CompObj`` (Embed Source, ``ToriGraph Graph``)
    - ``\\x01Ole`` (20-byte OLE 2.0 stream header)
    - ``ToriGraphProject`` & ``SciDAVisProject`` (UTF-8 ``.sciprj`` XML bytes)
    - ``SciDAVisMeta`` (JSON targetWindow & HIMETRIC/pixel geometry)
    - ``SciDAVisEmf`` (vector Enhanced Metafile preview bytes)
    """
    dest = Path(os.fspath(out_bin_path))
    dest.parent.mkdir(parents=True, exist_ok=True)

    ole10_stream = struct.pack("<IIIII", 0x02000001, 0, 0, 0, 0)
    meta_bytes = json.dumps(
        {
            "targetWindow": str(target_window),
            "targetLayer": 0,
            "widthHm": int(width_hm),
            "heightHm": int(height_hm),
            "widthPx": int(width_px),
            "heightPx": int(height_px),
        },
        separators=(",", ":"),
        ensure_ascii=False,
    ).encode("utf-8")

    if os.name != "nt":
        cfb_bytes = _build_cfb_pure_python(
            [
                ("\x01CompObj", _build_comp_obj_stream("ToriGraph Graph", TORIGRAPH_OLE_PROG_ID)),
                ("\x01Ole", ole10_stream),
                ("ToriGraphProject", project_bytes),
                ("SciDAVisProject", project_bytes),
                ("SciDAVisMeta", meta_bytes),
                ("SciDAVisEmf", emf_bytes),
            ],
            _CLSID_TORIGRAPH_BYTES,
        )
        dest.write_bytes(cfb_bytes)
        return dest

    from ctypes import wintypes

    ole32 = ctypes.WinDLL("ole32")
    user32 = ctypes.WinDLL("user32")

    co_init_ex = ole32.CoInitializeEx
    co_init_ex.argtypes = [ctypes.c_void_p, wintypes.DWORD]
    co_init_ex.restype = ctypes.c_long

    stg_create = ole32.StgCreateDocfile
    stg_create.argtypes = [
        ctypes.c_wchar_p,
        wintypes.DWORD,
        wintypes.DWORD,
        ctypes.POINTER(ctypes.c_void_p),
    ]
    stg_create.restype = ctypes.c_long

    write_class_stg = ole32.WriteClassStg
    write_class_stg.argtypes = [ctypes.c_void_p, ctypes.POINTER(_GUID)]
    write_class_stg.restype = ctypes.c_long

    reg_cf = user32.RegisterClipboardFormatW
    reg_cf.argtypes = [ctypes.c_wchar_p]
    reg_cf.restype = wintypes.UINT

    write_fmt_ut = ole32.WriteFmtUserTypeStg
    write_fmt_ut.argtypes = [ctypes.c_void_p, wintypes.WORD, ctypes.c_wchar_p]
    write_fmt_ut.restype = ctypes.c_long

    # COINIT_APARTMENTTHREADED = 0x2; ignore RPC_E_CHANGED_MODE (0x80010106) if already MTA
    co_init_ex(None, 0x2)

    STGM_CREATE = 0x00001000
    STGM_READWRITE = 0x00000002
    STGM_SHARE_EXCLUSIVE = 0x00000010
    mode = STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE

    stg_ptr = ctypes.c_void_p()
    hr = stg_create(os.fspath(dest), mode, 0, ctypes.byref(stg_ptr))
    if hr < 0 or not stg_ptr.value:
        raise RuntimeError(f"StgCreateDocfile('{dest}') failed: HRESULT=0x{hr & 0xFFFFFFFF:08X}")

    CreateStreamProto = ctypes.WINFUNCTYPE(
        ctypes.c_long,
        ctypes.c_void_p,
        ctypes.c_wchar_p,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.DWORD,
        ctypes.POINTER(ctypes.c_void_p),
    )
    CommitStgProto = ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_void_p, wintypes.DWORD)
    ReleaseProto = ctypes.WINFUNCTYPE(wintypes.ULONG, ctypes.c_void_p)
    WriteStreamProto = ctypes.WINFUNCTYPE(
        ctypes.c_long,
        ctypes.c_void_p,
        ctypes.c_void_p,
        wintypes.ULONG,
        ctypes.POINTER(wintypes.ULONG),
    )

    try:
        hr = write_class_stg(stg_ptr, ctypes.byref(_CLSID_TORIGRAPH))
        if hr < 0:
            raise RuntimeError(f"WriteClassStg failed: HRESULT=0x{hr & 0xFFFFFFFF:08X}")

        cf_embed = reg_cf("Embed Source")
        hr = write_fmt_ut(stg_ptr, wintypes.WORD(cf_embed & 0xFFFF), "ToriGraph Graph")
        if hr < 0:
            raise RuntimeError(f"WriteFmtUserTypeStg failed: HRESULT=0x{hr & 0xFFFFFFFF:08X}")

        vtable_stg = ctypes.cast(
            ctypes.cast(stg_ptr, ctypes.POINTER(ctypes.c_void_p))[0],
            ctypes.POINTER(ctypes.c_void_p),
        )
        create_stream = CreateStreamProto(vtable_stg[3])
        commit_stg = CommitStgProto(vtable_stg[9])

        def _write_stream(stream_name: str, payload: bytes) -> None:
            stm_ptr = ctypes.c_void_p()
            hr_s = create_stream(stg_ptr, stream_name, mode, 0, 0, ctypes.byref(stm_ptr))
            if hr_s < 0 or not stm_ptr.value:
                raise RuntimeError(
                    f"IStorage::CreateStream({stream_name!r}) failed: 0x{hr_s & 0xFFFFFFFF:08X}"
                )
            try:
                vt_stm = ctypes.cast(
                    ctypes.cast(stm_ptr, ctypes.POINTER(ctypes.c_void_p))[0],
                    ctypes.POINTER(ctypes.c_void_p),
                )
                write_fn = WriteStreamProto(vt_stm[4])
                if payload:
                    buf = ctypes.create_string_buffer(payload, len(payload))
                    written = wintypes.ULONG(0)
                    hr_w = write_fn(stm_ptr, buf, len(payload), ctypes.byref(written))
                    if hr_w < 0 or int(written.value) != len(payload):
                        raise RuntimeError(
                            f"IStream::Write({stream_name!r}) failed: 0x{hr_w & 0xFFFFFFFF:08X}"
                        )
            finally:
                vt_stm = ctypes.cast(
                    ctypes.cast(stm_ptr, ctypes.POINTER(ctypes.c_void_p))[0],
                    ctypes.POINTER(ctypes.c_void_p),
                )
                ReleaseProto(vt_stm[2])(stm_ptr)

        _write_stream("\x01Ole", ole10_stream)
        _write_stream("ToriGraphProject", project_bytes)
        _write_stream("SciDAVisProject", project_bytes)
        _write_stream("SciDAVisMeta", meta_bytes)
        _write_stream("SciDAVisEmf", emf_bytes)

        hr_c = commit_stg(stg_ptr, 0)
        if hr_c < 0:
            raise RuntimeError(f"IStorage::Commit failed: HRESULT=0x{hr_c & 0xFFFFFFFF:08X}")
    finally:
        vtable_stg = ctypes.cast(
            ctypes.cast(stg_ptr, ctypes.POINTER(ctypes.c_void_p))[0],
            ctypes.POINTER(ctypes.c_void_p),
        )
        ReleaseProto(vtable_stg[2])(stg_ptr)

    return dest


def build_ole_compound_bytes(
    project_bytes: bytes,
    emf_bytes: bytes,
    target_window: str,
    width_hm: int,
    height_hm: int,
    width_px: int,
    height_px: int,
) -> bytes:
    """Build and return in-memory bytes of a ``ToriGraph.Graph.1`` OLE2 Compound File."""
    with tempfile.TemporaryDirectory(prefix="torigraph_ole_") as tmp:
        bin_path = Path(tmp) / "torigraph_obj.bin"
        build_ole_compound_file(
            bin_path,
            project_bytes=project_bytes,
            emf_bytes=emf_bytes,
            target_window=target_window,
            width_hm=width_hm,
            height_hm=height_hm,
            width_px=width_px,
            height_px=height_px,
        )
        return bin_path.read_bytes()


def _extract_shape_xfrm_emu(el: Any) -> Optional[Tuple[int, int, int, int]]:
    """Extract ``(left_emu, top_emu, width_emu, height_emu)`` from any PresentationML shape XML node."""
    # 1. graphicFrame uses <p:xfrm>; 2. pic / sp / cxnSp use <p:spPr>/<a:xfrm>; 3. grpSp uses <p:grpSpPr>/<a:xfrm>
    candidates: List[Any] = []
    p_xfrm = el.find(f"{{{P_NS}}}xfrm")
    if p_xfrm is not None:
        candidates.append(p_xfrm)
    sp_pr = el.find(f"{{{P_NS}}}spPr")
    if sp_pr is not None:
        a_xfrm = sp_pr.find(f"{{{A_NS}}}xfrm")
        if a_xfrm is not None:
            candidates.append(a_xfrm)
    grp_pr = el.find(f"{{{P_NS}}}grpSpPr")
    if grp_pr is not None:
        g_xfrm = grp_pr.find(f"{{{A_NS}}}xfrm")
        if g_xfrm is not None:
            candidates.append(g_xfrm)
    if not candidates:
        candidates.extend(el.xpath(".//*[local-name()='xfrm']"))

    for xf in candidates:
        off = xf.find(f"{{{A_NS}}}off")
        ext = xf.find(f"{{{A_NS}}}ext")
        if off is not None and ext is not None:
            try:
                x = int(off.get("x", "0"))
                y = int(off.get("y", "0"))
                cx = int(ext.get("cx", "0"))
                cy = int(ext.get("cy", "0"))
                if cx > 0 and cy > 0:
                    return x, y, cx, cy
            except ValueError:
                continue
    return None


def _collect_referenced_rids(root_el: Any) -> set[str]:
    """Return all relationship IDs (r:id / r:embed / r:link) referenced inside ``root_el``."""
    rids: set[str] = set()
    for node in root_el.iter():
        for attr_name in (f"{{{R_NS}}}id", f"{{{R_NS}}}embed", f"{{{R_NS}}}link"):
            val = node.get(attr_name)
            if val:
                rids.add(val)
    return rids


def _drop_unreferenced_rels(slide_part: Any, sp_tree: Any, candidate_rids: set[str]) -> None:
    """Garbage-collect slide relationships that are no longer referenced after replacing a shape."""
    if not candidate_rids:
        return
    still_used = _collect_referenced_rids(sp_tree)
    rels_dict = getattr(slide_part.rels, "_rels", None)
    if isinstance(rels_dict, dict):
        for rid in candidate_rids - still_used:
            rels_dict.pop(rid, None)


def _fit_aspect_inside_box(
    box_left: int,
    box_top: int,
    box_w: int,
    box_h: int,
    aspect_w: float,
    aspect_h: float,
) -> Tuple[int, int, int, int]:
    """Letterbox ``(aspect_w, aspect_h)`` proportionally centered inside ``(box_left, box_top, box_w, box_h)``."""
    if aspect_w <= 0 or aspect_h <= 0 or box_w <= 0 or box_h <= 0:
        return int(box_left), int(box_top), max(1, int(box_w)), max(1, int(box_h))
    ratio = float(aspect_w) / float(aspect_h)
    box_ratio = float(box_w) / float(box_h)
    if ratio >= box_ratio:
        w = box_w
        h = max(1, int(round(box_w / ratio)))
    else:
        h = box_h
        w = max(1, int(round(box_h * ratio)))
    left = int(round(box_left + 0.5 * (box_w - w)))
    top = int(round(box_top + 0.5 * (box_h - h)))
    return left, top, int(w), int(h)


def _xml_escape_attr(val: str) -> str:
    return (
        str(val)
        .replace("&", "&amp;")
        .replace('"', "&quot;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
    )


def _find_shape_element_on_slide(
    sp_tree: Any,
    *,
    shape_name: Optional[str] = None,
    graph_name: Optional[str] = None,
) -> Optional[Any]:
    """Locate a direct child shape of ``sp_tree`` matching ``shape_name`` or ToriGraph alt_text."""
    target_alt = f"{TORIGRAPH_ALT_PREFIX}{graph_name}" if graph_name else None
    target_ole_name = f"{TORIGRAPH_SHAPE_PREFIX}{graph_name}" if graph_name else None

    for child in list(sp_tree):
        local = child.tag.rsplit("}", 1)[-1] if "}" in child.tag else child.tag
        if local in ("nvGrpSpPr", "grpSpPr"):
            continue
        # Only inspect top-level cNvPr of this shape (not nested preview <p:pic> inside <p:oleObj>)
        cnv_list = child.xpath(
            "./*[local-name()='nvSpPr' or local-name()='nvPicPr' or local-name()='nvGraphicFramePr' or local-name()='nvGrpSpPr' or local-name()='nvCxnSpPr']/*[local-name()='cNvPr']"
        )
        if not cnv_list:
            continue
        cnv = cnv_list[0]
        nm = cnv.get("name", "")
        descr = cnv.get("descr", "")
        if shape_name is not None:
            if nm == shape_name or descr == shape_name:
                return child
        elif graph_name is not None:
            if descr == target_alt or nm == target_ole_name or nm == target_alt:
                return child
    return None


def embed_ole_shape_on_slide(
    prs: Any,
    slide: Any,
    *,
    ole_bytes: bytes,
    emf_bytes: bytes,
    left_emu: int,
    top_emu: int,
    width_emu: int,
    height_emu: int,
    graph_name: str,
    shape_name: Optional[str] = None,
    prog_id: str = TORIGRAPH_OLE_PROG_ID,
    replace_element: Optional[Any] = None,
) -> Dict[str, Any]:
    """Insert (or replace in-place at original Z-order) a native OLE ``<p:graphicFrame>`` on ``slide``."""
    _, Part, PackURI, parse_xml = _import_pptx()
    sp_tree = slide.shapes._spTree

    insert_idx: Optional[int] = None
    old_rids: set[str] = set()
    if replace_element is not None and replace_element in sp_tree:
        insert_idx = list(sp_tree).index(replace_element)
        old_rids = _collect_referenced_rids(replace_element)
        sp_tree.remove(replace_element)
        _drop_unreferenced_rels(slide.part, sp_tree, old_rids)

    pkg = prs.part.package
    existing_uris = {str(p.partname) for p in pkg.iter_parts()}
    ole_idx = 1
    while f"/ppt/embeddings/oleObject{ole_idx}.bin" in existing_uris:
        ole_idx += 1
    img_idx = 1
    while f"/ppt/media/image{img_idx}.emf" in existing_uris:
        img_idx += 1

    ole_uri = f"/ppt/embeddings/oleObject{ole_idx}.bin"
    emf_uri = f"/ppt/media/image{img_idx}.emf"

    ole_part = Part(PackURI(ole_uri), CT_OLE_OBJECT, pkg, ole_bytes)
    emf_part = Part(PackURI(emf_uri), CT_EMF, pkg, emf_bytes)

    r_id_ole = slide.part.relate_to(ole_part, REL_OLE)
    r_id_img = slide.part.relate_to(emf_part, REL_IMAGE)

    max_id = 1
    for cnv in sp_tree.xpath(".//*[local-name()='cNvPr']"):
        try:
            max_id = max(max_id, int(cnv.get("id", "1")))
        except ValueError:
            pass
    shape_id = max_id + 1
    pic_id = max_id + 2

    eff_name = shape_name or f"{TORIGRAPH_SHAPE_PREFIX}{graph_name}"
    alt_text = f"{TORIGRAPH_ALT_PREFIX}{graph_name}"
    esc_name = _xml_escape_attr(eff_name)
    esc_alt = _xml_escape_attr(alt_text)
    esc_prog = _xml_escape_attr(prog_id)

    x_i = int(round(left_emu))
    y_i = int(round(top_emu))
    w_i = max(1, int(round(width_emu)))
    h_i = max(1, int(round(height_emu)))

    gf_xml = (
        f'<p:graphicFrame xmlns:p="{P_NS}" xmlns:a="{A_NS}" xmlns:r="{R_NS}">'
        f"<p:nvGraphicFramePr>"
        f'<p:cNvPr id="{shape_id}" name="{esc_name}" descr="{esc_alt}"/>'
        f'<p:cNvGraphicFramePr><a:graphicFrameLocks noChangeAspect="1"/></p:cNvGraphicFramePr>'
        f"<p:nvPr/>"
        f"</p:nvGraphicFramePr>"
        f"<p:xfrm>"
        f'<a:off x="{x_i}" y="{y_i}"/>'
        f'<a:ext cx="{w_i}" cy="{h_i}"/>'
        f"</p:xfrm>"
        f"<a:graphic>"
        f'<a:graphicData uri="http://schemas.openxmlformats.org/presentationml/2006/ole">'
        f'<p:oleObj name="ToriGraph Graph" r:id="{r_id_ole}" imgW="{w_i}" imgH="{h_i}" progId="{esc_prog}">'
        f"<p:embed/>"
        f"<p:pic>"
        f"<p:nvPicPr>"
        f'<p:cNvPr id="{pic_id}" name="{esc_name}_Preview" descr="{esc_alt}"/>'
        f'<p:cNvPicPr><a:picLocks noChangeAspect="1"/></p:cNvPicPr>'
        f"<p:nvPr/>"
        f"</p:nvPicPr>"
        f"<p:blipFill>"
        f'<a:blip r:embed="{r_id_img}"/>'
        f"<a:stretch><a:fillRect/></a:stretch>"
        f"</p:blipFill>"
        f"<p:spPr>"
        f"<a:xfrm>"
        f'<a:off x="{x_i}" y="{y_i}"/>'
        f'<a:ext cx="{w_i}" cy="{h_i}"/>'
        f"</a:xfrm>"
        f'<a:prstGeom prst="rect"><a:avLst/></a:prstGeom>'
        f"</p:spPr>"
        f"</p:pic>"
        f"</p:oleObj>"
        f"</a:graphicData>"
        f"</a:graphic>"
        f"</p:graphicFrame>"
    )
    gf_el = parse_xml(gf_xml)
    if insert_idx is not None:
        sp_tree.insert(insert_idx, gf_el)
    else:
        sp_tree.append(gf_el)

    return {
        "graph": graph_name,
        "shape_name": eff_name,
        "alt_text": alt_text,
        "ole_part": ole_uri,
        "emf_part": emf_uri,
        "left_cm": round(x_i / EMU_PER_CM, 4),
        "top_cm": round(y_i / EMU_PER_CM, 4),
        "width_cm": round(w_i / EMU_PER_CM, 4),
        "height_cm": round(h_i / EMU_PER_CM, 4),
        "replaced": replace_element is not None,
    }


def _pick_blank_slide_layout(prs: Any) -> Any:
    """Select the cleanest blank slide layout from ``prs.slide_layouts``."""
    layouts = list(prs.slide_layouts)
    if not layouts:
        raise RuntimeError("Presentation contains zero slide layouts.")
    for layout in layouts:
        nm = str(getattr(layout, "name", "") or "").strip().lower()
        if nm in ("blank", "白紙"):
            return layout
    # Prefer layout with zero placeholders
    best = min(layouts, key=lambda ly: len(list(getattr(ly, "placeholders", []))))
    return best


def _add_clean_blank_slide(prs: Any, title_text: Optional[str] = None) -> Any:
    """Append a blank slide (stripping inherited layout placeholders) and optional Arial slide title."""
    pptx, _, _, _ = _import_pptx()
    slide = prs.slides.add_slide(_pick_blank_slide_layout(prs))
    sp_tree = slide.shapes._spTree
    for child in list(sp_tree):
        if child.xpath(".//*[local-name()='ph']"):
            sp_tree.remove(child)

    if title_text:
        tb = slide.shapes.add_textbox(
            int(round(1.2 * EMU_PER_CM)),
            int(round(0.45 * EMU_PER_CM)),
            max(1, int(prs.slide_width) - int(round(2.4 * EMU_PER_CM))),
            int(round(1.3 * EMU_PER_CM)),
        )
        tf = tb.text_frame
        tf.word_wrap = True
        p = tf.paragraphs[0]
        run = p.add_run()
        run.text = str(title_text)
        run.font.name = "Arial"
        run.font.bold = False
        run.font.size = pptx.util.Pt(22)
    return slide


def _resolve_1based_slide(prs: Any, slide_spec: int) -> Tuple[Any, int]:
    """Resolve slide specifier (1-based PowerPoint index, 0 as alias for 1, or negative from end)."""
    n_slides = len(prs.slides)
    if n_slides == 0:
        raise IndexError("Target deck has 0 slides.")
    idx_int = int(slide_spec)
    if idx_int == 0:
        idx0 = 0
    elif idx_int > 0:
        idx0 = idx_int - 1
    else:
        idx0 = n_slides + idx_int
    if idx0 < 0 or idx0 >= n_slides:
        raise IndexError(
            f"Slide index {slide_spec} out of range for deck with {n_slides} slide(s) (1..{n_slides})."
        )
    return prs.slides[idx0], idx0 + 1


def _parse_box_cm(
    box: Sequence[float],
    width_hm: int,
    height_hm: int,
) -> Tuple[int, int, int, int]:
    """Convert ``box=(left_cm, top_cm, width_cm[, height_cm])`` into aspect-safe EMU coordinates."""
    vals = [float(v) for v in box]
    if len(vals) == 3:
        l_cm, t_cm, w_cm = vals
        if w_cm <= 0:
            raise ValueError(f"box width_cm must be > 0, got {w_cm}.")
        l_emu = int(round(l_cm * EMU_PER_CM))
        t_emu = int(round(t_cm * EMU_PER_CM))
        w_emu = int(round(w_cm * EMU_PER_CM))
        h_emu = max(1, int(round(w_emu * (float(height_hm) / float(max(1, width_hm))))))
        return l_emu, t_emu, w_emu, h_emu
    if len(vals) == 4:
        l_cm, t_cm, w_cm, h_cm = vals
        if w_cm <= 0:
            raise ValueError(f"box width_cm must be > 0, got {w_cm}.")
        l_emu = int(round(l_cm * EMU_PER_CM))
        t_emu = int(round(t_cm * EMU_PER_CM))
        w_emu = int(round(w_cm * EMU_PER_CM))
        if h_cm <= 0:
            h_emu = max(1, int(round(w_emu * (float(height_hm) / float(max(1, width_hm))))))
            return l_emu, t_emu, w_emu, h_emu
        h_emu = int(round(h_cm * EMU_PER_CM))
        return _fit_aspect_inside_box(l_emu, t_emu, w_emu, h_emu, width_hm, height_hm)
    raise ValueError(
        f"box must be (left_cm, top_cm, width_cm) or (left_cm, top_cm, width_cm, height_cm), got {box!r}."
    )


def _compute_grid_cell_emu(
    slide_w: int,
    slide_h: int,
    n_rows: int,
    n_cols: int,
    cell_idx: int,
    has_title: bool = False,
) -> Tuple[int, int, int, int]:
    """Compute cell ``(left_emu, top_emu, width_emu, height_emu)`` for ``per_slide`` grid layout."""
    margin_x = int(round(1.35 * EMU_PER_CM))
    margin_top = int(round((2.15 if has_title else 1.25) * EMU_PER_CM))
    margin_bottom = int(round(1.15 * EMU_PER_CM))
    gap_x = int(round(0.85 * EMU_PER_CM)) if n_cols > 1 else 0
    gap_y = int(round(0.80 * EMU_PER_CM)) if n_rows > 1 else 0

    usable_w = max(EMU_PER_CM, slide_w - 2 * margin_x - (n_cols - 1) * gap_x)
    usable_h = max(EMU_PER_CM, slide_h - margin_top - margin_bottom - (n_rows - 1) * gap_y)

    cell_w = usable_w // n_cols
    cell_h = usable_h // n_rows
    r = cell_idx // n_cols
    c = cell_idx % n_cols
    left = margin_x + c * (cell_w + gap_x)
    top = margin_top + r * (cell_h + gap_y)
    return left, top, cell_w, cell_h


_WIN_FORBIDDEN_FILENAME_CHARS = re.compile(r'[<>:"/\\|?*\x00-\x1f]')
_WIN_RESERVED_NAMES = frozenset(
    {"CON", "PRN", "AUX", "NUL"}
    | {f"COM{i}" for i in range(1, 10)}
    | {f"LPT{i}" for i in range(1, 10)}
)


def _safe_tmp_component(name: str, max_len: int = 40) -> str:
    """Make a window name (``Al/Zn``, ``Sample:1``, ``CON`` ...) safe as part of a temp file name."""
    slug = _WIN_FORBIDDEN_FILENAME_CHARS.sub("_", str(name)).strip(" .")
    if len(slug) > max_len:
        slug = slug[:max_len].rstrip(" .")
    if not slug:
        slug = "graph"
    if slug.split(".", 1)[0].upper() in _WIN_RESERVED_NAMES:
        slug = "_" + slug
    return slug


def _atomic_save_presentation(prs: Any, out_path: Path) -> None:
    """Save ``prs`` next to ``out_path`` first and ``os.replace`` it over the target.

    A crash or a full disk while writing never leaves a truncated ``.pptx`` behind, and an
    existing deck is only replaced once the new file is complete.
    """
    out_path.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp_name = tempfile.mkstemp(
        prefix=f".{_safe_tmp_component(out_path.stem, 24)}.", suffix=".tmp", dir=os.fspath(out_path.parent)
    )
    os.close(fd)
    try:
        prs.save(tmp_name)
        os.replace(tmp_name, os.fspath(out_path))
    finally:
        try:
            if os.path.exists(tmp_name):
                os.remove(tmp_name)
        except OSError:
            pass


_LAB_SLIDE_PRESETS = frozenset({"lab_slide_83mm", "lab_slide"})
#: Placement scale of the lab slide preset (83.3 mm / 12.9 pt x 1.4 = 18 pt on the slide).
LAB_SLIDE_PLACEMENT_SCALE = 1.4


def _is_lab_slide_preset(preset: Any) -> bool:
    return bool(preset) and str(preset).strip().lower() in _LAB_SLIDE_PRESETS


def _fetch_graph_ole_payload(
    session: "Session",
    graph_name: str,
    *,
    dpi: int,
    tmp_dir: Path,
    seq_idx: int,
    cached_project_bytes: Optional[bytes],
    embed_scope: str = "graph",
    warn_list: Optional[List[str]] = None,
    allow_whole_project: bool = False,
    ole_meta: Optional[Dict[str, Any]] = None,
) -> Tuple[bytes, bytes, int, int, int, int, str, str, Optional[bytes]]:
    """Fetch ``(project_bytes, emf_bytes, w_hm, h_hm, w_px, h_px, prog_id, canonical_window, updated_cache)`` via RPC.

    With ``embed_scope="graph"`` every graph gets its own project stream (the graph and the
    tables it plots), so the project cache is only used for ``embed_scope="project"``.
    Problems that change what is embedded or how large it is are appended to ``warn_list``
    and issued as :class:`UserWarning` instead of being hidden.

    A per-graph object must never silently contain the whole project (other windows, the
    results log): unless ``allow_whole_project`` is true, every path that would embed it
    raises :class:`ToriGraphError` instead.
    """
    from .client import RpcError, ToriGraphError

    def _refuse_whole_project(why: str, cause: Optional[BaseException] = None) -> None:
        if allow_whole_project:
            return
        raise ToriGraphError(
            f"{why} Embedding the whole project in the slide object could leak other windows "
            "and the results log; pass allow_whole_project=True to accept that."
        ) from cause

    def _warn(msg: str) -> None:
        if warn_list is not None:
            warn_list.append(msg)
        warnings.warn(msg, UserWarning, stacklevel=3)

    emf_tmp = tmp_dir / f"preview_{seq_idx}_{_safe_tmp_component(graph_name)}.emf"
    proj_bytes: Optional[bytes] = None
    emf_bytes: Optional[bytes] = None
    w_mm: Optional[float] = None
    h_mm: Optional[float] = None
    prog_id = TORIGRAPH_OLE_PROG_ID
    canonical_window = graph_name
    per_graph_project = embed_scope != "project"

    skip_proj = (cached_project_bytes is not None) and not per_graph_project
    try:
        ole_res = session.call(
            "graph.export_ole",
            graph=graph_name,
            emf_path=os.fspath(emf_tmp),
            dpi=int(dpi),
            include_base64=False,
            skip_project_stream=bool(skip_proj),
            embed_scope=str(embed_scope),
        )
        if isinstance(ole_res, Mapping):
            if ole_res.get("warning"):
                _warn(str(ole_res["warning"]))
            got_scope = ole_res.get("embed_scope")
            if per_graph_project and got_scope not in (None, "graph"):
                _refuse_whole_project(
                    f"graph.export_ole embedded scope {got_scope!r} for '{graph_name}' instead of 'graph'."
                )
                _warn(
                    f"graph.export_ole embedded scope {got_scope!r} for '{graph_name}' instead of "
                    "'graph'; the slide object contains the whole project."
                )
            elif per_graph_project and got_scope is None and not skip_proj:
                _refuse_whole_project(
                    f"This ToriGraph does not support embed_scope, so the object for '{graph_name}' "
                    "would contain the whole project."
                )
                _warn(
                    f"This ToriGraph does not support embed_scope; the object for '{graph_name}' "
                    "contains the whole project (all windows and the results log)."
                )
            prog_id = str(ole_res.get("prog_id") or TORIGRAPH_OLE_PROG_ID)
            if ole_res.get("graph"):
                canonical_window = str(ole_res["graph"])
            if ole_res.get("width_mm"):
                w_mm = float(ole_res["width_mm"])
            if ole_res.get("height_mm"):
                h_mm = float(ole_res["height_mm"])
            if ole_meta is not None:
                for k in ("slide_paste_percent", "applied_style_preset", "style_preset"):
                    if ole_res.get(k) is not None:
                        ole_meta[k] = ole_res[k]

            xml_val = ole_res.get("sciprj_xml") or ole_res.get("project_xml")
            if isinstance(xml_val, str) and xml_val:
                proj_bytes = xml_val.encode("utf-8")
            elif isinstance(ole_res.get("sciprj_base64"), str) and ole_res["sciprj_base64"]:
                proj_bytes = base64.b64decode(ole_res["sciprj_base64"])

            if emf_tmp.is_file() and emf_tmp.stat().st_size >= 44:
                emf_bytes = emf_tmp.read_bytes()
            elif ole_res.get("emf_path") and Path(str(ole_res["emf_path"])).is_file():
                emf_bytes = Path(str(ole_res["emf_path"])).read_bytes()
            elif isinstance(ole_res.get("emf_base64"), str) and ole_res["emf_base64"]:
                emf_bytes = base64.b64decode(ole_res["emf_base64"])
    except RpcError as exc:
        _refuse_whole_project(f"graph.export_ole failed for '{graph_name}' ({exc}).", exc)
        _warn(
            f"graph.export_ole failed for '{graph_name}' ({exc}). Falling back to a snapshot of "
            "the whole project and to the size recorded in the EMF preview; check the size on "
            "the slide."
        )

    if proj_bytes is None:
        if cached_project_bytes is not None and not per_graph_project:
            proj_bytes = cached_project_bytes
        else:
            if per_graph_project:
                _refuse_whole_project(f"No per-graph project was returned for '{graph_name}'.")
                _warn(
                    f"No per-graph project was returned for '{graph_name}'; the slide object "
                    "contains the whole project instead."
                )
            sciprj_tmp = tmp_dir / "workspace_ole_snapshot.sciprj"
            try:
                session.save_project(sciprj_tmp, compress=False, snapshot=True)
            except TypeError:
                session.save_project(sciprj_tmp, compress=False)
            proj_bytes = sciprj_tmp.read_bytes()
            cached_project_bytes = proj_bytes
    elif not per_graph_project:
        cached_project_bytes = proj_bytes

    if emf_bytes is None:
        session.call("graph.export", graph=graph_name, path=os.fspath(emf_tmp), dpi=int(dpi))
        emf_bytes = emf_tmp.read_bytes()

    if len(emf_bytes) < 44 or emf_bytes[40:44] != b" EMF":
        raise RuntimeError(f"Graph '{graph_name}' did not produce a valid Windows EMF preview.")

    w_hm, h_hm, w_px, h_px = parse_emf_dimensions(
        emf_bytes,
        fallback_width_mm=w_mm if w_mm else 85.0,
        fallback_height_mm=h_mm if h_mm else 85.0,
    )
    if not _emf_has_frame(emf_bytes) and not (w_mm and h_mm):
        _warn(
            f"The size of '{graph_name}' is unknown (no EMF frame and no width_mm/height_mm); "
            "a provisional 85 mm x 85 mm was used."
        )
    return (
        proj_bytes,
        emf_bytes,
        w_hm,
        h_hm,
        w_px,
        h_px,
        prog_id,
        canonical_window,
        cached_project_bytes,
    )


def _emf_has_frame(emf_bytes: bytes) -> bool:
    """True when the EMF header records a positive physical frame (``rclFrame``)."""
    if len(emf_bytes) < 44 or emf_bytes[40:44] != b" EMF":
        return False
    rec_type, _, _, _, _, _, f_l, f_t, f_r, f_b = struct.unpack("<IIiiiiiiii", emf_bytes[:40])
    return rec_type == 1 and (f_r - f_l) > 0 and (f_b - f_t) > 0


def _parse_scale(scale: Optional[Union[float, int, str]]) -> Optional[float]:
    """Accept ``1.4``, ``140`` (percent, when > 10) or ``"140%"``; ``None`` keeps the old fitting."""
    if scale is None:
        return None
    if isinstance(scale, str):
        txt = scale.strip()
        if txt.endswith("%"):
            val = float(txt[:-1]) / 100.0
        else:
            val = float(txt)
    else:
        val = float(scale)
    if val > 10.0:  # e.g. 140 meaning 140 %
        val /= 100.0
    if not math.isfinite(val) or val <= 0.0:
        raise ValueError(f"scale must be a positive factor (e.g. 1.4 or '140%'), got {scale!r}.")
    return val


def _graph_info_safe(session: "Session", graph_name: str) -> Optional[Mapping[str, Any]]:
    try:
        info = session.call("graph.get_info", graph=graph_name)
    except BatchReadError:
        raise
    except Exception:  # Graph3D windows and older servers have no 2D axis info
        return None
    return info if isinstance(info, Mapping) else None


def _graph_slide_scale(session: "Session", graph_name: str, info: Optional[Mapping[str, Any]]) -> Optional[float]:
    """Placement factor (e.g. 1.4) of a graph that already carries the lab slide preset.

    Sources, in order: the percentage ToriGraph reports (``slide_paste_percent``), the preset
    id it reports (``applied_style_preset`` / ``style_preset``), and — only when ``info``
    lacks the slide-preset fields (older server) — the presets this SDK session applied itself
    (``Session._slide_preset_graphs``, filled by ``graph.apply_style_preset``).
    ``None`` means "no slide preset known".
    """
    if info is not None:
        pct = info.get("slide_paste_percent")
        if isinstance(pct, (int, float)) and not isinstance(pct, bool) and math.isfinite(pct) and pct > 0:
            return float(pct) / 100.0
        applied = info.get("applied_style_preset") or info.get("style_preset")
        if _is_lab_slide_preset(applied):
            return LAB_SLIDE_PLACEMENT_SCALE
        if any(k in info for k in ("slide_paste_percent", "applied_style_preset", "style_preset")):
            return None
    if graph_name in (getattr(session, "_slide_preset_graphs", None) or ()):
        return LAB_SLIDE_PLACEMENT_SCALE
    return None


def _graph_nominal_font_pts(
    session: "Session", graph_name: str, info: Optional[Mapping[str, Any]] = None
) -> Dict[str, Optional[float]]:
    """Largest axis-title and tick-label font sizes (pt) of the graph at 100 % size."""
    out: Dict[str, Optional[float]] = {"axis_title_pt": None, "tick_label_pt": None}
    if info is None:
        info = _graph_info_safe(session, graph_name)
    if info is None:
        return out
    titles: List[float] = []
    ticks: List[float] = []
    for layer in info.get("layers") or []:
        axes = (layer or {}).get("axes") or {}
        for ax in axes.values():
            if not isinstance(ax, Mapping) or not ax.get("enabled", True):
                continue
            t_pt = ax.get("title_font_pt") or ax.get("title_font_size")
            n_pt = ax.get("numbers_font_pt") or ax.get("numbers_font_size")
            if isinstance(t_pt, (int, float)) and t_pt > 0:
                titles.append(float(t_pt))
            if isinstance(n_pt, (int, float)) and n_pt > 0:
                ticks.append(float(n_pt))
    out["axis_title_pt"] = max(titles) if titles else None
    out["tick_label_pt"] = max(ticks) if ticks else None
    return out


def _scaled_rect_emu(
    w_hm: int,
    h_hm: int,
    scale: float,
    anchor_rect: Tuple[int, int, int, int],
    *,
    center: bool,
) -> Tuple[int, int, int, int]:
    """Natural size x ``scale``, centred in ``anchor_rect`` (or at its top-left corner)."""
    w_e = max(1, int(round(w_hm * EMU_PER_HM * scale)))
    h_e = max(1, int(round(h_hm * EMU_PER_HM * scale)))
    a_l, a_t, a_w, a_h = anchor_rect
    if center:
        l_e = a_l + max(0, (a_w - w_e) // 2)
        t_e = a_t + max(0, (a_h - h_e) // 2)
    else:
        l_e, t_e = a_l, a_t
    return l_e, t_e, w_e, h_e


def _clamp_scaled_rect_to_slide_emu(
    l_e: int,
    t_e: int,
    w_e: int,
    h_e: int,
    slide_w: int,
    slide_h: int,
    *,
    anchor_rect: Optional[Tuple[int, int, int, int]] = None,
    center: bool = False,
) -> Tuple[int, int, int, int, Optional[Tuple[float, float]]]:
    """Clamp an oversize or out-of-bounds scaled placement proportionally into ``(0, 0, slide_w, slide_h)``."""
    tol = EMU_PER_MM
    if (
        l_e >= -tol
        and t_e >= -tol
        and l_e + w_e <= slide_w + tol
        and t_e + h_e <= slide_h + tol
    ):
        return l_e, t_e, w_e, h_e, None

    orig_cm = (round(w_e / EMU_PER_CM, 2), round(h_e / EMU_PER_CM, 2))
    pad = int(round(0.35 * EMU_PER_CM))
    if center:
        max_w = max(EMU_PER_CM, slide_w - 2 * pad)
        max_h = max(EMU_PER_CM, slide_h - 2 * pad)
    else:
        clamped_l = max(0, min(l_e, max(0, slide_w - EMU_PER_CM)))
        clamped_t = max(0, min(t_e, max(0, slide_h - EMU_PER_CM)))
        max_w = max(EMU_PER_CM, slide_w - clamped_l - pad)
        max_h = max(EMU_PER_CM, slide_h - clamped_t - pad)
        l_e, t_e = clamped_l, clamped_t

    if w_e > max_w or h_e > max_h:
        factor = min(float(max_w) / float(max(1, w_e)), float(max_h) / float(max(1, h_e)))
        w_e = max(1, int(round(w_e * factor)))
        h_e = max(1, int(round(h_e * factor)))

    if center and anchor_rect is not None:
        a_l, a_t, a_w, a_h = anchor_rect
        l_e = a_l + (a_w - w_e) // 2
        t_e = a_t + (a_h - h_e) // 2

    l_e = max(0, min(l_e, max(0, slide_w - w_e)))
    t_e = max(0, min(t_e, max(0, slide_h - h_e)))
    return l_e, t_e, w_e, h_e, orig_cm


def _explicit_box_emu(
    cur_box: Sequence[float], w_hm: int, h_hm: int, scale: Optional[float]
) -> Tuple[int, int, int, int]:
    """``(left, top)`` = natural size x scale; 3/4 values = the box (scale not allowed)."""
    vals = [float(v) for v in cur_box]
    if len(vals) == 2:
        s = 1.0 if scale is None else scale
        return _scaled_rect_emu(
            w_hm,
            h_hm,
            s,
            (int(round(vals[0] * EMU_PER_CM)), int(round(vals[1] * EMU_PER_CM)), 0, 0),
            center=False,
        )
    if scale is not None:
        raise ValueError(
            "box with a width cannot be combined with scale=; pass box=(left_cm, top_cm) "
            "to place the graph at its natural size x scale."
        )
    return _parse_box_cm(vals, w_hm, h_hm)


def export_graphs_to_pptx(
    session: "Session",
    path: Union[str, os.PathLike[str]],
    graphs: Optional[Union[str, "GraphProxy", Sequence[Union[str, "GraphProxy"]]]] = None,
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
    """Export one or more ToriGraph figures into a PowerPoint ``.pptx`` as editable native OLE objects.

    Parameters
    ----------
    session:
        Connected :class:`torigraph.Session`.
    path:
        Output ``.pptx`` file path.
    graphs:
        A graph name / :class:`GraphProxy`, a sequence of graphs, or ``None`` (all open graphs).
    per_slide:
        Automatic grid density when creating new slides: ``1``, ``2`` (1x2), ``3`` (1x3),
        ``4`` (2x2), or ``6`` (2x3).
    deck:
        Optional existing ``.pptx`` template/deck to append to, overlay onto, or update.
        Refuses ``path == deck`` unless ``overwrite=True``.
    slide:
        Optional 1-based target slide number in ``deck`` (``0`` is also accepted for Slide 1,
        and negative numbers index from the end).
    box:
        Explicit figure placement ``(left_cm, top_cm, width_cm)`` (preserving aspect ratio) or
        ``(left_cm, top_cm, width_cm, height_cm)``, or a per-graph list of boxes.
    replace:
        - ``[(2, "Picture 3"), ...]`` (or ``{graph_name: (slide_num, shape_name)}``): replace
          named placeholder shapes on the specified 1-based slide(s), preserving Z-order and box.
        - ``True``: idempotently update previously embedded ``alt_text="torigraph:<graph_name>"``
          OLE shapes in-place (preserving user-resized slide geometry unless ``box`` is given).
    dpi:
        Target render resolution (36..2400).
    overwrite:
        An existing ``path`` is never replaced silently: ``FileExistsError`` unless
        ``overwrite=True``.  The same flag is required to update ``path == deck`` in place
        and to update an existing ``path`` via ``replace=`` / ``slide=`` without ``deck``.
        The file is written to a temporary name next to ``path`` and moved over it with
        ``os.replace`` so a failure never leaves a broken deck.
    titles:
        Optional slide title (or per-slide sequence of titles) when generating new slides.
    preset:
        Optional style preset (e.g. ``"lab_slide_83mm"``) applied to each 2D graph before
        export (Graph3D windows are skipped); an RPC error such as an unknown preset or graph
        name is raised.  Graphs that carry ``lab_slide_83mm`` -- via ``preset=`` or applied
        earlier (``g.lab_slide_style()``) -- are placed at 140 % automatically (12.9 pt -> 18 pt)
        unless ``scale=`` or a ``box`` width is given.
    widescreen:
        Use 16:9 widescreen (``13.333 x 7.5 in``) when creating a brand-new deck.
    scale:
        Size on the slide relative to the graph's own (page) size, e.g. ``1.4``, ``140`` or
        ``"140%"``. Without it, graphs are shrunk/enlarged to fit their grid cell (or the
        replaced shape), as before. With it, each graph is placed at exactly natural size x
        ``scale`` (centred in its cell, or at the top-left of a replaced shape). It may be
        combined with ``box=(left_cm, top_cm)``, but not with a box that gives a width.
    embed_scope:
        ``"graph"`` (default) embeds each graph with only the tables/matrices it plots;
        ``"project"`` embeds the whole project (all windows and the results log) in every object.
    allow_whole_project:
        ``False`` (default): if a per-graph object cannot be produced (``graph.export_ole``
        fails or the server returns no per-graph project) a ``ToriGraphError`` is raised.
        ``True`` falls back to embedding a snapshot of the whole project (other windows and
        the results log included) with a warning.

    Returns
    -------
    dict
        Besides the counts, every entry of ``placements`` carries ``effective_scale`` (size on
        the slide / natural size), the natural and placed size in cm, and the nominal and
        effective (= nominal x effective_scale) axis-title and tick-label sizes in pt.
        ``warnings`` lists problems that changed what was embedded or how it was sized.
    """
    pptx, _, _, _ = _import_pptx()
    explicit_scale_f = _parse_scale(scale)
    new_placement_scale_f = explicit_scale_f
    # Lab slide preset (83.3 mm / 12.9 pt) is pasted at 140 % -> 18 pt, unless the caller
    # chose a size (scale= or a box with a width).  Graphs that got the preset earlier
    # (g.lab_slide_style() / apply_style_preset) are detected per graph below.
    box_has_explicit_width = False
    if box is not None:
        box_probe = list(box)
        if box_probe and isinstance(box_probe[0], (list, tuple)):
            box_has_explicit_width = any(len(b) >= 3 for b in box_probe)  # type: ignore[arg-type]
        else:
            box_has_explicit_width = len(box_probe) >= 3
    if new_placement_scale_f is None and _is_lab_slide_preset(preset) and not box_has_explicit_width:
        new_placement_scale_f = LAB_SLIDE_PLACEMENT_SCALE
    embed_scope = str(embed_scope or "graph").strip().lower()
    if embed_scope not in ("graph", "project"):
        raise ValueError(f"embed_scope must be 'graph' or 'project', got {embed_scope!r}.")
    warn_list: List[str] = []

    if int(per_slide) not in _VALID_PER_SLIDE:
        raise ValueError(
            f"Unsupported per_slide={per_slide!r}; expected one of {sorted(_VALID_PER_SLIDE)}."
        )
    per_slide_int = int(per_slide)

    out_path = Path(os.fspath(path))
    deck_path: Optional[Path] = Path(os.fspath(deck)) if deck is not None else None

    if deck_path is not None:
        if not deck_path.is_file():
            raise FileNotFoundError(f"Template/source deck does not exist: {deck_path}")
        if out_path.resolve() == deck_path.resolve() and not overwrite:
            raise ValueError(
                f"Safety check failed: output path '{out_path}' equals source deck '{deck_path}'. "
                "Pass overwrite=True to update the source deck in-place, or specify a new output path."
            )
    if out_path.exists() and not overwrite:
        updates_in_place = deck_path is None and (
            (replace is not None and replace is not False) or slide is not None
        )
        raise FileExistsError(
            f"'{out_path}' already exists"
            + (" and would be updated in place" if updates_in_place else " and would be replaced")
            + ". Pass overwrite=True to allow it, or choose a new output path."
        )

    # Resolve target graph names
    try:
        _windows = list(session.list_windows())
    except BatchReadError:
        raise
    except Exception:  # very old servers
        _windows = []
    graph3d_names = {str(w["name"]) for w in _windows if w.get("type") == "Graph3D" and w.get("name")}
    if graphs is None:
        graph_names = [
            str(w["name"])
            for w in _windows
            if w.get("type") in ("MultiLayer", "Graph3D") and w.get("name")
        ]
    elif isinstance(graphs, str) or hasattr(graphs, "name"):
        g_nm = graphs if isinstance(graphs, str) else str(graphs.name)
        graph_names = [g_nm]
    else:
        graph_names = [
            (g if isinstance(g, str) else str(g.name)) for g in graphs if g is not None
        ]

    if not graph_names:
        raise ValueError("No graphs specified (and no open graph windows found in session).")

    if preset:
        # Style presets exist for 2D graphs only; Graph3D windows are exported as they are.
        # RpcErrors (unknown preset, misspelled graph name, ...) are not swallowed.
        for g_nm in graph_names:
            if g_nm in graph3d_names:
                msg = f"Style preset '{preset}' is not applied to 3D graph '{g_nm}' (2D graphs only)."
                warn_list.append(msg)
                continue
            session.call("graph.apply_style_preset", graph=g_nm, preset=str(preset))

    # Normalize explicit per-graph boxes if provided
    per_graph_boxes: Optional[List[Sequence[float]]] = None
    single_box: Optional[Sequence[float]] = None
    if box is not None:
        box_seq = list(box)
        if box_seq and isinstance(box_seq[0], (list, tuple)):
            if len(box_seq) != len(graph_names):
                raise ValueError(
                    f"Length of box sequence ({len(box_seq)}) must match graphs ({len(graph_names)})."
                )
            per_graph_boxes = [list(b) for b in box_seq]  # type: ignore[arg-type]
        else:
            single_box = [float(x) for x in box_seq]  # type: ignore[arg-type]

    # Normalize named-shape replace mapping if provided
    named_replace_targets: Optional[List[Tuple[Optional[int], str]]] = None
    auto_replace_alt = False
    if isinstance(replace, bool):
        auto_replace_alt = replace
    elif isinstance(replace, str):
        if replace.strip().lower() in ("true", "auto", "in_place", "inplace"):
            auto_replace_alt = True
        else:
            if len(graph_names) != 1:
                raise ValueError(
                    "Single shape-name string in replace= requires exactly 1 graph; pass a list of (slide, shape_name)."
                )
            named_replace_targets = [(slide, replace)]
    elif isinstance(replace, Mapping):
        named_replace_targets = []
        for g_nm in graph_names:
            if g_nm not in replace:
                raise KeyError(f"Graph '{g_nm}' missing from replace mapping {list(replace.keys())}.")
            spec = replace[g_nm]
            if isinstance(spec, (list, tuple)) and len(spec) == 2:
                named_replace_targets.append((int(spec[0]), str(spec[1])))
            else:
                named_replace_targets.append((slide, str(spec)))
    elif isinstance(replace, (list, tuple)):
        if len(replace) != len(graph_names):
            raise ValueError(
                f"Length of replace targets ({len(replace)}) must match graphs ({len(graph_names)})."
            )
        named_replace_targets = []
        for item in replace:
            if isinstance(item, (list, tuple)) and len(item) == 2:
                named_replace_targets.append((int(item[0]), str(item[1])))
            elif isinstance(item, str):
                named_replace_targets.append((slide, item))
            else:
                raise ValueError(
                    f"Invalid replace entry {item!r}; expected (slide_num, shape_name) tuple."
                )

    # Open existing deck or create new Presentation
    if deck_path is not None:
        prs = pptx.Presentation(os.fspath(deck_path))
    elif (auto_replace_alt or named_replace_targets is not None or slide is not None) and out_path.is_file():
        prs = pptx.Presentation(os.fspath(out_path))
    else:
        prs = pptx.Presentation()
        if widescreen:
            prs.slide_width = pptx.util.Inches(13.333)
            prs.slide_height = pptx.util.Inches(7.5)
        else:
            prs.slide_width = pptx.util.Inches(10.0)
            prs.slide_height = pptx.util.Inches(7.5)

    slide_w = int(prs.slide_width)
    slide_h = int(prs.slide_height)
    n_rows, n_cols = _VALID_PER_SLIDE[per_slide_int]

    title_list: Optional[List[str]] = None
    if isinstance(titles, str):
        title_list = [titles]
    elif titles is not None:
        title_list = [str(t) for t in titles]

    placements: List[Dict[str, Any]] = []
    touched_slides: set[int] = set()
    replaced_count = 0
    embedded_count = 0

    with tempfile.TemporaryDirectory(prefix="torigraph_pptx_") as tmp:
        tmp_dir = Path(tmp)
        cached_proj: Optional[bytes] = None

        # Pre-build OLE payloads for all requested graphs
        built_items: List[Dict[str, Any]] = []
        for idx, g_nm in enumerate(graph_names):
            ole_meta: Dict[str, Any] = {}
            (
                proj_b,
                emf_b,
                w_hm,
                h_hm,
                w_px,
                h_px,
                prog_id,
                canonical_win,
                cached_proj,
            ) = _fetch_graph_ole_payload(
                session,
                g_nm,
                dpi=dpi,
                tmp_dir=tmp_dir,
                seq_idx=idx,
                cached_project_bytes=cached_proj,
                embed_scope=embed_scope,
                warn_list=warn_list,
                allow_whole_project=allow_whole_project,
                ole_meta=ole_meta,
            )
            graph_info = _graph_info_safe(session, canonical_win)
            merged_info: Optional[Dict[str, Any]] = (
                {**ole_meta, **dict(graph_info)}
                if graph_info is not None
                else (ole_meta or None)
            )
            ole_bin_path = tmp_dir / f"ole_{idx}_{_safe_tmp_component(canonical_win)}.bin"
            build_ole_compound_file(
                ole_bin_path,
                project_bytes=proj_b,
                emf_bytes=emf_b,
                target_window=canonical_win,
                width_hm=w_hm,
                height_hm=h_hm,
                width_px=w_px,
                height_px=h_px,
            )
            built_items.append(
                {
                    "graph_name": g_nm,
                    "canonical_window": canonical_win,
                    "ole_bytes": ole_bin_path.read_bytes(),
                    "emf_bytes": emf_b,
                    "w_hm": w_hm,
                    "h_hm": h_hm,
                    "prog_id": prog_id,
                    "font_pts": _graph_nominal_font_pts(session, canonical_win, merged_info),
                    "slide_scale": _graph_slide_scale(session, canonical_win, merged_info),
                }
            )

        slide_scale_used: List[bool] = []

        def _item_scale(item: Mapping[str, Any]) -> Optional[float]:
            """Placement factor of one graph: scale= / lab preset arg, else its own slide preset."""
            if new_placement_scale_f is not None:
                return new_placement_scale_f
            s = item.get("slide_scale")
            if s is not None and not box_has_explicit_width:
                slide_scale_used.append(True)
                return float(s)
            return None

        # Mode A: Named-shape replacement `replace=[(2, "Picture 3"), ...]`
        if named_replace_targets is not None:
            for idx, (item, (target_snum, target_sname)) in enumerate(
                zip(built_items, named_replace_targets)
            ):
                matched_slide = None
                matched_snum = 0
                matched_el = None
                if target_snum is not None:
                    cand_slide, cand_snum = _resolve_1based_slide(prs, target_snum)
                    matched_el = _find_shape_element_on_slide(
                        cand_slide.shapes._spTree, shape_name=target_sname
                    )
                    if matched_el is not None:
                        matched_slide, matched_snum = cand_slide, cand_snum
                else:
                    for s_i, cand_slide in enumerate(prs.slides, start=1):
                        el = _find_shape_element_on_slide(
                            cand_slide.shapes._spTree, shape_name=target_sname
                        )
                        if el is not None:
                            matched_slide, matched_snum, matched_el = cand_slide, s_i, el
                            break

                if matched_slide is None or matched_el is None:
                    where = f"slide {target_snum}" if target_snum is not None else "presentation"
                    raise KeyError(f"Target shape {target_sname!r} not found on {where}.")

                cur_box = per_graph_boxes[idx] if per_graph_boxes is not None else single_box
                clamped_orig_cm: Optional[Tuple[float, float]] = None
                if cur_box is not None:
                    box_scale = explicit_scale_f if len(cur_box) >= 3 else _item_scale(item)
                    l_e, t_e, w_e, h_e = _explicit_box_emu(
                        cur_box, item["w_hm"], item["h_hm"], box_scale
                    )
                    if len(cur_box) == 2 and box_scale is not None:
                        l_e, t_e, w_e, h_e, clamped_orig_cm = _clamp_scaled_rect_to_slide_emu(
                            l_e, t_e, w_e, h_e, slide_w, slide_h, center=False
                        )
                elif explicit_scale_f is not None:
                    anchor = _extract_shape_xfrm_emu(matched_el) or _compute_grid_cell_emu(
                        slide_w, slide_h, 1, 1, 0
                    )
                    l_e, t_e, w_e, h_e = _scaled_rect_emu(
                        item["w_hm"], item["h_hm"], explicit_scale_f, anchor, center=False
                    )
                    l_e, t_e, w_e, h_e, clamped_orig_cm = _clamp_scaled_rect_to_slide_emu(
                        l_e, t_e, w_e, h_e, slide_w, slide_h, anchor_rect=anchor, center=False
                    )
                else:
                    orig_xfrm = _extract_shape_xfrm_emu(matched_el)
                    if orig_xfrm is not None:
                        l_e, t_e, w_e, h_e = _fit_aspect_inside_box(
                            orig_xfrm[0],
                            orig_xfrm[1],
                            orig_xfrm[2],
                            orig_xfrm[3],
                            item["w_hm"],
                            item["h_hm"],
                        )
                    else:
                        c_l, c_t, c_w, c_h = _compute_grid_cell_emu(slide_w, slide_h, 1, 1, 0)
                        l_e, t_e, w_e, h_e = _fit_aspect_inside_box(
                            c_l, c_t, c_w, c_h, item["w_hm"], item["h_hm"]
                        )

                # Keep target_sname on cNvPr.name AND write descr="torigraph:<graph>" so both
                # repeated `replace=[(2, "Picture 3")]` AND `replace=True` stay 100% idempotent!
                pl = embed_ole_shape_on_slide(
                    prs,
                    matched_slide,
                    ole_bytes=item["ole_bytes"],
                    emf_bytes=item["emf_bytes"],
                    left_emu=l_e,
                    top_emu=t_e,
                    width_emu=w_e,
                    height_emu=h_e,
                    graph_name=item["graph_name"],
                    shape_name=target_sname,
                    prog_id=item["prog_id"],
                    replace_element=matched_el,
                )
                pl["slide"] = matched_snum
                if clamped_orig_cm is not None:
                    pl["_clamped_from_cm"] = clamped_orig_cm
                placements.append(pl)
                touched_slides.add(matched_snum)
                replaced_count += 1
                embedded_count += 1

        else:
            # Mode B / C: Idempotent `alt_text="torigraph:<graph>"` in-place update,
            # explicit `slide=` overlay, or paginated `per_slide` new-slide grid!
            pending_indices: List[int] = []
            for idx, item in enumerate(built_items):
                if auto_replace_alt and len(prs.slides) > 0:
                    search_slides: List[Tuple[int, Any]] = []
                    if slide is not None:
                        s_obj, s_num = _resolve_1based_slide(prs, slide)
                        search_slides.append((s_num, s_obj))
                    else:
                        search_slides = list(enumerate(prs.slides, start=1))

                    found_pair: Optional[Tuple[int, Any, Any]] = None
                    for s_num, s_obj in search_slides:
                        el = _find_shape_element_on_slide(
                            s_obj.shapes._spTree, graph_name=item["graph_name"]
                        )
                        if el is not None:
                            found_pair = (s_num, s_obj, el)
                            break

                    if found_pair is not None:
                        s_num, s_obj, existing_el = found_pair
                        cur_box = per_graph_boxes[idx] if per_graph_boxes is not None else single_box
                        clamped_orig_cm = None
                        if cur_box is not None:
                            box_scale = explicit_scale_f if len(cur_box) >= 3 else _item_scale(item)
                            l_e, t_e, w_e, h_e = _explicit_box_emu(
                                cur_box, item["w_hm"], item["h_hm"], box_scale
                            )
                            if len(cur_box) == 2 and box_scale is not None:
                                l_e, t_e, w_e, h_e, clamped_orig_cm = _clamp_scaled_rect_to_slide_emu(
                                    l_e, t_e, w_e, h_e, slide_w, slide_h, center=False
                                )
                        elif explicit_scale_f is not None:
                            anchor = _extract_shape_xfrm_emu(existing_el) or _compute_grid_cell_emu(
                                slide_w, slide_h, 1, 1, 0
                            )
                            l_e, t_e, w_e, h_e = _scaled_rect_emu(
                                item["w_hm"], item["h_hm"], explicit_scale_f, anchor, center=False
                            )
                            l_e, t_e, w_e, h_e, clamped_orig_cm = _clamp_scaled_rect_to_slide_emu(
                                l_e, t_e, w_e, h_e, slide_w, slide_h, anchor_rect=anchor, center=False
                            )
                        else:
                            orig_xfrm = _extract_shape_xfrm_emu(existing_el)
                            if orig_xfrm is not None:
                                ox, oy, ow, oh = orig_xfrm
                                old_ar = float(ow) / float(max(1, oh))
                                new_ar = float(item["w_hm"]) / float(max(1, item["h_hm"]))
                                if abs(old_ar - new_ar) / max(1e-6, new_ar) <= 0.05:
                                    l_e, t_e, w_e, h_e = ox, oy, ow, oh
                                else:
                                    l_e, t_e, w_e, h_e = _fit_aspect_inside_box(
                                        ox, oy, ow, oh, item["w_hm"], item["h_hm"]
                                    )
                            else:
                                c_l, c_t, c_w, c_h = _compute_grid_cell_emu(slide_w, slide_h, 1, 1, 0)
                                l_e, t_e, w_e, h_e = _fit_aspect_inside_box(
                                    c_l, c_t, c_w, c_h, item["w_hm"], item["h_hm"]
                                )

                        # Preserve existing cNvPr name if customized
                        cnv_existing = existing_el.xpath(
                            "./*[local-name()='nvGraphicFramePr' or local-name()='nvPicPr' or local-name()='nvSpPr']/*[local-name()='cNvPr']"
                        )
                        keep_name = (
                            cnv_existing[0].get("name")
                            if cnv_existing and cnv_existing[0].get("name")
                            else None
                        )
                        pl = embed_ole_shape_on_slide(
                            prs,
                            s_obj,
                            ole_bytes=item["ole_bytes"],
                            emf_bytes=item["emf_bytes"],
                            left_emu=l_e,
                            top_emu=t_e,
                            width_emu=w_e,
                            height_emu=h_e,
                            graph_name=item["graph_name"],
                            shape_name=keep_name,
                            prog_id=item["prog_id"],
                            replace_element=existing_el,
                        )
                        pl["slide"] = s_num
                        if clamped_orig_cm is not None:
                            pl["_clamped_from_cm"] = clamped_orig_cm
                        placements.append(pl)
                        touched_slides.add(s_num)
                        replaced_count += 1
                        embedded_count += 1
                        continue

                pending_indices.append(idx)

            # Place remaining graphs onto explicit `slide` OR newly appended `per_slide` slides
            if pending_indices:
                if slide is not None:
                    if len(prs.slides) == 0 and int(slide) in (0, 1):
                        s_obj = _add_clean_blank_slide(
                            prs, title_list[0] if title_list else None
                        )
                        s_num = 1
                    else:
                        s_obj, s_num = _resolve_1based_slide(prs, slide)

                    eff_rows, eff_cols = _VALID_PER_SLIDE.get(
                        per_slide_int if len(pending_indices) <= per_slide_int else min(6, len(pending_indices)),
                        (n_rows, n_cols),
                    )
                    for local_k, idx in enumerate(pending_indices):
                        item = built_items[idx]
                        cur_box = per_graph_boxes[idx] if per_graph_boxes is not None else single_box
                        clamped_orig_cm = None
                        if cur_box is not None:
                            box_scale = explicit_scale_f if len(cur_box) >= 3 else _item_scale(item)
                            l_e, t_e, w_e, h_e = _explicit_box_emu(
                                cur_box, item["w_hm"], item["h_hm"], box_scale
                            )
                            if len(cur_box) == 2 and box_scale is not None:
                                l_e, t_e, w_e, h_e, clamped_orig_cm = _clamp_scaled_rect_to_slide_emu(
                                    l_e, t_e, w_e, h_e, slide_w, slide_h, center=False
                                )
                        else:
                            c_l, c_t, c_w, c_h = _compute_grid_cell_emu(
                                slide_w,
                                slide_h,
                                eff_rows,
                                eff_cols,
                                local_k % (eff_rows * eff_cols),
                                has_title=bool(title_list),
                            )
                            if _item_scale(item) is not None:
                                l_e, t_e, w_e, h_e = _scaled_rect_emu(
                                    item["w_hm"], item["h_hm"], _item_scale(item), (c_l, c_t, c_w, c_h),
                                    center=True,
                                )
                                l_e, t_e, w_e, h_e, clamped_orig_cm = _clamp_scaled_rect_to_slide_emu(
                                    l_e,
                                    t_e,
                                    w_e,
                                    h_e,
                                    slide_w,
                                    slide_h,
                                    anchor_rect=(c_l, c_t, c_w, c_h),
                                    center=True,
                                )
                            else:
                                l_e, t_e, w_e, h_e = _fit_aspect_inside_box(
                                    c_l, c_t, c_w, c_h, item["w_hm"], item["h_hm"]
                                )
                        pl = embed_ole_shape_on_slide(
                            prs,
                            s_obj,
                            ole_bytes=item["ole_bytes"],
                            emf_bytes=item["emf_bytes"],
                            left_emu=l_e,
                            top_emu=t_e,
                            width_emu=w_e,
                            height_emu=h_e,
                            graph_name=item["graph_name"],
                            prog_id=item["prog_id"],
                        )
                        pl["slide"] = s_num
                        if clamped_orig_cm is not None:
                            pl["_clamped_from_cm"] = clamped_orig_cm
                        placements.append(pl)
                        touched_slides.add(s_num)
                        embedded_count += 1
                else:
                    # Paginate across new slides with `per_slide` cells per slide
                    for page_idx, start_k in enumerate(range(0, len(pending_indices), per_slide_int)):
                        chunk = pending_indices[start_k : start_k + per_slide_int]
                        s_title = (
                            title_list[page_idx]
                            if (title_list is not None and page_idx < len(title_list))
                            else (title_list[0] if (title_list is not None and len(title_list) == 1 and page_idx == 0) else None)
                        )
                        s_obj = _add_clean_blank_slide(prs, s_title)
                        s_num = len(prs.slides)
                        for cell_k, idx in enumerate(chunk):
                            item = built_items[idx]
                            cur_box = per_graph_boxes[idx] if per_graph_boxes is not None else single_box
                            clamped_orig_cm = None
                            if cur_box is not None:
                                box_scale = explicit_scale_f if len(cur_box) >= 3 else _item_scale(item)
                                l_e, t_e, w_e, h_e = _explicit_box_emu(
                                    cur_box, item["w_hm"], item["h_hm"], box_scale
                                )
                                if len(cur_box) == 2 and box_scale is not None:
                                    l_e, t_e, w_e, h_e, clamped_orig_cm = _clamp_scaled_rect_to_slide_emu(
                                        l_e, t_e, w_e, h_e, slide_w, slide_h, center=False
                                    )
                            else:
                                c_l, c_t, c_w, c_h = _compute_grid_cell_emu(
                                    slide_w,
                                    slide_h,
                                    n_rows,
                                    n_cols,
                                    cell_k,
                                    has_title=bool(s_title),
                                )
                                if _item_scale(item) is not None:
                                    l_e, t_e, w_e, h_e = _scaled_rect_emu(
                                        item["w_hm"], item["h_hm"], _item_scale(item),
                                        (c_l, c_t, c_w, c_h), center=True,
                                    )
                                    l_e, t_e, w_e, h_e, clamped_orig_cm = _clamp_scaled_rect_to_slide_emu(
                                        l_e,
                                        t_e,
                                        w_e,
                                        h_e,
                                        slide_w,
                                        slide_h,
                                        anchor_rect=(c_l, c_t, c_w, c_h),
                                        center=True,
                                    )
                                else:
                                    l_e, t_e, w_e, h_e = _fit_aspect_inside_box(
                                        c_l, c_t, c_w, c_h, item["w_hm"], item["h_hm"]
                                    )
                            pl = embed_ole_shape_on_slide(
                                prs,
                                s_obj,
                                ole_bytes=item["ole_bytes"],
                                emf_bytes=item["emf_bytes"],
                                left_emu=l_e,
                                top_emu=t_e,
                                width_emu=w_e,
                                height_emu=h_e,
                                graph_name=item["graph_name"],
                                prog_id=item["prog_id"],
                            )
                            pl["slide"] = s_num
                            if clamped_orig_cm is not None:
                                pl["_clamped_from_cm"] = clamped_orig_cm
                            placements.append(pl)
                            touched_slides.add(s_num)
                            embedded_count += 1

    # Effective size and font size of every placed graph (what the audience will see).
    items_by_graph = {it["graph_name"]: it for it in built_items}
    for pl in placements:
        item = items_by_graph.get(pl.get("graph_name") or pl.get("graph"))
        if item is None:
            continue
        nat_w_emu = item["w_hm"] * EMU_PER_HM
        nat_h_emu = item["h_hm"] * EMU_PER_HM
        try:
            w_emu = float(pl["width_cm"]) * EMU_PER_CM
            h_emu = float(pl["height_cm"]) * EMU_PER_CM
            l_emu = float(pl["left_cm"]) * EMU_PER_CM
            t_emu = float(pl["top_cm"]) * EMU_PER_CM
        except (KeyError, TypeError, ValueError):
            continue
        eff = float(w_emu) / float(max(1, nat_w_emu))
        pl["effective_scale"] = round(eff, 4)
        pl["natural_width_cm"] = round(nat_w_emu / EMU_PER_CM, 3)
        pl["natural_height_cm"] = round(nat_h_emu / EMU_PER_CM, 3)
        pts = item.get("font_pts") or {}
        for key in ("axis_title_pt", "tick_label_pt"):
            nominal = pts.get(key)
            pl["nominal_" + key] = nominal
            pl["effective_" + key] = round(nominal * eff, 2) if nominal else None
        clamped_from = pl.pop("_clamped_from_cm", None)
        tol = EMU_PER_MM  # rounding of the cm values
        if clamped_from is not None:
            pl["clamped"] = True
            msg = (
                f"'{item['graph_name']}' ({clamped_from[0]:.1f} x {clamped_from[1]:.1f} cm) "
                f"extends beyond slide {pl.get('slide')} and was clamped to "
                f"{pl['width_cm']:.1f} x {pl['height_cm']:.1f} cm."
            )
            warn_list.append(msg)
            warnings.warn(msg, UserWarning, stacklevel=2)
        elif (
            l_emu < -tol
            or t_emu < -tol
            or l_emu + w_emu > slide_w + tol
            or t_emu + h_emu > slide_h + tol
        ):
            msg = (
                f"'{item['graph_name']}' ({pl['width_cm']:.1f} x {pl['height_cm']:.1f} cm) "
                f"extends beyond slide {pl.get('slide')}."
            )
            warn_list.append(msg)
            warnings.warn(msg, UserWarning, stacklevel=2)

    _atomic_save_presentation(prs, out_path)

    return {
        "ok": True,
        "path": os.fspath(out_path),
        "graph_count": len(graph_names),
        "graphs": graph_names,
        "slide_count": len(prs.slides),
        "slides_modified": sorted(touched_slides),
        "embedded_count": embedded_count,
        "replaced_count": replaced_count,
        "clsid": TORIGRAPH_OLE_CLSID_STR,
        "prog_id": TORIGRAPH_OLE_PROG_ID,
        "placements": placements,
        "scale": explicit_scale_f if (replaced_count > 0 and replaced_count == embedded_count) else (new_placement_scale_f if new_placement_scale_f is not None else (LAB_SLIDE_PLACEMENT_SCALE if slide_scale_used else None)),
        "embed_scope": embed_scope,
        "effective_scales": [pl.get("effective_scale") for pl in placements],
        "warnings": warn_list,
    }
