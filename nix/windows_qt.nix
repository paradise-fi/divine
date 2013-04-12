{stdenv, fetchurl, writeScript}:

stdenv.mkDerivation rec {
  name = "windows-qt";
  version = "4.8.4";
  pkg = "qt-${version}-x86";

  qt = fetchurl {
    url = "http://releases.qt-project.org/qt4/source/qt-win-opensource-${version}-mingw.exe";
    sha256 = "12gnrxan9vdg951126y3balrbzk89awkscn5pa2k6z9il5pdvab0";
  };

  builder = writeScript "${name}-builder" "source $stdenv/setup && ensureDir $out";
  unpack = "cp ${qt} qt-setup.exe";
  install = ''
      D:\qt-setup.exe /S
      set PATH=%PATH%;C:\Qt\${version}\bin
  '';
}
