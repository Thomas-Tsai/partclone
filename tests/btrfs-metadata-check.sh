#!/bin/bash
# btrfs-metadata-check.sh - metadata preservation check for partclone.btrfs.
#
# Builds a small btrfs image packed with metadata fixtures (modes, owners,
# symlinks, hardlinks, xattrs incl. empty/multiline/binary-ish values,
# read-only+rw snapshots, quota qgroups), then for each given partclone.btrfs:
# clone -> restore -> dump every file's metadata (mode/uid/gid/size/type/
# nlink/mtime/target/xattrs) plus subvolume ro flags and qgroup state from
# the restored device, and diff it against the source device's dump.
#
# Usage: btrfs-metadata-check.sh [-i IMG] [-k] BINARY [BINARY...]
#   -k : keep/build-once (default: rebuild image each run)
set -euo pipefail

IMG=/tmp/opencode/btrfs-meta.img
KEEP=0
while getopts "i:k" o; do
    case "$o" in
	i) IMG="$OPTARG";;
	k) KEEP=1;;
	*) exit 2;;
    esac
done
shift $((OPTIND-1))
[ $# -ge 1 ] || { echo "need at least one partclone.btrfs binary" >&2; exit 2; }

W="$(mktemp -d /tmp/opencode/metachk.XXXXXX)"
SIZE_MB=256

if [ "$KEEP" = 0 ] || [ ! -f "$IMG" ]; then
    rm -f "$IMG"
    truncate -s ${SIZE_MB}M "$IMG"
    mkfs.btrfs -q -f "$IMG"
    BIG=$(printf 'X%.0s' $(seq 1 1200))
    guestfish --rw -a "$IMG" <<EOF
run
mount /dev/sda /
mkdir /d700
chmod 0700 /d700
chown 1234 4321 /d700
mkdir /d750
chmod 0750 /d750
write /d700/a.txt "alpha"
write /d700/b.bin "0 1 2 3"
chmod 0640 /d700/a.txt
chmod 0600 /d700/b.bin
chown 100 200 /d700/a.txt
chown 0 0 /d700/b.bin
setxattr user.author "alice; the builder" 18 /d700/a.txt
setxattr user.empty "" 0 /d700/a.txt
setxattr user.big "$BIG" 1200 /d700/a.txt
setxattr user.role "build\none" 10 /d750
setxattr user.dir "yes" 3 /d700
ln-s /d700/a.txt /link.rel
ln-s ../d700 /d750/link.up
ln /d700/a.txt /d700/hard.txt
mkdir /.snapshots
btrfs-subvolume-snapshot / /.snapshots/rw1
btrfs-subvolume-snapshot / /.snapshots/ro1 ro:true
btrfs-subvolume-snapshot / /.snapshots/ro2 ro:true
btrfs-quota-enable / true
btrfs-quota-rescan /
btrfs-qgroup-create 1/1000 /
btrfs-qgroup-limit / 1073741824
sync
umount /
EOF
    # best-effort: xattr on symlink itself (kernel-dependent; ignore failure)
    guestfish --rw -a "$IMG" run : mount /dev/sda / : lsetxattr user.lnk "on-symlink" 10 /link.rel : sync : umount / >/dev/null 2>&1 || true
fi

# dump_metadata DEVICE OUTPUT
# one deterministic text stream with all metadata of the filesystem;
# uses two appliance sessions: one to enumerate, one for the full dump.
dump_metadata() {
    local dev="$1" out="$2"
    guestfish --rw -a "$dev" run : mount-ro /dev/sda / : btrfs-subvolume-list / : find / > "$W/enum" 2>&1
    sed 's|^/||' "$W/enum" | grep -vE '^(\[|\}| *btrfssubvolume|$)' | sort | uniq > "$W/files"
    grep -oE "btrfssubvolume_path: .*" "$W/enum" | cut -d' ' -f2 | sort -u > "$W/subvols"
    # "nested snapshot roots" (a subvolume whose path has a subvolume
    # ancestor): the kernel synthesizes their root-dir mtime/ctime at mount
    # time -- the on-disk bytes are stable (verified by image sha256), but
    # every mount reports a different value, so they must be filtered out
    # of the comparison or it can never pass.
    : > "$W/nested"
    # a path P is a nested snapshot root when P = Q + "/" + S where Q is a
    # listed subvolume ancestor and S is itself a listed subvolume path
    # (such nested roots are hidden from `subvolume list`)
    while read -r p; do
	while read -r q; do
	    case "$p" in
		"$q"/*)
		    rest="${p#$q/}"
		    while read -r s; do
			[ "$rest" = "$s" ] && { echo "$p" >> "$W/nested"; break 2; }
		    done < "$W/subvols"
		    ;;
	    esac
	done < "$W/subvols"
    done < "$W/files"
    sort -u "$W/nested" -o "$W/nested" 2>/dev/null || true
    {
	echo "run"; echo "mount-ro /dev/sda /"
	echo "btrfs-subvolume-list /"
	while read -r sv; do
	    printf 'echo "SUBVOL-FLAGS %s"\n' "$sv"
	    printf 'btrfs-subvolume-show "/%s"\n' "$sv"
	done < "$W/subvols"
	echo "btrfs-qgroup-show /"
	while read -r p; do
	    printf 'echo "PATH /%s"\n' "$p"
	    printf 'lstatns "/%s"\n' "$p"
	    printf 'lgetxattrs "/%s"\n' "$p"
	done < "$W/files"
	echo "exit"
    } > "$W/dump.gf"
    guestfish --rw -a "$dev" -f "$W/dump.gf" 2>&1 | awk -v nestedfile="$W/nested" '
	BEGIN { while ((getline l < nestedfile) > 0) nested["/" l] = 1; skip = 0 }
	/^PATH / { skip = ($2 in nested) ? 1 : 0 }
	skip && /^st_(mtime|ctime)_(sec|nsec):/ { next }
	{ print }
    ' > "$out" || true
}

echo "== dumping source: $IMG"
dump_metadata "$IMG" "$W/dump.ref"
fail=0
for bin in "$@"; do
    name="$(basename "$(dirname "$(dirname "$bin")")").$(echo "$bin" | md5sum | cut -c1-6)"
    img="$W/$name.pcl"; raw="$W/$name.raw"
    echo "== $bin"
    "$bin" -c -a0 -s "$IMG" -O "$img" -L /dev/null >/dev/null 2>&1
    dd if=/dev/zero of="$raw" bs=1M count=$SIZE_MB status=none
    "$bin" -r -C -s "$img" -O "$raw" -L /dev/null >/dev/null 2>&1
    dump_metadata "$raw" "$W/dump.$name"
    if cmp -s "$W/dump.ref" "$W/dump.$name"; then
	echo "   metadata dump ($W/dump.$name): IDENTICAL to source"
    else
	echo "   metadata dump: DIFFERS from source:"
	diff "$W/dump.ref" "$W/dump.$name" | head -30 || true
	fail=1
    fi
done
echo "workdir: $W"
[ "$fail" = 0 ] && echo "RESULT: all metadata preserved" || echo "RESULT: FAILURES above"
exit "$fail"
