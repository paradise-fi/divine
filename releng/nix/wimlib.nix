{ stdenv, fetchurl, libxml2, fuse, pkgconfig, utillinux, cdrkit, makeWrapper }:

stdenv.mkDerivation rec {
  name = "wimlib-0.7.2";
  
  src = fetchurl {
    url = "mirror://sourceforge/wimlib/${name}.tar.gz";
    sha256 = "1pxcacgw4rqb47ss5w87dmr2x4hi9d1i7szljzbqlasdwrhyv732";
  };

  buildInputs = [ libxml2 fuse pkgconfig makeWrapper ];
  patches = [ ./mkwinpeimg.patch ];

  postInstall = ''
    wrapProgram $out/bin/mkwinpeimg --prefix PATH ':' \
      ${utillinux}/bin:${cdrkit}/bin:$out/bin:/var/setuid-wrappers/
  '';

  meta = {
    homepage = http://sourceforge.net/projects/wimlib/;
    license = "GPLv2+";
  };
}
