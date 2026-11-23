# Releasing partclone

Releasing is **maintainer-driven**: you prepare and push a tag locally, CI
tests it, then you trigger packaging and drafting manually from the GitHub
Actions page. Nothing on GitHub publishes anything without you.

```
local: bump + tag           CI: tag tests            CI: manual dispatch
──────────────────┐  ┌────────────────────┐  ┌──────────────────────────┐
 configure.ac     │  │ tag-check          │  │ Release Package (input:  │
 toolbox x3/x4    ─┼─►│ cross-arch-test    ─┼─►│   version) -> artifacts  │
 git tag X.Y.Z    │  │ debian-build       │  └───────────┬──────────────┘
 git push --tags  │  │ manual-on-host     │              ▼
                  │  └────────────────────┘  ┌──────────────────────────┐
                  │                          │ Release Draft (input:    │
                  │                          │   version) -> DRAFT      │
                  │                          └───────────┬──────────────┘
                  │                                      ▼
                  └──────────  you click "Publish release" on the web ───┘
```

## Phase 1 — local preparation (your machine)

```bash
# bump the version in configure.ac, e.g.
sed -i 's/^AC_INIT(\[Partclone\],\[[^]]*\]/AC_INIT([Partclone],[0.3.55]/' configure.ac

./toolbox --update-log            # regenerate ChangeLog from git
./toolbox --update-contributors   # regenerate CONTRIBUTORS from git
./toolbox --update-changelog      # debian changelog + rpm spec entries
./toolbox --update-po             # optional, heavy: sync translations

git add -u && git diff --cached --stat      # review everything
git commit -m "Bump version to 0.3.55"
git tag -a 0.3.55 -m "Release 0.3.55"
git push origin master && git push origin 0.3.55
```

`./toolbox --update-changelog` reads `AC_INIT` and, idempotently:

* prepends a `X.Y.Z-1~gh1` entry to `README.Packages/debian.sid/changelog`
  (bullets = `git log` since the previous tag), never touching the leftover
  `UNRELEASED` entries used by the Debian maintainer below it
* syncs `Version:` and prepends a `%changelog` entry in
  `README.Packages/partclone.spec`

Tagging first and running `--update-changelog` afterwards also works (it
detects the previous tag correctly) — just commit and push before dispatching
package builds, since the tag is what CI builds.

## Phase 2 — automatic tests on the tag

Pushing a tag fans out:

| Workflow | Purpose |
|---|---|
| `tag-check` | tag format `X.Y.Z` and `configure.ac` AC_INIT == tag name |
| `cross-arch-test` | QEMU save/restore on amd64/386/arm64/riscv64 |
| `debian-build` | sid snapshot `dpkg-buildpackage` + lintian |
| `manual-on-host` | self-hosted real-device FS tests (incl. ufs/vmfs/jfs) |

Watch them on the Actions page. Everything green? Continue.

## Phase 3 — package, then draft

1. **Actions → Release Package → Run workflow**, enter the version
   (e.g. `0.3.55`, tag must already exist). By default it softly verifies
   that all check runs on the tag commit succeeded; tick
   `skip_test_check` to bypass. Produces (artifacts, 30-day retention):

   | Artifact | Content |
   |---|---|
   | `pkg-source` | `partclone-<ver>.tar.gz` source tarball |
   | `pkg-deb-dynamic` | `partclone_<ver>-1~gh1_amd64.deb` (+ dsc/changes, sid) |
   | `pkg-deb-static` | `partclone-static_<ver>-1~gh1_amd64.deb` (+ sources) |
   | `pkg-rpm` | Fedora `.rpm` + `.src.rpm` |
   | `pkg-static-tarball` | `partclone-<ver>-static-linux-amd64.tar.gz` |
   | Docker Hub | `tlinux/partclone:<ver>` + `:latest` (build-only without secrets) |
   | `pkg-manifest` | version/sha marker consumed by Release Draft |

2. Download/inspect the artifacts you care about. Wrong? Fix and re-run.

3. **Actions → Release Draft → Run workflow**, enter the same version.
   It finds the latest successful *Release Package* run whose manifest matches
   the version, gathers all `pkg-*` artifacts, adds `SHA256SUMS` and release
   notes, and creates a **draft** release. An existing *draft* for that tag is
   deleted and recreated (the tag is kept); an existing *published* release is
   never touched — the workflow fails instead.

4. Open the draft on the web, review, press **Publish release** yourself.

### Versioning rules

* deb: `X.Y.Z-1~gh1`. The `~` sorts **before** the official Debian `-1`, so
  the archive package cleanly upgrades over these pre-built ones. `partclone`
  and `partclone-static` conflict with each other by design — install only one.
* rpm: `1%{?dist}.gh1`.
* tags stay `X.Y.Z` (no `v` prefix).

### Failure / retry cheat sheet

* **Tag points at wrong commit / wrong version in configure.ac** →
  `git tag -d X.Y.Z && git push origin :refs/tags/X.Y.Z`, fix, re-tag.
* **Tests red on the tag** → fix on master, re-tag (same as above), re-test.
  Nothing was built or published yet.
* **Package run failed or bad artifacts** → fix, re-run *Release Package*
  with the same version, then re-run *Release Draft* (it replaces the draft).
* **`manual-on-host` pending because the self-hosted runner is offline** →
  the soft check fails too; start the runner, or dispatch *Release Package*
  with `skip_test_check=true` when you are sure.

## Permissions / secrets

* `release-package.yml` and `tag-check.yml`: `contents: read` (+ `checks: read`
  for the soft test gate). No push permissions needed anywhere.
* `release-draft.yml`: `contents: write` (draft release) — the only write
  permission in the repo's workflows.
* Docker Hub push needs `DOCKERHUB_USERNAME` / `DOCKERHUB_TOKEN`; without
  them the image is built but not pushed.

## Quiet end-to-end rehearsal in a private staging repo

Nothing in the flow is truly public except *Publish release* and *Docker Hub
tags* — but if you want a zero-noise full rehearsal of a brand-new release
procedure, mirror to a private staging repo:

```bash
gh repo create Thomas-Tsai/partclone-staging --private   # one-time
git remote add staging git@github.com:Thomas-Tsai/partclone-staging.git
git push staging master:master

git checkout -b staging-rehearsal                        # keep local master clean
sed -i 's/^AC_INIT(\[Partclone\],\[[^]]*\]/AC_INIT([Partclone],[0.3.55]/' configure.ac
./toolbox --update-log && ./toolbox --update-contributors && ./toolbox --update-changelog
git commit -am "Bump version to 0.3.55" && git tag -a 0.3.55 -m "Release 0.3.55"
git push staging staging-rehearsal:master 0.3.55
```

Then in the **staging** repo (Actions tab):

1. tag-check / cross-arch / debian-build run automatically on the tag.
   `manual-on-host` stays pending forever (the self-hosted runner is only
   registered on the main repo) — expected.
2. Run **Release Package** with `skip_test_check=true` and
   `docker_push=false`, verify the artifacts and the summary page.
3. Run **Release Draft**, verify the draft release (auto-locates the
   package run via its manifest). Re-run it to confirm the
   delete-and-recreate-draft behavior.

Clean up afterwards: delete the draft release, delete the staging tag
(`git push staging :refs/tags/0.3.55`), and drop the local rehearsal branch.
The bump commit lives only on the staging mirror — the real release in the
main repo gets its own commit and tag later.

## ufs / vmfs / jfs and the DRBL-core repository

On `amd64`/`arm64` the packaging additionally enables **ufs, vmfs, jfs and
nilfs2** (see `README.Packages/debian.sid/rules`, `EXTRA_FS_ARCHES`).
`nilfs2` builds against plain Debian (`nilfs-tools` ships the headers and
library), but `libufs2`, `libvmfs` and `libjfs-dev` are **not in the Debian
or Ubuntu archives**; they are only published by the DRBL-core APT repo
(`http://free.nchc.org.tw/drbl-core`, suite `drbl`). Consequences:

* Both deb build jobs (`release-package.yml` and `debian-build.yml`) add the
  DRBL-core source before `mk-build-deps`, pinned so that **only** the ufs /
  vmfs / jfs packages may come from there (everything else stays on Debian).
* The **dynamically linked** `partclone` deb on amd64 gets runtime
  `Depends:` on those DRBL packages (`libufs2 8.2-3.drbl2`,
  `libvmfs 0.2.5-1.drbl3`, …). Users without the DRBL repo should install
  `partclone-static` or the static tarball instead.
* Long-term plan: replace these with **clean-room built-in parsers** (same
  approach as the `reiserx-cleanroom` branch for reiserfs/reiser4), so the
  DRBL dependency disappears.

## reiserfs / reiser4 status

Same situation as ufs/vmfs/jfs: on master they require external libraries
(`progsreiserfs` / `libreiser4`+`libaal`) that are **not in Debian sid**, so
the packaging keeps them `--disable`d. The unmerged `reiserx-cleanroom`
branch solves it with built-in parsers
(`--with-reiserfs-backend=auto|lib|builtin`); once that lands, the packaging
can enable reiser* the same way.
