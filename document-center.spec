#
# spec file for package document-center
#
# Copyright (c) 2014 SUSE LINUX Products GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#

# norootforbuild


Name:           document-center
Summary:        Document Center for GNOME
Version:        0.1
Release:        1
License:        GPLv2+
Group:          System/GUI/GNOME
Url:            https://github.com/initlove/document-center
Source:         %{name}-%{version}.tar.bz2
BuildRequires:  glib2-devel >= 2.30.0
BuildRequires:  gobject-introspection-devel
BuildRequires:  dbus-1-glib-devel
BuildRequires:  libxml2-devel
BuildRequires:  libsoup-devel
BuildRequires:  librest-devel
BuildRequires:  gtk3-devel
BuildRequires:  intltool
Requires:       %{name}-lang = %{version}
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
Document center is a place to find/read/tag/note/comment/share documents
both from local and remote.

Authors:
------
	David Liang <dliang@suse.com>

%lang_package
%prep
%setup -q

%build
libtoolize -if
autoreconf -if
./configure

make %{?jobs:-j%jobs}

%install
%makeinstall

%find_lang %{name}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root)
%dir %{_datadir}/pixmaps/document-center/
%dir %{_datadir}/document-center/
%dir %{_datadir}/document-center/ui/
%{_datadir}/pixmaps/document-center/*
%{_datadir}/document-center/ui/*
%{_bindir}/document-center

%files lang -f %{name}.lang

%changelog
