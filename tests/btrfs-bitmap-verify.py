#!/usr/bin/env python3
"""btrfs-bitmap-verify.py - ground-truth verification of a partclone image bitmap.

Extracts every EXTENT_ITEM/METADATA_ITEM (the filesystem's allocation truth)
from a btrfs device, maps them to physical ranges through the chunk tree, and
asserts that every mirror of every used extent is marked in the bitmap stored
inside the given partclone image (format 0002, BM_BIT, no per-block checksum).

Usage: btrfs-bitmap-verify.py IMAGE.pcl SOURCE_DEVICE [RESTORED_RAW]
Exit 0 when every used extent is covered; exit 1 with details otherwise.

With RESTORED_RAW (a device restored from the same image), additionally byte
-compares SOURCE_DEVICE vs RESTORED_RAW over every mapped physical range of
the used extents, proving the restore reproduces the real data exactly.

Intentionally supports the layouts used by the benches (single device,
single/DUP/RAID1{,C3,C4} profiles); anything RAID0/5/6/10 aborts as
unsupported instead of guessing.
"""
import re
import struct
import subprocess
import sys

def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True, check=True).stdout

def parse_super(raw):
    tot = used = node = sect = None
    for line in raw.splitlines():
        m = re.match(r"\s*(total_bytes|bytes_used|nodesize|sectorsize)\s+(\d+)", line)
        if m:
            v = int(m.group(2))
            if m.group(1) == "total_bytes": tot = v
            elif m.group(1) == "bytes_used": used = v
            elif m.group(1) == "nodesize": node = v
            elif m.group(1) == "sectorsize": sect = v
    return tot, used, node, sect

CHUNK_TYPES = {
    "SINGLE": 0, "DUP": 0x10,
    "RAID0": 1, "RAID1": 2, "RAID1C3": 0x40, "RAID1C4": 0x80,
    "RAID10": 4, "RAID5": 8, "RAID6": 0x20,
}

def parse_chunks(text):
    chunks = []
    cur = None
    for line in text.splitlines():
        m = re.search(r"key \(\S+ CHUNK_ITEM (\d+)\)", line)
        if m:
            cur = {"logical": int(m.group(1)), "stripes": [], "type": None, "len": None}
            chunks.append(cur)
            continue
        m = re.match(r"\s*length (\d+) owner \d+ stripe_len \d+ type (\S+)", line)
        if m and cur is not None:
            cur["len"] = int(m.group(1))
            cur["type"] = m.group(2)
            continue
        m = re.match(r"\s*num_stripes (\d+)", line)
        if m and cur is not None:
            cur["num_stripes"] = int(m.group(1))
            continue
        m = re.match(r"\s*stripe \d+ devid \d+ offset (\d+)", line)
        if m and cur is not None:
            cur["stripes"].append(int(m.group(1)))
    return chunks

def parse_extents(text):
    exts = []
    for line in text.splitlines():
        m = re.search(r"key \((\d+) EXTENT_ITEM (\d+)\)", line)
        if m:
            exts.append((int(m.group(1)), int(m.group(2)), "data"))
            continue
        m = re.search(r"key \((\d+) METADATA_ITEM (\d+)\)", line)
        if m:
            exts.append((int(m.group(1)), None, "meta"))  # len = nodesize
    return exts

def map_ranges(chunks, logical, length):
    for c in chunks:
        if c["logical"] <= logical < c["logical"] + c["len"]:
            t = c["type"]
            if "RAID0" in t or "RAID5" in t or "RAID6" in t or "RAID10" in t:
                sys.exit(f"unsupported chunk profile {t}")
            off = logical - c["logical"]
            if off + length > c["len"]:
                sys.exit(f"extent {logical}+{length} crosses chunk boundary (unsupported)")
            return [(s + off, length) for s in c["stripes"]]
    sys.exit(f"no chunk covers logical {logical}")

def main():
    img_path, dev = sys.argv[1], sys.argv[2]
    restored_path = sys.argv[3] if len(sys.argv) > 3 else None
    # image_desc_v2 is packed: head(36) + fs_info(52) + options(18) + crc(4)
    HEAD_V2 = 36
    DESC_V2 = 110
    with open(img_path, "rb") as f:
        head = f.read(DESC_V2)
        if head[0:15] != b"partclone-image":
            sys.exit(f"{img_path}: not a partclone image")
        if head[30:34] != b"0002":
            sys.exit("only format 0002 supported")
        size, total, used_sb, used_bm = struct.unpack("<QQQQ", head[52:84])
        bsize = struct.unpack("<I", head[84:88])[0]
        csum_mode, = struct.unpack("<H", head[96:98])
        bitmap_mode = head[105]
        if csum_mode != 0:
            sys.exit("verification only supports -a0 images (no block checksums)")
        if bitmap_mode != 1:  # BM_BIT
            sys.exit(f"unsupported bitmap_mode {bitmap_mode} (only BM_BIT)")
        if struct.unpack("<I", head[88:92])[0] != 18:
            sys.exit("unexpected image_options_v2 layout")
        if bsize == 0 or total == 0:
            sys.exit("invalid header fields")
        nbitmap = (total + 7) // 8
        f.seek(DESC_V2)
        bm = f.read(nbitmap)
        if len(bm) != nbitmap:
            sys.exit("image shorter than expected (bitmap truncated?)")

    def block_set(b):
        if b >= total:
            return False
        return bool(bm[b // 8] & (1 << (b % 8)))

    sup = run(["btrfs", "inspect-internal", "dump-super", dev])
    dev_total, dev_used, nodesize, sectorsize = parse_super(sup)
    if bsize != nodesize:
        sys.exit(f"image block_size {bsize} != fs nodesize {nodesize}")

    chunks = parse_chunks(run(["btrfs", "inspect-internal", "dump-tree", "-t", "chunk", dev]))
    exts = parse_extents(run(["btrfs", "inspect-internal", "dump-tree", "-t", "extent", dev]))
    if not chunks or not exts:
        sys.exit("failed to parse chunk or extent trees")

    missing_blocks = set()
    covered = 0
    mapped_ranges = []
    for logical, length, kind in exts:
        if kind == "meta":
            length = nodesize
        for phys, plen in map_ranges(chunks, logical, length):
            mapped_ranges.append((phys, plen))
            # same coverage rule as partclone's set_bitmap()
            b0 = phys // bsize
            b1 = (phys + plen + bsize - 1) // bsize
            for b in range(b0, b1):
                if block_set(b):
                    covered += 1
                else:
                    missing_blocks.add(b)

    print(f"device: totalblocks={total} bsize={bsize} used_sb={used_sb} image_used_bm={used_bm}")
    print(f"extents: {len(exts)} items, covered blocks={covered}, missing blocks={len(missing_blocks)}")
    if missing_blocks:
        mb = sorted(missing_blocks)
        print("first missing ranges:")
        s = p = mb[0]
        shown = 0
        for x in mb[1:]:
            if x != p + 1:
                print(f"  blocks {s}-{p} (0x{s*bsize:x}-0x{(p+1)*bsize:x})")
                shown += 1
                if shown >= 10:
                    break
                s = x
            p = x
        print(f"  blocks {s}-{p} (0x{s*bsize:x}-0x{(p+1)*bsize:x})")
        sys.exit(1)
    print("OK: every used extent (all mirrors) is marked in the image bitmap")

    if restored_path:
        bad = 0
        with open(dev, "rb") as fs, open(restored_path, "rb") as rr:
            for phys, plen in mapped_ranges:
                fs.seek(phys)
                rr.seek(phys)
                a, b = fs.read(plen), rr.read(plen)
                if a != b:
                    off = next((i for i in range(len(a)) if a[i] != b[i]), 0)
                    print(f"MISMATCH at 0x{phys+off:x} (extent phys 0x{phys:x} len {plen})")
                    bad += 1
                    if bad >= 10:
                        break
        if bad:
            sys.exit(1)
        print(f"OK: restored data matches source over {len(mapped_ranges)} physical ranges")

if __name__ == "__main__":
    main()
