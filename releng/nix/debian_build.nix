# This function compiles a source tarball in a virtual machine image
# that contains a Debian-like (i.e. dpkg-based) OS.

{ name ? "debian-build"
, diskImage
, src, stdenv, vmTools
, # Features provided by this package.
  debProvides ? []
, # Features required by this package.
  debRequires ? []
, ... } @ args:

with stdenv.lib;

vmTools.runInLinuxImage (stdenv.mkDerivation (

  {
    doCheck = true;
    prefix = "/usr";
    prePhases = "setBuildDir installExtraDebsPhase sysInfoPhase";
  }

  // removeAttrs args ["vmTools"] //

  {
    name = name + "-" + diskImage.name + (if src ? version then "-" + src.version else "");

    NIX_BUILD_TOP = "/var/tmp";
    setBuildDir = "cd /var/tmp";

    # !!! cut&paste from rpm-build.nix
    postHook = ''
      mkdir -p $out/nix-support
      cat "$diskImage"/nix-support/full-name > $out/nix-support/full-name

      # If `src' is the result of a call to `makeSourceTarball', then it
      # has a subdirectory containing the actual tarball(s).  If there are
      # multiple tarballs, just pick the first one.
      echo $src
      if test -d $src/tarballs; then
          src=$(ls $src/tarballs/*.tar.bz2 $src/tarballs/*.tar.gz | sort | head -1)
      fi
    ''; # */

    installExtraDebsPhase = ''
      for i in $extraDebs; do
        dpkg --install $(ls $i/debs/*.deb | sort | head -1)
      done
    '';

    sysInfoPhase = ''
      [ ! -f /etc/lsb-release ] || (source /etc/lsb-release; echo "OS release: $DISTRIB_DESCRIPTION")
      echo "System/kernel: $(uname -a)"
      if test -e /etc/debian_version; then echo "Debian release: $(cat /etc/debian_version)"; fi
      header "installed Debian packages"
      dpkg-query --list
      stopNest
    '';

    buildPhase = ''
      eval "$preBuild"
      dpkg-buildpackage -us -uc
      eval "$postBuild"
    '';

    installPhase = ''
      eval "$preInstall"
      export LOGNAME=root

      mkdir -p $out/debs
      cp ../*.deb $out/debs/

      [ "$(echo $out/debs/*.deb)" != "" ]

      for i in $out/debs/*.deb; do
        header "Generated DEB package: $i"
        dpkg-deb --info "$i"
        pkgName=$(dpkg-deb -W "$i" | awk '{print $1}')
        dpkg -i "$i"
        echo "file deb $i" >> $out/nix-support/hydra-build-products
        stopNest
      done

      for i in $extraDebs; do
        echo "file deb-extra $(ls $i/debs/*.deb | sort | head -1)" >> $out/nix-support/hydra-build-products
      done

      eval "$postInstall"
    ''; # */

    meta = (if args ? meta then args.meta else {}) // {
      description = "Build of a Deb package on ${diskImage.fullName} (${diskImage.name})";
    };
  }

))
