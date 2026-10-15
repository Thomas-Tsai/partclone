#!/usr/bin/env python3
"""zfs-cleanroom-check.py - clean-room hygiene checks for src/zfs/ + zfsclone.c

Implements the checks called for in src/zfs/clean-room-req.md (sections
3.5 "static analysis & copyright check" and 6 "continuous legality"):

  1. forbidden-marker scan:
     no CDDL license headers / OpenZFS copyright lines may appear in the
     clean-room sources.
  2. code-similarity check:
     token-shingle containment of each clean-room file against a local
     OpenZFS source tree. Only metadata-parsing subsystems are compared
     (spa/vdev/metaslab/space_map/zap/dmu/dnode/nvpair). Fails when the
     containment ratio exceeds --fail-threshold percent (default 20, per
     section 3.5 of the requirements doc).
  3. static analysis (informational, never fails the run):
     cppcheck --enable=all over the clean-room sources, when available.

Usage:
  tests/zfs-cleanroom-check.py --openzfs-src /path/to/openzfs-checkout
  tests/zfs-cleanroom-check.py                       # scans 1+3 only

OpenZFS sources:  git clone --depth 1 https://github.com/openzfs/zfs

Exit status: 0 = clean, 1 = finding(s) above threshold, 2 = bad usage.
"""

import argparse
import glob
import os
import re
import shutil
import subprocess
import sys

RE_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

CLEANROOM_GLOBS = (
    "src/zfs/*.c",
    "src/zfs/*.h",
    "src/zfsclone.c",
    "src/zfsclone.h",
)

FORBIDDEN = (
    (re.compile(r"COMMON DEVELOPMENT AND DISTRIBUTION LICENSE", re.I),
     "CDDL license text"),
    (re.compile(r"SPDX-License-Identifier:\s*CDDL", re.I),
     "CDDL SPDX tag"),
    (re.compile(r"copyright.{0,80}sun microsystems", re.I),
     "Sun Microsystems copyright"),
    (re.compile(r"copyright.{0,80}lawrence livermore", re.I),
     "Lawrence Livermore copyright"),
    (re.compile(r"copyright.{0,80}(oracle|delphix)", re.I),
     "Oracle/Delphix copyright"),
)

# OpenZFS files an implementer of this reader could plausibly have
# consulted; comparing against these keeps the corpus (and runtime) small.
OPENZFS_COMPARE_GLOBS = (
    "module/zfs/spa*.c",
    "module/zfs/vdev*.c",
    "module/zfs/metaslab*.c",
    "module/zfs/space_map*.c",
    "module/zfs/uberblock.c",
    "module/zfs/zap*.c",
    "module/zfs/dmu*.c",
    "module/zfs/dnode*.c",
    "module/zfs/dsl_*.c",
    "module/zfs/blkptr.c",
    "module/zfs/zio_compress.c",
    "module/zfs/lz4*.c",
    "module/zfs/lzjb*.c",
    "module/nvpair/*.c",
    "include/sys/spa*.h",
    "include/sys/vdev*.h",
    "include/sys/metaslab*.h",
    "include/sys/space_map*.h",
    "include/sys/uberblock*.h",
    "include/sys/zap*.h",
    "include/sys/dmu*.h",
    "include/sys/dnode*.h",
    "include/sys/blkptr*.h",
    "include/sys/zio_compress*.h",
    "include/sys/nvpair*.h",
    "include/sys/fs/zfs.h",
)

SHINGLE_LEN = 8

COMMENT_RE = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
TOKEN_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*|0[xX][0-9a-fA-F]+|\d+|[^\s]")


def tokens_of(path, keep_strings=False):
    with open(path, encoding="utf-8", errors="replace") as f:
        text = f.read()
    text = COMMENT_RE.sub(" ", text)
    if not keep_strings:
        # on-disk key strings ("vdev_tree", "com.delphix:*") are format
        # facts any reader must share; folding them avoids false hits.
        text = STRING_RE.sub(" ", text)
    return TOKEN_RE.findall(text)


def shingle_hashes(tokens, n=SHINGLE_LEN):
    if len(tokens) < n:
        return set()
    return {hash(tuple(tokens[i:i + n])) for i in range(len(tokens) - n + 1)}


def expand_globs(root, patterns):
    out = []
    for pat in patterns:
        out.extend(glob.glob(os.path.join(root, pat)))
    return sorted(set(out))


def scan_forbidden(paths):
    print("== 1. forbidden-marker scan ==")
    findings = 0
    for path in paths:
        with open(path, encoding="utf-8", errors="replace") as f:
            for lineno, line in enumerate(f, 1):
                for rx, what in FORBIDDEN:
                    if rx.search(line):
                        print("  HIT %s:%d: %s: %s"
                              % (path, lineno, what, line.strip()))
                        findings += 1
    print("  %d file(s) scanned, %d finding(s)\n" % (len(paths), findings))
    return findings


def scan_similarity(clean_paths, openzfs_src, threshold):
    print("== 2. code-similarity check ==")
    corpus_paths = expand_globs(openzfs_src, OPENZFS_COMPARE_GLOBS)
    if not corpus_paths:
        print("  no OpenZFS files matched under %s" % openzfs_src)
        print("  (expected e.g. %s; is this an OpenZFS checkout?)\n"
              % OPENZFS_COMPARE_GLOBS[0])
        return None

    corpus = {}
    union = set()
    for path in corpus_paths:
        s = shingle_hashes(tokens_of(path))
        if s:
            corpus[path] = s
            union |= s
    print("  corpus: %d file(s), %d shingle(s)"
          % (len(corpus), len(union)))

    worst = 0.0
    failures = 0
    for path in clean_paths:
        tokens = tokens_of(path)
        mine = shingle_hashes(tokens)
        if not mine:
            print("  %-28s skipped (too small)" % path)
            continue
        ratio = 100.0 * len(mine & union) / len(mine)
        worst = max(worst, ratio)
        tag = "OK  " if ratio <= threshold else "FAIL"
        print("  %s %-28s %6.2f%% containment" % (tag, path, ratio))
        if ratio > threshold:
            failures += 1
            offenders = sorted(
                ((len(mine & s), p) for p, s in corpus.items() if mine & s),
                reverse=True)[:3]
            for hits, p in offenders:
                print("       <- %5d shingle(s) vs %s"
                      % (hits, os.path.relpath(p, openzfs_src)))
            # show one sample matching token run for human review
            n = SHINGLE_LEN
            ordered = [tuple(tokens[i:i + n])
                       for i in range(len(tokens) - n + 1)]
            sample = next((t for t in ordered if hash(t) in union), None)
            if sample:
                print("       sample: %s" % " ".join(sample))
    print("  worst containment %.2f%% (threshold %.2f%%)\n"
          % (worst, threshold))
    return failures


def run_cppcheck(clean_paths):
    print("== 3. cppcheck (informational) ==")
    exe = shutil.which("cppcheck")
    if exe is None:
        print("  cppcheck not found, skipped\n")
        return
    cmd = [exe, "--enable=all", "--std=c99", "--quiet",
           "--template={file}:{line}: {severity}: {message}"] + clean_paths
    proc = subprocess.run(cmd, capture_output=True, text=True)
    out = (proc.stdout + proc.stderr).strip()
    print("  " + "\n  ".join(out.splitlines()) if out else "  no findings")
    print()


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--openzfs-src", metavar="DIR",
                    help="path to an OpenZFS source checkout; enables "
                         "the similarity check")
    ap.add_argument("--fail-threshold", type=float, default=20.0,
                    help="containment percent that fails the run "
                         "(default: 20)")
    ap.add_argument("--no-cppcheck", action="store_true",
                    help="skip the cppcheck pass")
    args = ap.parse_args()

    clean_paths = expand_globs(RE_ROOT, CLEANROOM_GLOBS)
    if not clean_paths:
        print("no clean-room sources found (looked for %s under %s)"
              % (CLEANROOM_GLOBS, RE_ROOT))
        return 2

    status = 0
    if scan_forbidden(clean_paths):
        status = 1

    if args.openzfs_src:
        failures = scan_similarity(clean_paths, args.openzfs_src,
                                   args.fail_threshold)
        if failures is None:
            return 2
        if failures:
            status = 1
    else:
        print("== 2. code-similarity check ==\n"
              "  skipped (pass --openzfs-src DIR to enable)\n")

    if not args.no_cppcheck:
        run_cppcheck(clean_paths)

    print("RESULT: %s" % ("CLEAN" if status == 0 else "FINDINGS - review "
          "required (see src/zfs/clean-room-req.md section 3.5)"))
    return status


if __name__ == "__main__":
    sys.exit(main())
