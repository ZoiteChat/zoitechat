Name:           zoitechat
Version:        2.18.3
Release:        %autorelease
Summary:        HexChat-based IRC client
License:        GPL-2.0-or-later WITH cryptsetup-OpenSSL-exception
URL:            https://github.com/ZoiteChat/zoitechat
Source0:        %{url}/archive/refs/tags/%{name}-%{version}.tar.gz

BuildRequires:  desktop-file-utils
BuildRequires:  gcc
BuildRequires:  gettext
BuildRequires:  libappstream-glib
BuildRequires:  meson >= 0.55.0
BuildRequires:  perl
BuildRequires:  perl-devel
BuildRequires:  python3
BuildRequires:  python3-cffi
BuildRequires:  publicsuffix-list
BuildRequires:  xwayland-run
BuildRequires:  weston
BuildRequires:  pkgconfig(ayatana-appindicator3-0.1)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(enchant-2) >= 2.8.19
BuildRequires:  pkgconfig(gio-2.0) >= 2.36.0
BuildRequires:  pkgconfig(gmodule-2.0)
BuildRequires:  pkgconfig(gtk+-3.0) >= 3.22
BuildRequires:  pkgconfig(iso-codes)
BuildRequires:  pkgconfig(libarchive)
BuildRequires:  pkgconfig(libcanberra) >= 0.22
BuildRequires:  pkgconfig(libpci)
BuildRequires:  pkgconfig(lua)
BuildRequires:  pkgconfig(openssl) >= 0.9.8
BuildRequires:  pkgconfig(python3)

Requires:       hicolor-icon-theme
Requires:       iso-codes

%package devel
Summary:        Development files for ZoiteChat plugins
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description devel
Development files for building ZoiteChat plugins.

%description
ZoiteChat is a HexChat-based IRC client for Windows and UNIX-like operating
systems.

%prep
%autosetup -C

%build
%meson \
  -Dtext-frontend=false \
  -Dwith-checksum=true \
  -Dwith-fishlim=true \
  -Dwith-lua=lua \
  -Dwith-perl=perl \
  -Dwith-python=python3 \
  -Dwith-sysinfo=true \
  -Dinstall-appdata=true \
  -Dinstall-plugin-metainfo=true
%meson_build

%install
%meson_install
%find_lang %{name}

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/net.zoite.Zoitechat.desktop

appstream-util validate-relax --nonet %{buildroot}%{_datadir}/metainfo/net.zoite.Zoitechat.appdata.xml
appstream-util validate-relax --nonet %{buildroot}%{_datadir}/metainfo/net.zoite.Zoitechat*.metainfo.xml

xwfb-run -- /usr/bin/meson test -C %{_vpath_builddir} --num-processes %{_smp_build_ncpus} --print-errorlogs \
  "Theme Manager Dispatch Routing Tests" \
  "Validate net.zoite.Zoitechat.desktop" \
  "Validate translations" \
  "Fishlim Tests"

%files -f %{name}.lang
%license COPYING
%doc readme.md troubleshooting.md
%{_bindir}/zoitechat
%{_datadir}/applications/net.zoite.Zoitechat.desktop
%{_datadir}/dbus-1/services/org.zoitechat.service.service
%{_datadir}/icons/hicolor/48x48/apps/net.zoite.Zoitechat.png
%{_datadir}/icons/hicolor/scalable/apps/net.zoite.Zoitechat.svg
%{_datadir}/metainfo/net.zoite.Zoitechat.appdata.xml
%{_datadir}/metainfo/net.zoite.Zoitechat*.metainfo.xml
%dir %{_libdir}/zoitechat
%dir %{_libdir}/zoitechat/plugins
%dir %{_libdir}/zoitechat/python
%{_libdir}/zoitechat/plugins/*.so
%{_libdir}/zoitechat/python/*.py
%{_mandir}/man1/zoitechat.1*

%files devel
%{_includedir}/zoitechat-plugin.h
%{_libdir}/pkgconfig/zoitechat-plugin.pc

%changelog
%autochangelog
