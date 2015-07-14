{ nixpkgs ? <nixpkgs>, divineSrc ? ./., release ? false, buildType ? "Debug" }:

let
  pkgs = import nixpkgs {};
  debuild = arch: args:
    import ./debian_build.nix ({ stdenv = pkgs.stdenv; vmTools = pkgs.vmTools; } // args);
  rpmbuild = arch: args: if arch == "i386"
      then pkgs.pkgsi686Linux.releaseTools.rpmBuild
               (args // (if args ? memSize && args.memSize > 2047 then { memSize = 2047; } else {}))
      else pkgs.releaseTools.rpmBuild args;
  vmImgs = pkgs.vmTools.diskImageFuns;
  lib = pkgs.lib;

  wimlib = pkgs.callPackage ./wimlib.nix {};
  windows7_iso = pkgs.fetchurl {
    url = "http://msft.digitalrivercontent.net/win/X17-59183.iso";
    sha256 = "13l3skfp3qi2ccv9djhpg7a7f2g57rph8n38dnkw8yh8w1bdyk7x";
  };

  windows7_img = pkgs.callPackage ./windows_img.nix {
    inherit wimlib;
    iso = windows7_iso;
    name = "windows7";
    vmTools = import "${nixpkgs}/pkgs/build-support/vm/default.nix" {
       inherit pkgs;
       rootModules = [ "virtio_net" "virtio_pci" "virtio_blk" "virtio_balloon"
                       "9p" "9pnet_virtio" "ext4" "fuse" "loop" "udf" ];
    };
  };

  windows_cmake = pkgs.callPackage ./windows_cmake.nix {};
  windows_mingw = pkgs.callPackage ./windows_mingw.nix {};
  windows_nsis = pkgs.callPackage ./windows_nsis.nix {};
  windows_qt = pkgs.callPackage ./windows_qt.nix {
      windows_img = windows7_img; inherit pkgs windows_mingw; };
  windows_python = pkgs.callPackage ./windows_python.nix {};
  windows_llvm = pkgs.callPackage ./windows_llvm.nix {
      windows_img = windows7_img;
      inherit pkgs windows_mingw windows_cmake windows_python;
      build_clang = true;
  };

  extra_debs = [ "cmake" "build-essential" "debhelper" "m4"
                 "libqt4-dev" "libboost-dev" "libncurses5-dev"
                 "binutils-gold" "libxml2-dev" ];
  extra_debs34 = extra_debs ++ [ "llvm-3.4-dev" "clang-3.4" ];
  extra_rpms = [ "cmake" "redhat-rpm-config" "llvm-devel" "clang" "libxml2-devel" "boost-devel" "bison" "llvm-static" ];

  mkVM = { VM, extras, disk, mem ? 3072,
           name, tarball ? jobs.tarball, extra_opts ? "-DMIN_INSTANCES_PER_FILE=4 -DMAX_INSTANCE_FILES_COUNT=100" }: arch:
     let flags = "-DCMAKE_BUILD_TYPE=${buildType} ${extra_opts}";
         nicesys = if lib.eqStrings arch "i386" then "x86" else
                      (if lib.eqStrings arch "x86_64" then "x64" else "unknown");
   in (VM arch) {
     name = "divine";
     src = tarball;
     systems = [];
     diskImage = ((builtins.getAttr (disk + arch) vmImgs) { extraPackages = extras; size = 14000; }) //
                 { name = name + "_" + (lib.toLower buildType) + "_" + nicesys; };
     CMAKE_FLAGS = flags;
     NIX_BUILD = 1;
     BATCH = 1;
     dontUseTmpfs = 1;

     # this actually runs just in debian-based distros, RPM based does not have configurePhase
     configurePhase = ''
          echo "${flags}" > pkgbuildflags
     '';
     doCheck = false; # the package builder is supposed to run checks
     memSize = mem;
   };

  mkbuild = { name, inputs,
              flags ? [],
              clang ? false,
              compilerPkg ? (pkgs: if clang then pkgs.clangSelf else pkgs.gcc),
              clang_runtime ? (pkgs: pkgs.clang), # version of clang used in divine compile --llvm
              systems ? []
            }: system:
    let pkgs = import nixpkgs { inherit system; };
        nicesys = if lib.eqStrings system "i686-linux" then "x86" else
                     (if lib.eqStrings system "x86_64-linux" then "x64" else "unknown");
        cmdflags = [ "-DCMD_CC=${compilerPkg pkgs}/bin/cc"
                     "-DCMD_CXX=${compilerPkg pkgs}/bin/c++"
                     "-DCMD_CLANG=${(clang_runtime pkgs).clang}/bin/clang" ];
        debug = if clang then [] else [ "-DCMAKE_CXX_FLAGS_DEBUG=-gsplit-dwarf" ];
        profile = if lib.eqStrings buildType "Debug" && !clang
                     then [ "-DDEV_GCOV=ON -DGCOV=${pkgs.gcc.gcc}/bin/gcov" ] else [];
        compiler =    [ "-DCMAKE_CXX_COMPILER=${compilerPkg pkgs}/bin/c++"
                        "-DCMAKE_C_COMPILER=${compilerPkg pkgs}/bin/cc"
                        "-DMAX_INSTANCE_FILES_COUNT=260" "-DMIN_INSTANCES_PER_FILE=4" ];
    in pkgs.releaseTools.nixBuild {
       name = "divine-" + name + "_" + (lib.toLower buildType) + "_" + nicesys;
       inherit systems;
       src = jobs.tarball;
       buildInputs = [ pkgs.cmake pkgs.perl pkgs.m4 pkgs.lcov pkgs.which ] ++ inputs pkgs;
       cmakeFlags = [ "-DCMAKE_BUILD_TYPE=${buildType}" ] ++ compiler ++ cmdflags ++ debug ++ profile ++ flags;
       dontStrip = true;
       checkPhase = ''
          ulimit -c unlimited
          NIX_BUILD=1 BATCH=1 make check
          make lcov-report && \
            cp -R lcov-report $out/ && \
            echo "report coverage $out/lcov-report" >> $out/nix-support/hydra-build-products || \
            true
       '';
    };

  mkwin = image: flags: deps: pkgs.callPackage ./windows_build.nix {
    inherit windows_mingw;
    tools = [ windows_cmake windows_nsis ] ++ deps;
    img = image // { name = "win7_${lib.toLower buildType}_x86"; };
    src = jobs.tarball;
    name = "divine";
    mem = "2048M";
    buildScript = ''
      set -ex
      mkdir build && cd build
      # Windows/mingw breaks on big files :-(
      bt=${buildType}
      test "$bt" = "RelWithDebInfo" && echo ${flags} | grep -v SMALL && bt=Release
      cmake -LA -G "MSYS Makefiles" \
        -DQT_UIC_EXECUTABLE=$QTDIR/bin/uic.exe \
        -DQT_RCC_EXECUTABLE=$QTDIR/bin/rcc.exe \
        -DQT_MOC_EXECUTABLE=$QTDIR/bin/moc.exe \
        -DQT_QCOLLECTIONGENERATOR_EXECUTABLE=$QTDIR/bin/qcollectiongenerator.exe \
        -DQT_INCLUDE_DIR=$QTDIR/include \
        -DQT_QTCORE_INCLUDE_DIR=$QTDIR/include/QtCore \
        -DQT_QTGUI_INCLUDE_DIR=$QTDIR/include/QtGui \
        -DQT_QTXML_INCLUDE_DIR=$QTDIR/include/QtXml \
        -DLLVM_INCLUDE_DIRS=D:\\llvm\\include \
        -DLLVM_LIBRARY_DIRS=D:\\llvm\\lib \
        -DRX_PATH=D:\\mingw\\include \
        -DCMAKE_BUILD_TYPE=$bt ${flags} ../source
      make VERBOSE=1
      mkdir E:\\nix-support
      make unit || touch E:\\nix-support\\failed
      make functional || touch E:\\nix-support\\failed
      make package || touch E:\\nix-support\\failed
      cp tools/divine.exe E:/
      cp divine-*.exe E:/ || true
    '';
  };

  versionMaj = lib.removeSuffix "\n" (builtins.readFile ../release/version);
  versionPatch = lib.removeSuffix "\n" (builtins.readFile ../release/patchlevel);
  version = "${versionMaj}.${versionPatch}";

  gcc_llvm_vers = llvm: clang: with builtins; mk: mk {
      inputs = pkgs: [ (getAttr llvm pkgs) (getAttr clang pkgs) ];
      clang_runtime = pkgs: getAttr clang pkgs;
  };

  vms = {
    debian70   = mk: mk { VM = debuild; disk = "debian70"; extras = extra_debs; };
    ubuntu1210 = mk: mk { VM = debuild; disk = "ubuntu1210"; extras = extra_debs; };
    ubuntu1304 = mk: mk { VM = debuild; disk = "ubuntu1304"; extras = extra_debs; };
    ubuntu1310 = mk: mk { VM = debuild; disk = "ubuntu1310"; extras = extra_debs34; };
    ubuntu1404 = mk: mk { VM = debuild; disk = "ubuntu1404"; extras = extra_debs34; };

    fedora20   = mk: mk { VM = rpmbuild; disk = "fedora20"; extras = extra_rpms; };
  };

  builds = let
    allFlags = [ "-DALG_OWCTY=ON" "-DALG_EXPLICIT=ON" "-DALG_CSDR=ON" "-DALG_WEAKREACHABILITY=ON"
                 "-DALG_NDFS=ON" "-DALG_MAP=ON" "-DGEN_EXPLICIT=ON" "-DGEN_EXPLICIT_PROB=ON"
                 "-DGEN_LLVM_PROB=ON" "-DGEN_LLVM_PTST=ON" "-DGEN_DUMMY=ON"
               ];
    allInCommon = pkgs: [ pkgs.openmpi pkgs.qt4 pkgs.libxml2 pkgs.boost pkgs.flex pkgs.byacc ];
    allInGcc = pkgs: (allInCommon pkgs ++ [ pkgs.llvm pkgs.clang ]);
    allInClang = pkgs: (allInCommon pkgs ++ [ pkgs.llvmPackagesSelf.llvm pkgs.clangSelf ]);
  in {
    gcc_min =   mk: mk { inputs = pkgs: []; };
    gcc_def   = mk: mk { inputs = allInGcc; };
    gcc_all   = mk: mk { inputs = allInGcc; flags = allFlags; systems = [ "x86_64-linux" ]; };

    gcc49_all = mk: mk { inputs = allInGcc; flags = allFlags; compilerPkg = pkgs: pkgs.gcc49;
                         systems = [ "x86_64-linux" ]; };

    clang_min = mk: mk { inputs = pkgs: []; clang = true; };
    clang_def = mk: mk { inputs = allInClang; clang = true; systems = [ "x86_64-linux" ]; };
    clang_all = mk: mk { inputs = allInClang; flags = allFlags; clang = true;
                         systems = [ "x86_64-linux" ]; };

    gcc_llvm_33 =  gcc_llvm_vers "llvm_33" "clang_33";
    gcc_llvm_34 =  gcc_llvm_vers "llvm_34" "clang_34";

  };

  windows = {
    win7_gui.i386 = mkwin windows7_img "" [ windows_qt ];
    win7.i386 = mkwin windows7_img "" [];
    win7_llvm.i386 = mkwin windows7_img "" [ windows_llvm ];
  };

  mapsystems = systems: attrs: with ( pkgs.lib // builtins );
      mapAttrs ( n: fun: listToAttrs ( filter ( a: any (s: a.name == s) a.value.systems || a.value.systems == [] )
          ( map (sys: { name = sys; value = fun sys; }) systems ) ) ) attrs;

  namedbuild = build: name: fun: fun (attrs: build ({ "name" = name; } // attrs));

  jobs = rec {

    tarball = pkgs.releaseTools.sourceTarball rec {
        inherit version;
        name = "divine-tarball";
        versionSuffix = if divineSrc ? revCount
                           then "+pre${toString divineSrc.revCount}"
                           else "";
        src = divineSrc;
        buildInputs = (with pkgs; [ cmake which ]);
        cmakeFlags = [ "-DVERSION_APPEND=${versionSuffix}" ];
        dontFixCmake = true;
        autoconfPhase = ''
          sed -e 's,"","${versionSuffix}",' -i cmake/VersionAppend.cmake
          ${pkgs.haskellPackages.pandoc}/bin/pandoc --from markdown --to html README > README.html

          sed -e "s,^\(Version:.*\)0$,\1${version}${versionSuffix}," -i divine.spec
          mv debian/changelog debian/changelog.xxx
          echo "divine (${version}${versionSuffix}) unstable; urgency=low" >> debian/changelog
          echo >> debian/changelog
          echo "  * Automated build" >> debian/changelog
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

    manual =
     let tex = pkgs.texLiveAggregationFun { paths = [ pkgs.texLive pkgs.lmodern ]; };
          in pkgs.releaseTools.nixBuild {
              name = "divine-manual";
              src = jobs.tarball;
              buildInputs = [ pkgs.cmake pkgs.perl pkgs.which
                              pkgs.haskellPackages.pandoc tex ];
              buildPhase = "make manual website";
              installPhase = ''
                mkdir $out/manual $out/website
                cp manual/manual.pdf manual/manual.html $out/manual/
                cp website/*.html website/*.png website/*.css website/*.js $out/website/ #*/
                cp ../website/template.html $out/website
              '';
              checkPhase = ":";
          };
  } // mapsystems [ "i686-linux" "x86_64-linux" ] (lib.mapAttrs (namedbuild mkbuild) builds)
    // mapsystems [ "i386" "x86_64" ] (lib.mapAttrs (namedbuild mkVM) vms) // windows;

in
  jobs
