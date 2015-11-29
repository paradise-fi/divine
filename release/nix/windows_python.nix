{stdenv, fetchurl, writeScript}:

stdenv.mkDerivation rec {
  name = "windows-python";
  pkg = "python-2.7.4-win32-x86";

  msi = fetchurl {
    url = "http://www.python.org/ftp/python/2.7.4/python-2.7.4.msi";
    sha256 = "11mdlllv25qrl2zy4zvlahk18ir1px3jbj16da99rv1v3kaz5jwb";
  };

  builder = writeScript "${name}-builder" "source $stdenv/setup && ensureDir $out";
  unpack = "cp ${msi} python.msi";
  install = ''
      msiexec /qb /i D:\python.msi
      set PATH=%PATH%;C:\Python27\
  '';
}
