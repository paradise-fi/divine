{stdenv, fetchurl, unzip, writeScript}:
 
stdenv.mkDerivation rec {
  name = "windows-cmake";
  pkg = "cmake-2.8.8-win32-x86";

  cmake = fetchurl {
    url = "http://www.cmake.org/files/v2.8/${pkg}.zip";
    sha256 = "1iaqczikn7ka3b9qxym3afa45smyjyax8bia67agdj2y3hdsbbvx";
  };

  builder = writeScript "${name}-builder" ''
    source $stdenv/setup
    mkdir -p $out
    ${unzip}/bin/unzip -q ${cmake}
    mv ${pkg} cmake
    tar czf $out/package.tar.gz cmake
  '';

  unpack = "tar xzf $package/package.tar.gz";
  install = "set PATH=%PATH%;D:\\cmake\\bin";

}
