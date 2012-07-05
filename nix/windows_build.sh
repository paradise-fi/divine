source $stdenv/setup

ensureDir $out
$kvm/bin/qemu-img create -f qcow2 -b $img/hda.img $out/hda.img
mkdir result

mkdir data
cd data

tar xvzf $source
mv * source

cp $batch batch.cmd
cp $stage2 stage2.cmd

mkdir tools; cd tools

for f in $tools_tar; do
    tar xvaf $f
done

for f in $tools_zip; do
    $unzip/bin/unzip $f >& list
    tops=`grep inflating list | sed -e "s,.*inflating: ,," | cut -d/ -f1 | sort -u`
    echo $tops
    if test `echo $tops | wc -w` = 1; then
        cp -a $tops/* .
        rm -rf $tops
    fi
done

cd ../..

$cdrkit/bin/genisoimage -D -J -o tools.iso data

set -x

TMPDIR=`pwd`
eval "${samba}"
cat smb.conf
touch xchg/empty.txt

$kvm/bin/qemu-kvm -cdrom tools.iso -hda $out/hda.img -m 1024M -nographic \
     -chardev socket,id=samba,path=./samba \
     -net nic \
     -net user,guestfwd=tcp:10.0.2.4:445-chardev:samba,restrict=on

test `cat xchg/code.txt` -eq 0 || {
   echo VM failed with code `cat xchg/code.txt`
   exit 1
}

rm xchg/code.txt
cp xchg/* $out/
