source $stdenv/setup

ensureDir $out

$kvm/bin/qemu-img create -f qcow2 $out/hda.img 8G
$kvm/bin/qemu-kvm -cdrom ${winpe_iso}/boot.iso -drive file=$iso,index=3,media=cdrom \
    -hda $out/hda.img -m 1024M -nographic -net nic -net user,restrict=on
