#!/bin/bash
# btrfs-bitmap-bench.sh - worst-case bitmap benchmark for partclone.btrfs
#
# Builds a snapshot-heavy btrfs image (snapper-like worst case: many
# snapshots sharing the same extents) using libguestfs (no root needed),
# then runs each given partclone.btrfs binary in clone mode and:
#   1. reports wall/user/sys time per run,
#   2. compares every produced image bit-for-bit against the FIRST
#      binary's image (the reference), to prove no bitmap behavior change.
#
# Usage:
#   btrfs-bitmap-bench.sh [-i IMAGE] [-s SNAPSHOTS] [-m SIZE_MB] [-d DATA_MB] \
#                         [-k] [-V] [-R] [-n] BINARY [BINARY...]
#   -k : keep existing IMAGE if present (default: rebuild)
#   -V : verify each image against the fs's own extent tree (add when mixing
#        binaries whose images legitimately differ, e.g. extent-tree builds)
#   -R : with -V, also restore each image and byte-compare used extents
#   -n : no bit-compare gate against the first binary's image
# Example:
#   tests/btrfs-bitmap-bench.sh /path/master/partclone.btrfs src/partclone.btrfs
set -euo pipefail

IMG=/tmp/opencode/btrfs-worst.img
SNAPS=64
SIZE_MB=2048
DATA_MB=384
KEEP=0
VERIFY=0
RESTORE=0
COMPARE=1
SCRIPTDIR="$(cd "$(dirname "$0")" && pwd)"
WORKDIR="$(mktemp -d /tmp/opencode/btbench.XXXXXX)"

while getopts "i:s:m:d:kVRn" o; do
    case "$o" in
	i) IMG="$OPTARG";;
	s) SNAPS="$OPTARG";;
	m) SIZE_MB="$OPTARG";;
	d) DATA_MB="$OPTARG";;
	k) KEEP=1;;
	V) VERIFY=1;;
	R) RESTORE=1; VERIFY=1;;
	n) COMPARE=0;;
	*) echo "unknown option" >&2; exit 2;;
    esac
done
shift $((OPTIND-1))
[ $# -ge 1 ] || { echo "need at least one partclone.btrfs binary" >&2; exit 2; }

command -v guestfish >/dev/null || { echo "guestfish required" >&2; exit 1; }
command -v mkfs.btrfs >/dev/null || { echo "mkfs.btrfs required" >&2; exit 1; }

if [ "$KEEP" = 0 ] || [ ! -f "$IMG" ]; then
    echo "== building worst-case image: $IMG (${SIZE_MB}MiB, ${DATA_MB}MiB shared data, ${SNAPS} snapshots)"
    rm -f "$IMG"
    dd if=/dev/zero of="$IMG" bs=1M count="$SIZE_MB" status=none
    mkfs.btrfs -q -f "$IMG"
    # guestfish script: files of 16 MiB each in /data, snapshot after each
    # small churn step so snapshots share almost everything.
    {
	echo "run"
	echo "mount /dev/sda /"
	echo "mkdir /data"
	echo "mkdir /.snapshots"
	nfiles=$(( (DATA_MB + 15) / 16 ))
	for i in $(seq 1 "$nfiles"); do
	    echo "fill $((64 + i % 32)) 16777216 /data/blob$i"
	done
	# many small one-sector files: each costs one EXTENT_DATA item that
	# every snapshot's fs tree re-enumerates; this is what makes the
	# snapshot-heavy walk quadratic-ish in practice.
	# snapshot-create speed in the appliance makes 4096 too slow, so keep
	# it bounded; increase via -n if the box is fast.
	smallfiles=${SMALLFILES:-1024}
	for i in $(seq 1 "$smallfiles"); do
	    echo "fill $((33 + i % 16)) 4096 /data/tiny$i"
	done
	for s in $(seq 1 "$SNAPS"); do
	    # churn: overwrite 512 KiB in one file and add a tiny file per snap
	    echo "fill $((96 + s % 16)) 524288 /data/churn"
	    echo "btrfs-subvolume-snapshot / /.snapshots/snap$s"
	done
	echo "umount /"
	echo "exit"
    } > "$WORKDIR/build.gf"
    guestfish --rw -a "$IMG" -f "$WORKDIR/build.gf"
fi

echo "== source fs summary"
btrfs inspect-internal dump-super "$IMG" 2>/dev/null | grep -E "bytes_used|generation" || true

REF=""
REFIMG=""
RESULTS=()
for bin in "$@"; do
    name="$(basename "$(dirname "$(dirname "$bin")")").$(basename "$bin")"
    name="${name//\//_}.$(echo "$bin" | md5sum | cut -c1-6)"
    out="$WORKDIR/$name.pcl"
    tlog="$WORKDIR/$name.time"
    echo "== cloning with $bin"
    /usr/bin/time -f "%e %U %S" -o "$tlog" "$bin" -c -a0 -s "$IMG" -O "$out" -L /dev/null >/dev/null 2>&1
    read -r wall user sys < "$tlog"
    sum="$(sha256sum "$out" | awk '{print $1}')"
    if [ -z "$REFIMG" ]; then
	REFIMG="$out"; REF="$sum"; refname="$name"; verdict="(reference)"
    elif [ "$COMPARE" = 1 ]; then
	if [ "$sum" = "$REF" ]; then verdict="OK  bit-identical to $refname"; else verdict="FAIL differs from $refname"; fi
    else
	verdict="(no bit-compare)"
    fi
    echo "   wall=${wall}s user=${user}s sys=${sys}s sha=$sum $verdict"
    RESULTS+=("$verdict")

    if [ "$VERIFY" = 1 ]; then
	raw="$WORKDIR/$name.raw"
	if [ "$RESTORE" = 1 ]; then
	    dd if=/dev/zero of="$raw" bs=1M count="$SIZE_MB" status=none
	    "$bin" -r -C -s "$out" -O "$raw" -L /dev/null >/dev/null 2>&1
	    if python3 "$SCRIPTDIR/btrfs-bitmap-verify.py" "$out" "$IMG" "$raw" \
		> "$WORKDIR/$name.verify" 2>&1; then
		RESULTS+=("verify+restore OK $name")
	    else
		RESULTS+=("verify FAIL $name (see $WORKDIR/$name.verify)")
	    fi
	else
	    if python3 "$SCRIPTDIR/btrfs-bitmap-verify.py" "$out" "$IMG" \
		> "$WORKDIR/$name.verify" 2>&1; then
		RESULTS+=("verify OK $name")
	    else
		RESULTS+=("verify FAIL $name (see $WORKDIR/$name.verify)")
	    fi
	fi
	tail -1 "$WORKDIR/$name.verify" | sed 's/^/   /'
    fi
done

echo
for v in "${RESULTS[@]}"; do
    case "$v" in
	*FAIL*) echo "RESULT: $v"; exit 1;;
    esac
done
if [ "$COMPARE" = 1 ]; then
    echo "RESULT: all images bit-identical"
else
    echo "RESULT: all checks passed"
fi
echo "workdir: $WORKDIR"
