{ nixpkgs ? <nixpkgs>, divineSrc ? ./., release ? false, buildType ? "Debug" }:

let
  pkgs = import nixpkgs {};
  debuild = arch: args:
    import ./nix/debian_build.nix ({ stdenv = pkgs.stdenv; vmTools = pkgs.vmTools; } // args);
  rpmbuild = arch: args: if arch == "i386"
      then pkgs.pkgsi686Linux.releaseTools.rpmBuild
               (args // (if args ? memSize && args.memSize > 2047 then { memSize = 2047; } else {}))
      else pkgs.releaseTools.rpmBuild args;
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
    vmTools = import "${nixpkgs}/pkgs/build-support/vm/default.nix" {
       inherit pkgs;
       rootModules = [ "virtio_net" "virtio_pci" "virtio_blk" "virtio_balloon"
                       "9p" "9pnet_virtio" "ext4" "fuse" "loop" "udf" ];
    };
  };

  windows_cmake = pkgs.callPackage nix/windows_cmake.nix {};
  windows_mingw = pkgs.callPackage nix/windows_mingw.nix {};
  windows_nsis = pkgs.callPackage nix/windows_nsis.nix {};
  windows_qt = pkgs.callPackage nix/windows_qt.nix {
      windows_img = windows7_img; inherit pkgs windows_mingw; };
  windows_python = pkgs.callPackage nix/windows_python.nix {};
  windows_llvm = pkgs.callPackage nix/windows_llvm.nix {
      windows_img = windows7_img;
      inherit pkgs windows_mingw windows_cmake windows_python;
      build_clang = true;
  };

  extra_debs = [ "cmake" "build-essential" "debhelper" "m4"
                 "libqt4-dev" "libboost-dev" "libncurses5-dev"
                 "binutils-gold" "libxml2-dev" ];
  extra_debs32 = extra_debs ++ [ "llvm-3.2-dev" "clang-3.2" ];
  extra_debs34 = extra_debs ++ [ "llvm-3.4-dev" "clang-3.4" ];
  extra_rpms = [ "cmake" "redhat-rpm-config" "llvm-devel" "clang" "libxml2-devel" "boost-devel" "bison" "llvm-static" ];

  mkVM = { VM, extras, disk, mem ? 3072, require ? "DVE;LLVM;TIMED;CESMI;COMPRESS;EXPLICIT",
           tarball ? jobs.tarball }: arch:
   (VM arch) rec {
     name = "divine";
     src = tarball;
     diskImage = (builtins.getAttr (disk + arch) vmImgs) { extraPackages = extras; size = 10240; };
     CMAKE_FLAGS = "-DCMAKE_BUILD_TYPE=${buildType} -DREQUIRED=${require}";
     NIX_BUILD = 1;
     BATCH = 1;
     dontUseTmpfs = 1;

     # this actually runs just in debian-based distros, RPM based does not have configurePhase
     configurePhase = ''
          echo "${CMAKE_FLAGS}" > pkgbuildflags
          echo "override_dh_auto_test:" >> debian/rules
          echo "	dh_auto_test || touch $out/nix-support/failed" >> debian/rules
     '';
     doCheck = false; # the package builder is supposed to run checks
     memSize = mem;
   };

  mkbuild = { name, inputs,
              flags ? [ "-DSTORE_COMPRESS=OFF" "-DGEN_EXPLICIT=OFF" ],
              clang ? false,
              clang_runtime ? (pkgs: pkgs.clang), # version of clang used in divine compile --llvm
              llvm ? (pkgs: pkgs.llvm)
            }: system:
    let pkgs = import nixpkgs { inherit system; };
        nicesys = if lib.eqStrings system "i686-linux" then "x86" else
                     (if lib.eqStrings system "x86_64-linux" then "x64" else "unknown");
        cmdflags = [ "-DCMD_GCC=${pkgs.gcc}/bin/gcc" ] ++
                   (if lib.eqStrings (builtins.substring 0 4 name) "llvm" ||
                       lib.eqStrings name "full" ||
                       lib.eqStrings name "medium"
                      then [ "-DCMD_CLANG=${(clang_runtime pkgs).clang}/bin/clang"
                             "-DCMD_AR=${pkgs.gcc.binutils}/bin/ar"
                             "-DCMD_GOLD=${pkgs.gcc.binutils}/bin/ld.gold"
                             "-DCMD_LLVMGOLD=${llvm pkgs}/lib/LLVMgold.so" ]
                      else []);
        profile = if lib.eqStrings buildType "Debug" && !clang
                     then [ "-DPROFILE=ON" "-DGCOV=${pkgs.gcc.gcc}/bin/gcov" ] else [];
        compiler = if clang
                      then [ "-DCMAKE_CXX_COMPILER=${pkgs.clangSelf}/bin/clang++"
                             "-DCMAKE_C_COMPILER=${pkgs.clangSelf}/bin/clang" ]
                      else [ "-DCMAKE_CXX_COMPILER=${pkgs.gcc}/bin/g++"
                             "-DCMAKE_C_COMPILER=${pkgs.gcc}/bin/gcc" ];
    in pkgs.releaseTools.nixBuild {
       name = "divine-" + name + "_" + (lib.toLower buildType) + "_" + nicesys;
       src = jobs.tarball;
       buildInputs = [ pkgs.cmake pkgs.perl pkgs.m4 pkgs.lcov pkgs.which ] ++ inputs pkgs;
       cmakeFlags = [ "-DCMAKE_BUILD_TYPE=${buildType}" ] ++ compiler ++ cmdflags ++ profile ++ flags;
       dontStrip = true;
       checkPhase = ''
          make unit || touch $out/nix-support/failed
          BATCH=1 make functional || touch $out/nix-support/failed
          cp -R test/results $out/test-results && \
            echo "report tests $out/test-results" >> $out/nix-support/hydra-build-products || true
          make lcov-report && \
            cp -R lcov-report $out/ && \
            echo "report coverage $out/lcov-report" >> $out/nix-support/hydra-build-products || \
            true
       '';
    };

  mkwin = image: flags: deps: pkgs.callPackage nix/windows_build.nix {
    inherit windows_mingw;
    tools = [ windows_cmake windows_nsis ] ++ deps;
    img = image;
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
      llvm = pkgs: getAttr llvm pkgs;
      clang_runtime = pkgs: getAttr clang pkgs;
      flags = [ "-DREQUIRED=LLVM" ];
  };

  vms = {
    debian70   = mkVM { VM = debuild; disk = "debian70"; extras = extra_debs; require="DVE;TIMED;CESMI"; };
    ubuntu1210 = mkVM { VM = debuild; disk = "ubuntu1210"; extras = extra_debs; require="DVE;TIMED;CESMI"; };
    ubuntu1304 = mkVM { VM = debuild; disk = "ubuntu1304"; extras = extra_debs32; };
    ubuntu1310 = mkVM { VM = debuild; disk = "ubuntu1310"; extras = extra_debs34; };
    ubuntu1404 = mkVM { VM = debuild; disk = "ubuntu1404"; extras = extra_debs34; };

    fedora19   = mkVM { VM = rpmbuild; disk = "fedora19"; extras = extra_rpms; tarball = tarball_with_binutils; };
    fedora20   = mkVM { VM = rpmbuild; disk = "fedora20"; extras = extra_rpms; tarball = tarball_with_binutils; };
  };

  binutils = pkgs.fetchurl {
      url = "http://ftp.gnu.org/gnu/binutils/binutils-2.24.tar.bz2";
      sha256 = "0ds1y7qa0xqihw4ihnsgg6bxanmb228r228ddvwzgrv4jszcbs75";
  };

  tarball_with_binutils = pkgs.runCommand "divine-tarball" {} ''
      set -ex
      archive=`ls ${jobs.tarball}/tarballs`
      echo $archive
      tar xaf ${jobs.tarball}/tarballs/divine-*.tar.gz
      cd divine-*/external/binutils
      tar xaf ${binutils}
      cd -
      mkdir -p $out/tarballs
      tar caf $out/tarballs/$archive divine-*
  '';

  builds = {
    gcc_min =      mk: mk { inputs = pkgs: []; };
    gcc_mpi =      mk: mk { inputs = pkgs: [ pkgs.openmpi ]; };
    gcc_gui =      mk: mk { name = "gui"; inputs = pkgs: [ pkgs.qt4 ]; };

    gcc_llvm =     mk: mk { name = "llvm"; inputs = pkgs: [ pkgs.llvm pkgs.clang ]; };
    gcc_llvm_31 =  gcc_llvm_vers "llvm_31" "clang_31";
    gcc_llvm_32 =  gcc_llvm_vers "llvm_32" "clang_32";
    gcc_llvm_33 =  gcc_llvm_vers "llvm_33" "clang_33";
    gcc_llvm_34 =  gcc_llvm_vers "llvm_34" "clang_34";

    gcc_timed =    mk: mk { inputs = pkgs: [ pkgs.libxml2 pkgs.boost ];
                           flags = [ "-DREQUIRED=TIMED" ]; };
    gcc_compress = mk: mk { name = "compression"; inputs = pkgs: [];
                            flags = [ "-DSTORE_HC=OFF" "-DSTORE_COMPRESS=ON" "-DGEN_EXPLICIT=OFF" ]; };
    gcc_hashcomp = mk: mk { inputs = pkgs: [];
                            flags = [ "-DSTORE_COMPRESS=OFF" "-DSTORE_HC=ON" "-DGEN_EXPLICIT=OFF" ]; };
    gcc_explicit = mk: mk { inputs = pkgs: [];
                            flags = [ "-DSTORE_COMPRESS=OFF" "-DALG_EXPLICIT=ON" "-DGEN_EXPLICIT=ON" ]; };
    gcc_full =     mk: mk { inputs = pkgs: [ pkgs.openmpi pkgs.llvm pkgs.clang pkgs.qt4 pkgs.libxml2 pkgs.boost ];
                            flags = [ "-DREQUIRED=DVE;LLVM;TIMED;CESMI;COMPRESS;EXPLICIT" ]; };
    clang_min =    mk: mk { inputs = pkgs: []; clang = true; };
    clang_medium = mk: mk { inputs = pkgs: [ pkgs.openmpi pkgs.llvmPackagesSelf.llvm pkgs.clangSelf pkgs.libxml2 ];
                            flags = []; clang = true; };
  };

  windows = {
    win7_gui.i386 = mkwin windows7_img "" [ windows_qt ];
    win7.i386 = mkwin windows7_img "" [];
    win7_llvm.i386 = mkwin windows7_img "-DREQUIRED=LLVM" [ windows_llvm ];
  };

  mapsystems = systems: attrs: with ( pkgs.lib // builtins );
    mapAttrs ( n: fun: listToAttrs ( map (sys: { name = sys; value = fun sys; }) systems ) ) attrs;

  namedbuild = name: fun: fun (attrs: mkbuild ({ "name" = name; } // attrs));

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
  } // mapsystems [ "i686-linux" "x86_64-linux" ] (lib.mapAttrs namedbuild builds)
    // mapsystems [ "i386" "x86_64" ] vms // windows;

in
  jobs
