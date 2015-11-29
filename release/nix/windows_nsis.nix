{stdenv, fetchurl, writeScript}:

stdenv.mkDerivation rec {
  name = "windows-nsis";
  pkg = "nsis-2.46-win32-x86";

  nsis = fetchurl {
    url = "mirror://sourceforge/nsis/nsis-2.46-setup.exe";
    sha256 = "0ckv8rldz6b8pk41ihg8sg02ykkdm3xaypwh4q35pr1fkxfaxhk9";
  };

  builder = writeScript "${name}-builder" "source $stdenv/setup && ensureDir $out";
  unpack = "cp ${nsis} nsis-setup.exe";
  install = "D:\\nsis-setup.exe /S";
}
