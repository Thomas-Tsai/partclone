#!/bin/bash
# btrfs-file-cksum-check.sh - file-level checksum verification after restore.
#
# Mounts each given btrfs device read-only via guestmount (libguestfs, no
# root needed) and compares, against the FIRST device (the reference):
#   1. the complete file list of the whole tree (all snapshots included),
#   2. sha256 of every file outside /.snapshots (the root subvolume),
#   3. sha256 of every file in one full snapshot (.snapshots/snap50 by
#      default, SNAPSHOT= to override).
#
# Usage: btrfs-file-cksum-check.sh SOURCE_DEV RESTORED_DEV [RESTORED_DEV...]
set -euo pipefail

SNAPSUB="${SNAPSHOT:-.snapshots/snap50}"
[ $# -ge 2 ] || { echo "usage: $0 SOURCE_DEV RESTORED_DEV..." >&2; exit 2; }

W="$(mktemp -d /tmp/opencode/ckchk.XXXXXX)"
mkdir -p "$W/mnt"

collect() {
    local dev="$1" tag="$2"
    guestmount --ro -a "$dev" -m /dev/sda "$W/mnt"
    ( cd "$W/mnt" && {
	find . -type f | sort > "$W/files.$tag"
	find . -path ./.snapshots -prune -o -type f -print0 | sort -z \
	    | xargs -0 sha256sum > "$W/cksums.$tag"
	if [ -d "$SNAPSUB" ]; then
	    find "./$SNAPSUB" -type f -print0 | sort -z \
		| xargs -0 sha256sum | sed "s|\./$SNAPSUB/|@snap/|" > "$W/cksumsnap.$tag"
	fi
    } )
    guestunmount "$W/mnt"
}

fail=0
collect "$1" ref
shift
for dev in "$@"; do
    tag="$(basename "$dev")"
    echo "== $dev"
    collect "$dev" "$tag"
    cmp -s "$W/files.ref" "$W/files.$tag" \
	&& echo "   file list: identical ($(wc -l < "$W/files.$tag") files)" \
	|| { echo "   file list: DIFFERS"; fail=1; }
    cmp -s "$W/cksums.ref" "$W/cksums.$tag" \
	&& echo "   root-subvol sha256 ($(wc -l < "$W/cksums.$tag") files): identical" \
	|| { echo "   root-subvol sha256: DIFFERS"; fail=1; }
    if [ -s "$W/cksumsnap.ref" ]; then
	cmp -s "$W/cksumsnap.ref" "$W/cksumsnap.$tag" \
	    && echo "   $SNAPSUB sha256 ($(wc -l < "$W/cksumsnap.$tag") files): identical" \
	    || { echo "   $SNAPSUB sha256: DIFFERS"; fail=1; }
    fi
done
echo "workdir: $W"
[ "$fail" = 0 ] && echo "RESULT: all file checksums identical" || echo "RESULT: FAILURES above"
exit "$fail"
