# Releasing partclone

Releases are driven by the **Release** GitHub Actions workflow
(`.github/workflows/release.yml`). It is started manually from
*Actions → Release → Run workflow*.

## Inputs

| Input | Default | Meaning |
|---|---|---|
| `version` | (required) | Version to release, e.g. `0.3.55` (no `v` prefix) |
| `update_po` | `false` | Also regenerate `po/` translations (runs autogen+configure) |
| `dry_run` | `false` | Build all artifacts from the dispatch commit, but skip bumping, tagging, publishing and the GitHub Release |
| `cleanup_on_failure` | `true` | Delete the freshly pushed tag if any test/publish job fails, so the same version can simply be re-run |

## Two modes

* **Full mode** — the tag does not exist yet. The workflow will:
  1. Bump `AC_INIT` in `configure.ac`
  2. Regenerate `ChangeLog` and `CONTRIBUTORS` (`./toolbox --update-log` / `--update-contributors`, plus optional `--update-po`)
  3. Sync packaging **before tagging**:
     * `README.Packages/debian.sid/changelog` gets a `${version}-1~gh1` entry (bullets from the previous tag's git log)
     * `README.Packages/partclone.spec` gets the new `Version:` and a `%changelog` entry
  4. Commit ("Bump version to …"), push the branch, create an annotated tag and push it
* **Existing-tag mode** — the tag already exists (e.g. created by hand). The workflow only verifies that the tag's `configure.ac` matches the requested version, then builds and publishes a GitHub Release for it. Useful for back-filling releases.

In both modes the actual tests and builds run against the tag.

## Pipeline order

```
prepare ──► test (debian-build, cross-arch) ──► publish ──► GitHub Release
```

* Tests must pass before anything is published. If a test fails, nothing is
  released and (full mode) the pushed tag is removed again when
  `cleanup_on_failure` is enabled — fix the code and re-run with the same
  version.
* `manual-on-host.yml` (self-hosted runner) is intentionally **not** part of
  the release pipeline; it keeps running on every tag push on its own, as an
  observer.
* The Docker Hub image is built and pushed in the publish stage, after the
  tests have passed. Without `DOCKERHUB_USERNAME`/`DOCKERHUB_TOKEN` secrets
  the image is built but not pushed.

## Published artifacts

| Artifact | Content |
|---|---|
| `partclone-<ver>.tar.gz` | Source tarball (`git archive`, version substituted) |
| `partclone_<ver>-1~gh1_amd64.deb` | Debian sid, dynamically linked |
| `partclone-static_<ver>-1~gh1_amd64.deb` | Debian sid, statically linked + `.dsc/.changes` |
| `partclone-<ver>-1.<dist>.gh1.*.rpm` / `.src.rpm` | Fedora container build |
| `partclone-<ver>-static-linux-amd64.tar.gz` | Statically linked CLI binaries |
| `partclone-<ver>-x86_64.AppImage` | AppImage bundling the static binaries (`./partclone-<ver>-x86_64.AppImage partclone.extfs …`) |
| `SHA256SUMS` | Checksums for everything above |
| Docker Hub `tlinux/partclone:<ver>` and `:latest` | Container image |

### Debian versioning rule

CI builds use debian revision `~gh1` (`0.3.55-1~gh1`). The tilde sorts
**before** the official Debian revision `-1`, so when the official package
reaches the Debian archive it cleanly upgrades over these pre-built packages.
Only one of `partclone` / `partclone-static` can be installed at a time
(they `Conflicts`/`Replaces` each other by design).

### ufs / vmfs / jfs and the DRBL-core repository

On `amd64`/`arm64` the packaging additionally enables **ufs, vmfs, jfs and
nilfs2** (see `README.Packages/debian.sid/rules`, `EXTRA_FS_ARCHES`).
`nilfs2` builds against plain Debian (`nilfs-tools` ships the headers and
library), but `libufs2`, `libvmfs` and `libjfs-dev` are **not in the Debian
or Ubuntu archives**; they are only published by the DRBL-core APT repo
(`http://free.nchc.org.tw/drbl-core`, suite `drbl`). Consequences:

* Both deb build jobs (`release.yml` and `debian-build.yml`) add the
  DRBL-core source before `mk-build-deps`, pinned so that **only** the ufs /
  vmfs / jfs packages may come from there (everything else stays on Debian).
* The **dynamically linked** `partclone` deb on amd64 gets runtime
  `Depends:` on those DRBL packages (`libufs2 8.2-3.drbl2`,
  `libvmfs 0.2.5-1.drbl3`, …). Users without the DRBL repo should install
  `partclone-static` (or the AppImage / static tarball) instead.
* Long-term plan: replace these with **clean-room built-in parsers** (same
  approach as the `reiserx-cleanroom` branch for reiserfs/reiser4), so the
  DRBL dependency disappears.

### reiserfs / reiser4 status

Same situation as ufs/vmfs/jfs: on master they require external libraries
(`progsreiserfs` / `libreiser4`+`libaal`) that are **not in Debian sid**, so
the packaging keeps them `--disable`d. The unmerged `reiserx-cleanroom`
branch solves it with built-in parsers
(`--with-reiserfs-backend=auto|lib|builtin`); once that lands, the packaging
can enable reiser* the same way.

## Retry cheat sheet

* Test failed → fix on master → re-run the workflow with the same `version`
  (full mode repeats from scratch; the stale tag was deleted).
* Publish job failed but tag was kept (`cleanup_on_failure` off) → re-run
  with the same `version`; it takes the existing-tag path and re-uploads
  assets onto the release (existing assets with the same name are replaced).

## Requirements on the repository

* *Settings → Actions → General → Workflow permissions* must be
  **Read and write permissions** (the workflow pushes the bump commit and tag).
* Docker publishing needs the `DOCKERHUB_USERNAME` / `DOCKERHUB_TOKEN`
  secrets; without them the pipeline still succeeds (build-only image).
