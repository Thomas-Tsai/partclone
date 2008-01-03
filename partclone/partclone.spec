Name:         partclone
Version:      0.0.1
Release:      2
Group:        System/Filesystems
URL:          http://partclone.sf.net
License:      GPL
Summary:      File System Clone Utilities for ext2/3, reiserfs, reiser4, xfs, hfs+ File System
Source0:      %{name}-%{version}.tar.bz2
BuildRequires: e2fsprogs-devel, reiser4progs, xfsprogs-devel, 
BuildRoot:    %{_tmppath}/%{name}-build

%description
A set of file system clone utilities, including
ext2/3, reiserfs, reiser4, xfs, hfs+ file system

Authors:
--------
    Thomas Tsai <Thomas _at_ nchc org tw>
    Jazz Wang <jazz _at_ nchc org tw>

%prep
%setup -q -n %{name}

%build
./configure --prefix=/usr --enable-hfsp --enable-xfs --enable-reiser4
#make -j4 LDFLAGS+="-static"
make -j4

%install
[ -d %{buildroot} ] && rm -rf %{buildroot}
make -j4 install prefix=%{buildroot}/usr/

%post

%postun
ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc AUTHORS BUGS COPYING CREDITS ChangeLog NEWS README THANKS TODO
%doc %{_mandir}/man?/*
%{_sbindir}/*
/usr/share/locale/*

%changelog
* Mon Dec 31 2007 - Steven Shiau <steven _at_ nchc org tw> 0.0.1-2
- Some doc and debian rules were added by Thomas Tsai.

* Mon Dec 10 2007 - Steven Shiau <steven _at_ nchc org tw> 0.0.1-1
- Initial release for partclone.
