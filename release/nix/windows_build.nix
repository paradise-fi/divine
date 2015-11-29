{stdenv, lib, qemu_kvm, writeText, img, cdrkit, unzip, socat, utillinux, samba, vmTools, writeScript,
 windows_mingw, name ? "build", extraSources ? "",
 tools ? [], src, buildScript, mem ? "1024M" }:

let origtools = tools; origname = name;
    startSamba = ''
      export WHO=`whoami`
      mkdir -p $TMPDIR/xchg

      cat > $TMPDIR/smb.conf <<SMB
      [global]
        private dir = $TMPDIR
        smb ports = 0
        socket address = 127.0.0.1
        pid directory = $TMPDIR
        lock directory = $TMPDIR
        log file = $TMPDIR/log.smbd
        smb passwd file = $TMPDIR/smbpasswd
        security = share
      [store]
        force user = $WHO
        path = /nix/store
        read only = no
        guest ok = yes
      [xchg]
        force user = $WHO
        path = $TMPDIR/xchg
        read only = no
        guest ok = yes
      $EXTRA_SAMBA_CONF
      SMB

      rm -f ./samba
      ${socat}/bin/socat unix-listen:./samba exec:"${utillinux}/bin/setsid ${samba}/sbin/smbd -s $TMPDIR/smb.conf",nofork > /dev/null 2>&1 &
      while [ ! -e ./samba ]; do sleep 0.1; done # ugly
    '';
 in stdenv.mkDerivation rec {

  inherit img unzip cdrkit;
  kvm = qemu_kvm;
  tools = [ windows_mingw ] ++ origtools;
  build = writeText "stage3.sh" buildScript;

  requiredSystemFeatures = [ "kvm" ];

  name = origname + "-" + img.name + (if src ? version then "-" + src.version else "");

  tools_unpack = lib.concatMapStrings (x: ''
    package=${x}
    ${x.unpack}
  '') tools;
  tools_install = lib.concatMapStrings (x: x.install + "\n") tools;

  batch = writeText "batch.cmd" ''
    D:
    stage2.cmd > COM1 2>&1
  '';

  stage2 = writeText "stage2.cmd" ''
    rem run commands to install build dependencies
    fsutil volume diskfree C:
    ${tools_install}
    setx PATH %PATH%
    fsutil volume diskfree C:

:retry
    net use e: \\10.0.2.4\xchg
    if errorlevel 1 goto retry

    C:
    cd \
    mkdir WORK
    cd WORK
    tar xvf /tools/source.tar

    bash /tools/stage3.sh
    if errorlevel 1 goto err
    echo 0 > E:\code.txt
    shutdown /s
    exit 0
:err
    echo 1 > E:\code.txt
    shutdown /s
    exit 1
  '';

  builder = writeScript "${name}-builder" ''
    source $stdenv/setup
    set -x
    mkdir data && cd data
    cp $batch batch.cmd
    cp $stage2 stage2.cmd
    cp $build stage3.sh
    ${tools_unpack}

    mkdir source && cd source
    src=${src}
    if test -d ${src}/tarballs; then src=$(ls $src/tarballs/*.tar.* | sort | head -1); fi
    echo source: $src
    tar xaf $src
    mv */* .
    ${extraSources}
    cd ..
    tar cf source.tar source
    rm -rf source
    cd ..

    ensureDir $out
    $cdrkit/bin/genisoimage -D -J -joliet-long -o tools.iso data

    TMPDIR=`pwd`
    ${startSamba}

    $kvm/bin/qemu-img create -f qcow2 -b $img/hda.img hda.img
    $kvm/bin/qemu-kvm -cdrom tools.iso -hda hda.img -m ${mem} -nographic \
     -chardev socket,id=samba,path=./samba \
     -net nic \
     -net user,guestfwd=tcp:10.0.2.4:445-chardev:samba,restrict=on

    result=`cat xchg/code.txt | sed -e 's,\([0-9]\+\).*,\1,' | head -n 1`
    test "$result" -eq 0 || {
      echo VM failed with code $result
      exit 1
    }

    rm xchg/code.txt
    cp -R xchg/* $out/

    mkdir -p $out/nix-support
    for f in $out/*.exe; do
        echo BUILT: $f
        echo file exe $f >> $out/nix-support/hydra-build-products
    done
  '';
}
