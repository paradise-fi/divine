{ nixpkgs ? <nixpkgs> }:

let
  pkgs = import nixpkgs {};
  lib = pkgs.lib;

  wimlib = pkgs.callPackage nix/wimlib.nix {};
  windows7_iso = pkgs.fetchurl {
    url = "http://msft.digitalrivercontent.net/win/X17-59183.iso";
    sha256 = "13l3skfp3qi2ccv9djhpg7a7f2g57rph8n38dnkw8yh8w1bdyk7x";
  };

  windows7_img = pkgs.callPackage nix/windows_img.nix {
    inherit wimlib;
    iso = windows7_iso;
    name = "windows7";
  };
  windows_cmake = pkgs.callPackage nix/windows_cmake.nix {};
  windows_mingw = pkgs.callPackage nix/windows_mingw.nix {};
  windows_nsis = pkgs.callPackage nix/windows_nsis.nix {};

  mkbuild = { name, inputs }: { system ? builtins.currentSystem, divineSrc ? src }:
    let pkgs = import nixpkgs { inherit system; }; in
    pkgs.releaseTools.nixBuild {
       name = "divine-" + name;
       src = jobs.tarball { inherit divineSrc; };
       buildInputs = [ pkgs.cmake pkgs.perl pkgs.m4 ] ++ inputs { inherit pkgs; };
    };

  src = pkgs.fetchurl {
    url = "http://divine.fi.muni.cz/divine-2.5.2.tar.gz";
    sha256 = "0sxnpqrv9wbfw1m1pm9jzd7hs02yvvnvkm8qdbafhdl2qmll7c0l";
  };

  versionFile = builtins.readFile ./divine/version.cpp;
  versionLine = builtins.head (
    lib.filter (str: lib.eqStrings (builtins.substring 0 22 str) "#define DIVINE_VERSION")
               (lib.splitString "\n" versionFile));
  version = builtins.head (builtins.tail (lib.splitString "\"" (versionLine + " ")));

  jobs = rec {

    tarball = { divineSrc ? src }:
      pkgs.releaseTools.sourceTarball rec {
        inherit version;
        name = "divine-tarball";
        versionSuffix = if divineSrc ? revCount
                           then "+pre${toString divineSrc.revCount}"
                           else "";
        src = divineSrc;
        buildInputs = (with pkgs; [ cmake ]);
        cmakeFlags = [ "-DVERSION_APPEND=${versionSuffix}" ];
        autoconfPhase = ''
          sed -e "s,^\(Version:.*\)0$,\1${version}${versionSuffix}," -i divine.spec
          sed -e 's,"","${versionSuffix}",' -i cmake/VersionAppend.cmake
          chmod +x configure # ha-ha
        '';
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
        name = "divine";
        src = jobs.tarball { inherit divineSrc; };
        diskImage = pkgs.vmTools.diskImageFuns.ubuntu1204i386 {
          extraPackages = [ "cmake" "build-essential" "debhelper" ]; };
        memSize = 2047;
        configurePhase = ''./configure -DCMAKE_INSTALL_PREFIX=/usr'';
      };

    fedora16_i386 = { divineSrc ? src }:
      pkgs.releaseTools.rpmBuild rec {
        name = "divine";
        src = jobs.tarball { inherit divineSrc; };
        diskImage = pkgs.vmTools.diskImageFuns.fedora16i386 { extraPackages = [ "cmake" ]; };
        memSize = 2047;
      };

    windows_i386 = { divineSrc ? src }: pkgs.callPackage nix/windows_build.nix {
      inherit windows_mingw;
      tools = [ windows_cmake windows_nsis ];
      img = windows7_img;
      src = jobs.tarball { inherit divineSrc; };
      name = "divine";
      buildScript = ''
        set -ex
        mkdir build && cd build
        cmake -G "MSYS Makefiles" -DRX_PATH=D:\\mingw\\include -DHOARD=OFF ../source
        make
        make check || true # ignore failures for now
        make package
        cp tools/divine.exe E:/
        cp divine-*.exe E:/
      '';
  };
  };
in
  jobs
