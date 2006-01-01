# We need an explicit dependency on mozilla as the so files don't have a version number 
%define mozilla_ver %(/usr/bin/mozilla-config --version)

Summary: Metasearch tool
Name: pinot
Version: 0.35
Release: 1
License: GPL
Group: Applications/Internet 
Source: %{name}-%{version}.tar.gz
Patch0: libxmlpp026.patch
URL: http://pinot.berlios.de/
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
Requires: xapian-core-libs >= 0.9.0, neon >= 0.24, gtkmm24 >= 2.6, mozilla >= %{mozilla_ver}, sqlite >= 3.1.2, ots >= 0.4.2, libtextcat >= 2.2, fam >= 2.6.10, gmime >= 2.1, file
BuildRequires: xapian-core-devel >= 0.9.0, neon-devel >= 0.24, gtkmm24-devel >= 2.6, mozilla-devel >= %{mozilla_ver}, sqlite-devel >= 3.1.2, ots-devel >= 0.4.2, libtextcat-devel >= 2.2, fam-devel >= 2.6.10, gmime-devel >= 2.1, file, boost-devel >= 1.32, desktop-file-utils
%if 0%{?_with_libxmlpp026:1}
Requires: libxml++ >= 0.26
BuildRequires: libxml++-devel >= 0.26
%else
Requires: libxml++ >= 2.6 
BuildRequires: libxml++-devel >= 2.6
%endif

%description
Pinot is a metasearch tool for the Free Desktop.  It enables one to query
sources, display as well as analyze and locally index the returned results.
It may also be used as a lightweight personal space search tool.

%package pdf
Summary: PDF tokenizer for Pinot that uses pdftohtml
Group: Applications/Internet
Requires: %{name} = %{version}
Requires: pdftohtml

%description pdf
The included tokenizer enables Pinot to index PDF documents.

%package word 
Summary: MS Word tokenizer for Pinot that uses antiword
Group: Applications/Internet
Requires: %{name} = %{version}
Requires: antiword

%description word
The included tokenizer enables Pinot to index MS Word documents.

%package omega 
Summary: Xapian Omega plugin for Pinot
Group: Applications/Internet
Requires: %{name} = %{version}
Requires: xapian-omega

%description omega
The included plugin enables Pinot to use Xapian Omega as a search engine.

%prep
%setup -q
%if 0%{?_with_libxmlpp026:1}
%patch0 -p1 -b .xml026
%endif

%build
make DEBUG=yes
make DEBUG=yes pinot_mo

%install
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf $RPM_BUILD_ROOT
make install PREFIX=$RPM_BUILD_ROOT
# This engine is not usable as it is
mv $RPM_BUILD_ROOT/%{_datadir}/pinot/engines/AmazonAPI.src $RPM_BUILD_ROOT/%{_datadir}/pinot/
# Desktop file
cat >%{name}.desktop << EOF
[Desktop Entry]
Name=Pinot Metasearch tool
Comment=Search the Web and your documents
Exec=%{_bindir}/pinot
StartupNotify=true
Icon=pinot.png
Terminal=false
Type=Application
Categories=Application;Network;
Encoding=UTF-8
EOF
mkdir -p $RPM_BUILD_ROOT/%{_datadir}/applications
desktop-file-install --vendor Amra \
	--dir $RPM_BUILD_ROOT/%{_datadir}/applications  \
	%{name}.desktop

%post
gtk-update-icon-cache -q -f %{_datadir}/icons/hicolor || :

%clean
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root, -)
%doc ChangeLog COPYING README TODO
%{_bindir}/pinot
%{_datadir}/pinot/index.html
%{_datadir}/pinot/xapian-powered.png
%{_datadir}/pinot/metase-gtk2.glade
%{_datadir}/pinot/metase-gtk2.gladep
%{_datadir}/pinot/textcat_conf.txt
%{_datadir}/pinot/*.src
%dir %{_datadir}/pinot/engines/
%config(noreplace) %{_datadir}/pinot/engines/A9.src
%config(noreplace) %{_datadir}/pinot/engines/Acoona.src
%config(noreplace) %{_datadir}/pinot/engines/Altavista.src
%config(noreplace) %{_datadir}/pinot/engines/AskJeeves.src
%config(noreplace) %{_datadir}/pinot/engines/BitTorrent.src
%config(noreplace) %{_datadir}/pinot/engines/Clusty.src
%config(noreplace) %{_datadir}/pinot/engines/Freshmeat.src
%config(noreplace) %{_datadir}/pinot/engines/Koders.src
%config(noreplace) %{_datadir}/pinot/engines/Google.src
%config(noreplace) %{_datadir}/pinot/engines/Lycos.src
%config(noreplace) %{_datadir}/pinot/engines/MSN.src
%config(noreplace) %{_datadir}/pinot/engines/Teoma.src
%config(noreplace) %{_datadir}/pinot/engines/Topix.src
%config(noreplace) %{_datadir}/pinot/engines/WiseNut.src
%config(noreplace) %{_datadir}/pinot/engines/Yahoo.src
%config(noreplace) %{_datadir}/pinot/engines/YahooAPI.src
%config(noreplace) %{_datadir}/pinot/engines/Wikipedia.src
%{_datadir}/locale/fr/LC_MESSAGES/pinot.mo
%{_datadir}/icons/hicolor/48x48/apps/pinot.png
%{_datadir}/applications/Amra-%{name}.desktop

%files pdf
%defattr(-, root, root, -)
%dir %{_datadir}/pinot/tokenizers/pdf_tokenizer.so

%files word
%defattr(-, root, root, -)
%dir %{_datadir}/pinot/tokenizers/word_tokenizer.so

%files omega
%defattr(-, root, root, -)
%config(noreplace) %{_datadir}/pinot/engines/Omega.src

%changelog
