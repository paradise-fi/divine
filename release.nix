{ nixpkgs ? <nixpkgs>, divineSrc, release ? false }:

let
  pkgs = import nixpkgs {};
  debuild = args:
    import ./nix/debian_build.nix ({ stdenv = pkgs.stdenv; vmTools = pkgs.vmTools; } // args);
  rpmbuild = pkgs.releaseTools.rpmBuild;
  rpmbuild_i386 = pkgs.pkgsi686Linux.releaseTools.rpmBuild;
  vmImgs = pkgs.vmTools.diskImageFuns;
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
  extra_debs = [ "cmake" "build-essential" "debhelper" "m4"
                 "libqt4-dev" "libboost-dev" "libncurses5-dev" ];
  extra_rpms = [ "cmake" ];

  mkVM = { VM, extras, diskFun }:
   VM rec {
     name = "divine";
     src = jobs.tarball;
     diskImage = diskFun { extraPackages = extras; };
     configurePhase = ''./configure -DCMAKE_INSTALL_PREFIX=/usr'';
     doCheck = false; # the package builder is supposed to run checks
     memSize = 2047;
   };

  mkbuild = { name, inputs }: { system ? builtins.currentSystem }:
    let pkgs = import nixpkgs { inherit system; }; in
    pkgs.releaseTools.nixBuild {
       name = "divine-" + name;
       src = jobs.tarball;
       buildInputs = [ pkgs.cmake pkgs.perl pkgs.m4 ] ++ inputs { inherit pkgs; };
    };

  versionFile = builtins.readFile ./divine/version.cpp;
  versionLine = builtins.head (
    lib.filter (str: lib.eqStrings (builtins.substring 0 22 str) "#define DIVINE_VERSION")
               (lib.splitString "\n" versionFile));
  version = builtins.head (builtins.tail (lib.splitString "\"" (versionLine + " ")));

  jobs = rec {

    tarball = pkgs.releaseTools.sourceTarball rec {
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

          mv debian/changelog debian/changelog.xxx
          echo "divine (${version}${versionSuffix}) unstable; urgency=low" >> debian/changelog
          echo >> debian/changelog
          echo "  * Automated Hydra build" >> debian/changelog
          echo >> debian/changelog
          echo " -- Petr Rockai <mornfall@debian.org>  `date -R`" >> debian/changelog
          echo >> debian/changelog
          cat debian/changelog.xxx >> debian/changelog
          rm debian/changelog.xxx

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
    full = mkbuild { name = "full"; inputs = { pkgs }:
                      [ pkgs.openmpi pkgs.llvm pkgs.clang pkgs.qt4 ]; };

    ubuntu1204_i386 = mkVM { VM = debuild; diskFun = vmImgs.ubuntu1204i386; extras = extra_debs; };
    ubuntu1110_i386 = mkVM { VM = debuild; diskFun = vmImgs.ubuntu1110i386; extras = extra_debs; };
    fedora16_i386 = mkVM { VM = rpmbuild_i386; diskFun = vmImgs.fedora16i386; extras = extra_rpms; };

    windows_i386 = pkgs.callPackage nix/windows_build.nix {
      inherit windows_mingw;
      tools = [ windows_cmake windows_nsis ];
      img = windows7_img;
      src = jobs.tarball;
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
