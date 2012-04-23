{ nixpkgs ? <nixpkgs> }:

let
  pkgs = import nixpkgs {};

  mkbuild = { name, inputs }: { system ? builtins.currentSystem, divineSrc ? src }:
    let pkgs = import nixpkgs { inherit system; }; in
    pkgs.releaseTools.nixBuild {
       name = "divine-" + name;
       src = jobs.tarball { inherit divineSrc; };
       buildInputs = [ pkgs.cmake pkgs.perl ] ++ inputs { inherit pkgs; };
    };

  src = pkgs.fetchurl {
    url = "http://divine.fi.muni.cz/divine-2.5.2.tar.gz";
    sha256 = "0sxnpqrv9wbfw1m1pm9jzd7hs02yvvnvkm8qdbafhdl2qmll7c0l";
  };

  jobs = rec {

    tarball = { divineSrc ? src }: 
      pkgs.releaseTools.sourceTarball { 
        name = "divine-tarball";
        src = divineSrc;
        buildInputs = (with pkgs; [ cmake ]);
        distPhase = ''
            make package_source
            mkdir $out/tarballs
            cp divine-*.tar.gz $out/tarballs
        '';
      };

    minimal = mkbuild { name = "minimal"; inputs = { pkgs }: []; };
    mpi = mkbuild { name = "mpi"; inputs = { pkgs }: [ pkgs.openmpi ]; };
    gui = mkbuild { name = "gui"; inputs = { pkgs }: [ pkgs.qt4 ]; };
    llvm = mkbuild { name = "llvm"; inputs = { pkgs }: [ pkgs.llvm pkgs.clang ]; };
    full = mkbuild { name = "full"; inputs = { pkgs }: [ pkgs.openmpi pkgs.llvm pkgs.clang pkgs.qt4 ]; };

    debian6_i386 = { divineSrc ? src }:
      debuild rec {
        name = "divine";
        src = jobs.tarball { inherit divineSrc; };
        diskImage = pkgs.vmTools.diskImageFuns.debian60i386 {
          extraPackages = [ "cmake" "build-essential" "debhelper" ]; };
        memSize = 2047;
        configurePhase = ''./configure -DCMAKE_INSTALL_PREFIX=/usr'';
      };

    ubuntu1204_i386 = { divineSrc ? src }:
      debuild rec {
        name = "divine-deb";
        src = jobs.tarball { inherit divineSrc; };
        diskImage = pkgs.vmTools.diskImageFuns.ubuntu1204i386 {
          extraPackages = [ "cmake" "build-essential" "debhelper" ]; };
        memSize = 2047;
        configurePhase = ''./configure -DCMAKE_INSTALL_PREFIX=/usr'';
      };

    fedora16_i386 = { divineSrc ? src }:
      pkgs.releaseTools.rpmBuild rec {
        name = "divine-rpm";
        src = jobs.tarball { inherit divineSrc; };
        diskImage = pkgs.vmTools.diskImageFuns.fedora16i386 { extraPackages = [ "cmake" ]; };
        memSize = 2047;
      };
  };
in
  jobs
