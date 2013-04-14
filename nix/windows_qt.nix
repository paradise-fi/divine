{stdenv, fetchurl, windows_img, windows_mingw, writeScript, pkgs}:

stdenv.mkDerivation rec {
  name = "windows-qt";
  version = "4.8.4";
  pkg = "qt-${version}-x86";

  source = fetchurl {
    url = "http://releases.qt-project.org/qt4/source/qt-everywhere-opensource-src-4.8.4.tar.gz";
    sha256 = "0w1j16q6glniv4hppdgcvw52w72gb2jab35ylkw0qjn5lj5y7c1k";
  };

  build = pkgs.callPackage ./windows_build.nix {
    inherit windows_mingw;
    img = windows_img;
    src = source;
    name = "qt";
    mem = "2048M";
    buildScript = ''
      set -ex
      cd source
      echo y | ./configure.exe -opensource -fast -release
      for i in `seq 1 20`; do # make seems to die often due to a bug in MSYS
          make && break
      done
      make install INSTALL_ROOT=/install
      cp -R C:/install/WORK/source E:/qt
    '';
  };

  builder = writeScript "${name}-builder" ''
      source $stdenv/setup
      ensureDir $out
      ln -s ${build}/qt $out/qt
  '';

  unpack = "cp -R ${build}/qt qt";
  install = ''
      set PATH=%PATH%;D:\qt\bin
      set QTDIR=D:\qt
      setx QTDIR D:\qt
  '';
}
