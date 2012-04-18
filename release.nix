{ nixpkgs ? <nixpkgs> }:

let
  pkgs = import nixpkgs {}; 

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
      };

    build = 
      { system ? builtins.currentSystem,
        divineSrc ? src }:

      let pkgs = import nixpkgs { inherit system; }; in
      pkgs.releaseTools.nixBuild { 
        name = "divine" ;
        src = jobs.tarball { inherit divineSrc; };
        configureFlags = [ "--disable-silent-rules" ];
      };
  };
in
  jobs
