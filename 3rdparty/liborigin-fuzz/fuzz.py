#!/usr/bin/env python3
"""Mutation fuzzer for the liborigin .opj parser (standalone harness).

    python3 3rdparty/liborigin-fuzz/fuzz.py --harness <out>/opj_fuzz \
        --work <scratch-dir> [--seeds test/Histo.opj test/USstates.opj] \
        [--count 3000] [--jobs 2] [--timeout 20] [--rng 1]

Generates deterministic mutants of the seed files (truncation at many offsets,
random byte flips, corruption of the "<uint32 size>\\n" object-size fields and
their delimiters, chunk deletion/duplication), runs every mutant in its own
process under ASan+UBSan with a per-file timeout, and groups failures by
(sanitizer error kind, first liborigin stack frame). Hangs (timeout) and
huge allocations (ASan allocation-size-too-big / out-of-memory with
max_allocation_size_mb) are reported as failures too.

Standard library only.
"""
import argparse
import concurrent.futures as cf
import os
import random
import re
import struct
import subprocess
import sys

ASAN_OPTIONS = ("allocator_may_return_null=0:max_allocation_size_mb=512:"
                "detect_leaks=1:abort_on_error=0:symbolize=1:handle_abort=1")
UBSAN_OPTIONS = "print_stacktrace=1:halt_on_error=1"
SRC_RE = re.compile(r"(Origin\w*\.(?:cpp|h)|endianfstream\.hh|tree\.hh):(\d+)")


def size_fields(data):
    """Walk the leading chain of '<u32>\\n<blob>\\n' objects and return offsets
    of the size fields (best effort; stops at the first irregularity)."""
    out = []
    p = data.find(b"\n") + 1
    while 0 < p and p + 5 <= len(data):
        if data[p + 4:p + 5] != b"\n":
            break
        (n,) = struct.unpack_from("<I", data, p)
        out.append(p)
        if n == 0:
            p += 5
        else:
            if p + 5 + n >= len(data):
                break
            p += 5 + n + 1
    # also add heuristic matches anywhere in the file (catches fields inside
    # windows/layers/notes/project tree that the linear walk does not reach)
    for m in re.finditer(rb"[\x00-\xff]{2}\x00\x00\n", data):
        out.append(m.start())
    return sorted(set(out))


def mutants(seed_path, data, count, rng):
    name = os.path.splitext(os.path.basename(seed_path))[0]
    n = len(data)
    fields = size_fields(data)
    k = 0
    # 1) truncations: evenly spaced + random + right after every 50th field
    nt = count // 4
    offs = set(int(n * i / (nt // 2)) for i in range(1, nt // 2))
    offs.add(int(n * 0.7))
    while len(offs) < nt:
        offs.add(rng.randrange(1, n))
    for f in fields[::50]:
        offs.update((f + 2, f + 4, f + 5, f + 6))
    for o in sorted(offs):
        yield "%s_trunc_%d" % (name, o), data[:o]
        k += 1
    # 2) size-field corruption
    special = [0, 1, 2, 3, 4, 7, 0x10, 0x1B, 0x58, 0x71, 0xFF, 0xFFFF, 0x7FFFFFFF,
               0x80000000, 0xFFFFFFFF, 0xFFFFFFFE, n, n * 2]
    for i in range(count // 3):
        f = rng.choice(fields)
        (old,) = struct.unpack_from("<I", data, f)
        r = rng.random()
        if r < 0.5:
            v = rng.choice(special)
        elif r < 0.8:
            v = max(0, old + rng.randint(-16, 16))
        elif r < 0.9:
            v = rng.randrange(0, 1 << 32)
        else:
            v = None  # break the '\n' delimiter
        b = bytearray(data)
        if v is None:
            b[f + 4] = rng.choice([0, 0x20, 0x0D, 0xFF])
            tag = "delim"
        else:
            struct.pack_into("<I", b, f, v & 0xFFFFFFFF)
            tag = "size%x" % (v & 0xFFFFFFFF)
        if rng.random() < 0.3:  # combine with truncation
            b = b[:rng.randrange(f, n)]
            tag += "_t"
        yield "%s_field_%d_%s_%d" % (name, f, tag, i), bytes(b)
    # 3) random byte flips / chunk ops for the remainder
    i = 0
    while True:
        b = bytearray(data)
        op = rng.random()
        # half of the mutants target the tail (windows, layers, annotations,
        # curves, notes, project tree, attachments) instead of the datasets
        lo = 0 if rng.random() < 0.5 else int(n * 0.6)
        if op < 0.6:
            for _ in range(rng.choice([1, 2, 4, 8, 32])):
                p = rng.randrange(lo, n)
                b[p] = rng.randrange(256) if rng.random() < 0.5 else b[p] ^ (1 << rng.randrange(8))
            tag = "flip"
        elif op < 0.8:
            p = rng.randrange(lo, n)
            ln = rng.randrange(1, 4096)
            del b[p:p + ln]
            tag = "del"
        else:
            p = rng.randrange(n)
            ln = rng.randrange(1, 4096)
            b[p:p] = b[rng.randrange(n):][:ln]
            tag = "dup"
        yield "%s_%s_%d" % (name, tag, i), bytes(b)
        i += 1


def classify(rc, out):
    if rc == 124 or rc == -9:
        return "HANG/timeout"
    m = re.search(r"ERROR: AddressSanitizer: ([\w-]+)", out)
    kind = None
    if "requested allocation size" in out or "allocation-size-too-big" in out:
        kind = "asan:allocation-size-too-big"
    elif "out of memory" in out or "out-of-memory" in out:
        kind = "asan:out-of-memory"
    elif m:
        kind = "asan:" + m.group(1)
    else:
        m = re.search(r"(\S+):(\d+):\d+: runtime error: ([^\n]*)", out)
        if m:
            msg = re.sub(r"-?\d+|'[^']*'|0x[0-9a-f]+", "N", m.group(3))[:60]
            kind = "ubsan:%s:%s: %s" % (os.path.basename(m.group(1)), m.group(2), msg)
        elif "ERROR: LeakSanitizer" in out:
            kind = "lsan:leak"
    if kind is None:
        if rc != 0:
            kind = "rc=%d" % rc
        else:
            return None
    m = SRC_RE.search(out[out.find("ERROR"):] if "ERROR" in out else out)
    where = "%s:%s" % (m.group(1), m.group(2)) if m else "?"
    return "%s @ %s" % (kind, where)


def run_one(harness, path, timeout):
    env = dict(os.environ, ASAN_OPTIONS=ASAN_OPTIONS, UBSAN_OPTIONS=UBSAN_OPTIONS)
    try:
        p = subprocess.run(["timeout", "-s", "KILL", str(timeout), harness, path],
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, env=env)
        rc, err = p.returncode, p.stderr.decode("utf-8", "replace")
    except Exception as e:  # pragma: no cover
        return path, "harness-error:%s" % e, ""
    if rc == -9 or rc == 137:
        rc = 124
    return path, classify(rc, err), err


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--harness", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--seeds", nargs="+", default=["test/Histo.opj", "test/USstates.opj"])
    ap.add_argument("--count", type=int, default=1500, help="mutants per seed")
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--timeout", type=int, default=20)
    ap.add_argument("--rng", type=int, default=1)
    ap.add_argument("--corpus", help="re-run an existing corpus dir instead of generating")
    args = ap.parse_args()

    corpus = args.corpus or os.path.join(args.work, "corpus")
    crashes = os.path.join(args.work, "crashes")
    os.makedirs(corpus, exist_ok=True)
    os.makedirs(crashes, exist_ok=True)
    files = []
    if args.corpus:
        files = sorted(os.path.join(corpus, f) for f in os.listdir(corpus))
    else:
        rng = random.Random(args.rng)
        for s in args.seeds:
            data = open(s, "rb").read()
            gen = mutants(s, data, args.count, rng)
            for _ in range(args.count):
                nm, blob = next(gen)
                p = os.path.join(corpus, nm + ".opj")
                with open(p, "wb") as fh:
                    fh.write(blob)
                files.append(p)

    classes = {}
    done = 0
    with cf.ThreadPoolExecutor(args.jobs) as ex:
        for path, cls, err in ex.map(lambda f: run_one(args.harness, f, args.timeout), files):
            done += 1
            if cls:
                lst = classes.setdefault(cls, [])
                lst.append(path)
                if len(lst) == 1:
                    tag = re.sub(r"[^\w.-]+", "_", cls)[:100]
                    with open(os.path.join(crashes, tag + ".log"), "w") as fh:
                        fh.write(path + "\n" + err)
            if done % 250 == 0:
                print("  %d/%d inputs, %d failing" % (done, len(files),
                      sum(len(v) for v in classes.values())), file=sys.stderr, flush=True)
    total = sum(len(v) for v in classes.values())
    print("inputs=%d failing=%d classes=%d" % (len(files), total, len(classes)))
    for cls, lst in sorted(classes.items(), key=lambda kv: -len(kv[1])):
        print("%5d  %s   e.g. %s" % (len(lst), cls, os.path.basename(lst[0])))
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
