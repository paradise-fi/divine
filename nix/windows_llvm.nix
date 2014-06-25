{stdenv, fetchurl, windows_img, windows_mingw, windows_cmake, windows_python, writeScript, pkgs}:
 
stdenv.mkDerivation rec {
  name = "windows-llvm";
  pkg = "llvm-3.4.2-x86-win32";

  source = fetchurl {
      url = "http://llvm.org/releases/3.4.2/llvm-3.4.2.src.tar.gz";
      sha256 = "1mzgy7r0dma0npi1qrbr1s5n4nbj1ipxgbiw0q671l4s0r3qs0qp";
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
      cmake -G "MSYS Makefiles" -DCMAKE_INSTALL_PREFIX=E:\\llvm -DLLVM_TARGETS_TO_BUILD=X86 ../source
      make
      make install
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
