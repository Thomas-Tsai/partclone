# Clean-room Reimplementation of ZFS Features with AI Assistance

> Workflow for a project that **links no OpenZFS library and reimplements
> everything from scratch**.

## Prerequisites (hard rules)

1. Only **public specification documents** (e.g.
   <https://mminkus.github.io/zfs-ondiskformat/>) and **direct on-disk
   observation** may be used as knowledge sources.
2. **Never open OpenZFS source code while writing code.**
3. All AI-produced code must go through a **manual pass** of
   de-similarization, renaming and license review.

The six phases below describe what AI can do at each step, and how to
work with AI safely and legally.

---

## Phase 1 – Requirements & specification document

*(written by a person who does NOT write the implementation)*

| Step | AI can help with | Example prompt |
|---|---|---|
| 1.1 Read the spec | Summarize chapters of the ZFS on-disk format into a concept list (e.g. `spa`, objset, dnode, spacemap structures) | "Summarize the 'Object Set' and 'Space Map' chapters of <https://mminkus.github.io/zfs-ondiskformat/> as a bullet list of field meanings and lengths." |
| 1.2 Build a feature list | Map each concept to a C API (e.g. `zpool_get_spacemaps()`, `zfs_read_block_history()`) | "Based on the concepts above, list every public C API we need (name, inputs, return value, error codes), one sentence each." |
| 1.3 Write the requirements doc | Use a template to produce a Markdown requirements document: description, I/O, edge cases, test vectors | "Turn the API list into a requirements document, one section per API: ## name, - purpose, - parameters, - return, - failure modes." |
| 1.4 License check | Have AI flag text fragments that look like OpenZFS source code | "Check whether the following text is from OpenZFS; if so mark 'possibly CDDL-protected': <...>" |

> This phase must be done by a team member (or external consultant) who
> will NOT write the code, so implementers never accidentally look at
> the original source. AI only summarises and formats; a human signs off
> the final document.

## Phase 2 – Code skeleton

| Step | AI can help with | Example prompt |
|---|---|---|
| 2.1 Generate headers from requirements | Function prototypes, struct declarations, error-code enums | "From this requirements doc, generate `myzfs_spacemap.h` with `struct myzfs_spacemap`, `int myzfs_spacemap_load(const char *, struct myzfs_spacemap *)` and an error-code enum." |
| 2.2 Generate `.c` frames | Function skeletons with TODO comments so logic is filled in later | "From `myzfs_spacemap.h`, generate `myzfs_spacemap.c` with each function reduced to comments describing the steps (e.g. 'read N bytes at offset 0x…')." |
| 2.3 Project scaffolding | `CMakeLists.txt`, `Makefile`, `README.md` | "Create a GCC/Clang CMake project named `myzfs` with `src/` and `include/`." |

> **Caution:** skeleton code must not reuse OpenZFS variable names,
> macros or data structures. If an AI sample contains names like
> `dnode_t` or `spa_t`, rename them yourself (e.g. `myzfs_obj_t`).

## Phase 3 – Filling in the implementation

*(AI generates a "reference implementation" + human review)*

| Step | AI can help with | Example prompt |
|---|---|---|
| 3.1 Read disk blocks | Sample `open`/`pread`/`O_DIRECT` code from the format description | "Write C code that opens device `dev` and `pread`s `len` bytes from `offset`, returning an int error code." |
| 3.2 Parse the Object Set | Byte-decoding code (`memcpy`, bit ops) with constants in `#define`s, no magic numbers from the original source | "From the ZFS object set chapter, write `int myzfs_objset_parse(const uint8_t *, size_t, struct myzfs_objset *)` with every field length/offset as a macro." |
| 3.3 Spacemap handling | B-tree/bitmap traversal examples using only libc | "Write a minimal spacemap parser in pure C, returning each region's start LBA and length." |
| 3.4 Unit-test generation | GoogleTest / CMocka / Unity cases from the requirements doc | "From `myzfs_spacemap.h`, write 5 unit tests covering normal, out-of-space and corrupt-structure cases." |
| 3.5 Static analysis & plagiarism check | Script that runs `cppcheck`, `clang-tidy` and a code-similarity tool against OpenZFS | "Write a Bash script that runs `cppcheck --enable=all src/` and then checks similarity of `src/` against OpenZFS; fail above 20%." |

### Review checklist

- **Variable / function names**: if AI reuses an OpenZFS name, rename immediately.
- **Magic constants**: lift every hard-coded value into a `#define` and
  annotate its source (spec page/chapter).
- **Control flow**: watch for copied loops / conditionals; rewrite
  anything too similar.

## Phase 4 – Testing, verification, debugging

| Step | AI can help with | Example prompt |
|---|---|---|
| 4.1 Build test disk images | `dd`/`losetup`/`zpool create` scripts — observation only, results never baked into code | "Write a Bash script that makes a 200 MiB `zfs.img`, attaches it via `losetup` and shows `zdb -e` output for reference." |
| 4.2 Output diffing | Diff scripts comparing your output with `zdb` observations | "Write a Python script that loads `zdb -e -p` JSON output, calls `myzfs_spacemap_load`, and compares the block lists." |
| 4.3 Benchmarks | `time`/`perf` benchmark scripts | "Write `benchmark.sh` running `myzfs_spacemap_load` 1000 times on a 10 GiB image, reporting average time and memory." |
| 4.4 CI | GitHub Actions / GitLab CI configs: build, test, license-check jobs | "Write `.github/workflows/ci.yml` with build, test and license-check jobs." |

## Phase 5 – Copyright statement & license

| Step | AI can help with | Example prompt |
|---|---|---|
| 5.1 Choose a license | Compare common GPL-compatible licenses (MIT, Apache-2.0, BSD-3-Clause) | "Compare MIT, Apache-2.0 and BSD-3-Clause: GPL compatibility and patent clauses." |
| 5.2 Generate LICENSE | Produce the LICENSE file | "Generate an Apache-2.0 LICENSE with 'Copyright © 2026 <name>' on top." |
| 5.3 File headers | Uniform copyright/license header comment for every `.c`/`.h` | "Turn this LICENSE text into a C-style file header with filename and short description." |
| 5.4 README | Project purpose, build steps, license info, and the clean-room legal statement | "Write README.md stating: 'This project reimplements the ZFS on-disk format without using OpenZFS code; license: Apache-2.0.'" |

## Phase 6 – Release & long-term maintenance

| Item | AI's role |
|---|---|
| Release notes | Generate a CHANGELOG from commits (Keep a Changelog format) |
| Security scanning | Run trivy/snyk and interpret reports with fixes |
| Doc translation | Translate README while preserving the license statement |
| User support | FAQ: "how to build on macOS", "supported block sizes", ... |
| Continuous legality | Re-run code-similarity + license-audit after every major feature, confirming no OpenZFS code crept in |

---

## Summary – where AI fits

| Phase | AI as generator | AI as checker |
|---|---|---|
| Requirements / spec | Summaries, lists, templates | License / similarity check |
| Skeleton | Headers, prototypes, Makefile | Style, naming conflicts |
| Implementation | Sample code, tests, benchmarks | Static analysis, similarity |
| Testing | Test cases, CI configs | Output diffing, automated reports |
| License | LICENSE / README / headers | License-compatibility report |
| Maintenance | Changelog, translation, FAQ | Periodic security/license scans |

As long as you strictly keep the **"never look at the original source"**
principle, AI is a huge help for productivity, documentation and test
automation — and **human review after every AI output** remains
mandatory to keep the final code free of accidental copying that would
trigger the CDDL/GPL conflict.

## Quick-start prompt example

    You are a programmer familiar with the ZFS on-disk format. From the
    public specification excerpts below, produce a READ-ONLY ZFS
    spacemap parser skeleton.

    Spec excerpts:
    - The spacemap is a series of 16-byte records.
    - First 8 bytes: start LBA (little-endian); next 8 bytes: length in
      512-byte sectors.
    - The spacemap lives in the MOS (meta-object set) in ZFS's "tree".

    Requirements:
    1. Pure C99: `myzfs_spacemap.h` + `myzfs_spacemap.c`, libc only.
    2. Signature:
         int myzfs_spacemap_load(const char *dev_path,
                                 uint64_t mos_obj,   /* MOS objset object id */
                                 struct myzfs_spacemap *out);
    3. All magic numbers as #define constants in the header, annotated
       with their spec source (page X).
    4. Leave /* TODO: implement */ in each function body.
    5. Also generate test_spacemap.c (CMocka) covering normal reads and
       truncated input.
