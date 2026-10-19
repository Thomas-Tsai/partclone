# src/zfs/ – Clean-room ZFS Implementation Statement

This directory is the **clean-room implementation behind partclone.zfs
(`--enable-zfs`, the mainline driver)**: it uses and links no OpenZFS
(CDDL-1.0) code or headers, and parses ZFS pools directly from the
published on-disk format to produce partclone's used-block bitmap.

For format parsing details and on-disk observation notes see
`zfsclone-gpl.md` at the project root; this document focuses on the
**clean-room principles, the development process, and why the result
may be licensed under the GPL**.

## 1. Scope (v1)

- stripe pool, single top-level "disk" vdev (the user supplies the ZFS
  partition, e.g. `/dev/sda1`)
- little-endian pools (x86/arm64 LE hosts)
- read-only: computes used blocks only, never writes to the pool

## 2. Clean-room Principles

1. **Knowledge comes from exactly two sources**
   - the MIT-licensed independent specification
     "[ZFS On-Disk Format](https://mminkus.github.io/zfs-ondiskformat/)"
     (ch.1 vdevs/labels/uberblocks, ch.2 block pointers, ch.3 DMU,
     ch.5 ZAP, ch.9 space maps, plus glossary enum values)
   - **direct on-disk observation** (hexdump / `zdb` comparison against
     self-built test pools; observations are recorded only in tests and
     comments, e.g. the embedded blkptr +1 size bias and the out-of-band
     placement of the uberblock rootbp)
2. **No OpenZFS source or headers are read while writing code.**
   Every symbol (`zspa_*`, `zdn_*`, `zbp_*`, `zsm_*`, `zzap_*`,
   `znv_*`, ...) is independently named, with no correspondence to
   OpenZFS's `spa_t`/`dnode_t`/`blkptr_t`/`SM_*_DECODE` etc.
3. **AI output is always human-reviewed**: every generated piece of
   code goes through manual renaming, de-similarization and license
   review (process: `src/zfs/clean-room-req.md`).

## 3. Development Process

Mapped against the six phases of `src/zfs/clean-room-req.md`:

| Phase | What this project did |
|---|---|
| 1. Requirements / specification | Concept list from the MIT spec (uberblock, dnode, ZAP, space map); functional requirements map directly onto partclone's `read_super_blocks` / `read_bitmap` interface |
| 2. Code skeleton | `zfs_disk.h` concentrates all on-disk constants and accessors (each constant annotated with its spec chapter); remaining modules (`zfs_spa` / `zfs_dmu` / `zfs_zap` / `zfs_spacemap` / `zfs_nvlist` / `zfs_compress` / `zfs_traverse`) start from header contracts |
| 3. Implementation fill | Module by module, per spec chapter; all magic numbers lifted into `#define`s with source annotations; variable/function names manually chosen, no OpenZFS correspondence |
| 4. Testing / verification | Self-built loop-device ZFS pools; bitmap results compared against `zdb -b` (spacemap allocated); on-disk observation fills in behaviour the spec does not spell out |
| 5. Licensing | Every file carries a uniform GPL-2.0-or-later header plus *Format derived from the MIT-licensed specification … No OpenZFS (CDDL) source is used or linked.* |
| 6. Continuous checking | `tests/zfs-cleanroom-check.py` (forbidden-marker scan + OpenZFS similarity comparison + cppcheck); audit records live in `tests/zfs-cleanroom-audit-*.txt` |

## 4. Why the Result Can Be GPL

1. **No CDDL input means no CDDL/GPL license conflict.**
   This directory neither `#include`s nor links nor adapts any OpenZFS
   file; the only external libraries are liblz4 (BSD-2-Clause) and zlib
   (zlib license), both GPL-compatible.
2. **Format facts are not copyrightable.**
   Magic numbers, field offsets, bit layouts and on-disk key strings
   (`"vdev_tree"`, `"com.delphix:log_spacemap_zap"`, ...) are
   *functional facts required for interoperability* — every
   implementation must agree on them; matching them does not copy
   OpenZFS's expression. This implementation takes them from the
   MIT-licensed specification and on-disk observation, a clean origin.
3. **The expression is original.**
   Module partitioning, naming and data structures (e.g. the state
   struct `zfs_spa_t`, the linear scan in `zfs_zap.c` that replaces the
   salted CRC64 hash, the callback-replay space map design) are
   independent designs, corroborated quantitatively by similarity
   tooling (latest audit: worst containment **10.02%** < 20% threshold;
   the highest entry is the structurally unavoidable similarity of the
   block-pointer tree walk — see
   `tests/zfs-cleanroom-audit-2026-10-15.txt`).

The whole directory is therefore an original work of the partclone
project, released under **GPL-2.0-or-later**.

> Note: the CDDL-based reference implementation survives as
> `src/openzfsclone.c` (`--enable-openzfs`, links `libzpool`), used
> only as a cross-check and as a fallback for scenarios the clean-room
> driver does not support. It shares no code or headers with this
> directory.

## 5. Reproducing the Audit

```sh
git clone --depth 1 https://github.com/openzfs/zfs /tmp/openzfs
python3 tests/zfs-cleanroom-check.py --openzfs-src /tmp/openzfs
```

Three checks: CDDL copyright-marker scan, 8-token shingle similarity
comparison (20% threshold), and cppcheck static analysis
(informational). Re-run and refresh `tests/zfs-cleanroom-audit-*.txt`
after every major functional update.
