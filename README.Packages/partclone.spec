%global isal_opt %{nil}
%ifarch x86_64 aarch64 ppc64le s390x
%global isal_opt --enable-isal
%endif

Name:         partclone
Version:      0.3.55
Release:      1%{?dist}.gh1
License:      GPL-2.0-only
URL:          http://partclone.org/
Summary:      Utility to clone and restore partitions

# Built by the release CI from a git archive tarball (see release.yml);
# no network fetch is performed for Source0.
Source0:      %{name}-%{version}.tar.gz

BuildRequires: gcc
BuildRequires: make
BuildRequires: autoconf
BuildRequires: automake
BuildRequires: libtool
BuildRequires: gettext-devel
BuildRequires: intltool
BuildRequires: pkgconf
BuildRequires: libuuid-devel
BuildRequires: openssl-devel
BuildRequires: zlib-devel
BuildRequires: libzstd-devel
BuildRequires: e2fsprogs-devel
BuildRequires: userspace-rcu-devel
BuildRequires: xfsprogs-devel
BuildRequires: ntfs-3g-devel
BuildRequires: ncurses-devel
BuildRequires: libblkid-devel
BuildRequires: lz4-devel
BuildRequires: xxhash-devel
BuildRequires: libxslt
BuildRequires: docbook-style-xsl
%ifarch x86_64 aarch64 ppc64le s390x
BuildRequires: isa-l-devel
%endif

Requires: bash-completion

%description
Partclone is a project like the well-known backup utility
"Partition Image" a.k.a. partimage.

Partclone provides utilities to back up used blocks and is
designed for highest compatibility with the file systems using
supported libraries like e2fslibs.

Supported file systems in this build:
ext2/ext3/ext4, xfs, hfs+, apfs, fat, exfat, ntfs, btrfs, minix,
f2fs and zfs (clean-room parser).

%prep
%autosetup -n %{name}-%{version}

%build
./autogen
%configure \
  --enable-ncursesw \
  --enable-xfs \
  --enable-extfs \
  --disable-reiserfs \
  --disable-reiser4 \
  --enable-hfsp \
  --enable-apfs \
  --enable-fat \
  --enable-exfat \
  --enable-ntfs \
  --disable-ufs \
  --disable-vmfs \
  --disable-jfs \
  --enable-btrfs \
  --enable-minix \
  --enable-f2fs \
  --enable-zfs \
  --disable-nilfs2 \
  %{isal_opt}
%make_build

%install
%make_install
%find_lang %{name}

%files -f %{name}.lang
%license COPYING
%doc AUTHORS ChangeLog NEWS README.md
%{_sbindir}/partclone*
%{_mandir}/man8/partclone*.8*
%{_datadir}/bash-completion/completions/partclone

%changelog
* Fri Nov 27 2026 Thomas Tsai <tlinux.tsai@gmail.com> - 0.3.55-1.gh1
- Automated upstream release build 0.3.55.

* Mon Nov 16 2026 Thomas Tsai <tlinux.tsai@gmail.com> - 0.3.54-1.gh1
- Rewrite spec for the 0.3.x series: align configure flags with
  README.Packages/debian.sid/rules, refresh BuildRequires for current
  Fedora, and publish via the upstream release workflow.
