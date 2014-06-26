{ stdenv, fetchurl, windows_img, windows_mingw, windows_cmake, windows_python, writeScript, pkgs,
  build_clang ? false
}:
 
stdenv.mkDerivation rec {
  name = "windows-llvm";
  pkg = "llvm-3.4.2-x86-win32";

  source = fetchurl {
      url = "http://llvm.org/releases/3.4.2/llvm-3.4.2.src.tar.gz";
      sha256 = "1mzgy7r0dma0npi1qrbr1s5n4nbj1ipxgbiw0q671l4s0r3qs0qp";
  };

  clang = fetchurl {
      url = "http://llvm.org/releases/3.4.2/cfe-3.4.2.src.tar.gz";
      sha256 = "045wjnp5j8xd2zjhvldcllnwlnrwz3dafmlk412z804d5xvzb9jv";
  };

  compRT = fetchurl {
      url = "http://llvm.org/releases/3.4/compiler-rt-3.4.src.tar.gz";
      sha256 = "0p5b6varxdqn7q3n77xym63hhq4qqxd2981pfpa65r1w72qqjz7k";
  };

  build = pkgs.callPackage ./windows_build.nix {
    inherit windows_mingw;
    tools = [ windows_cmake windows_python ];
    img = windows_img;
    src = source;
    name = "llvm";
    mem = "2048M";
    buildScript = ''
      set -ex
      mkdir build && cd build
      cmake -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=E:\\llvm -DLLVM_TARGETS_TO_BUILD=X86 \
          -DBUILD_SHARED_LIBS=OFF ../source
      cat CMakeCache.txt
      make
      make install
    '';

    extraSources = if !build_clang then "" else ''
      tar xaf ${clang}
      mv cfe-*.src tools/clang

      tar xaf ${compRT}
      mv compiler-rt-* projects/compiler-rt
    '';
  };

  builder = writeScript "${name}-builder" ''
      source $stdenv/setup
      ensureDir $out
      ln -s ${build}/llvm $out/llvm
  '';

  unpack = "cp -R ${build}/llvm .";
  install = ''set PATH=%PATH%;D:\llvm\bin'';
}
