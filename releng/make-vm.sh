resources="-m 12288 -smp 8"
dist=xenial
img=xenial-server-cloudimg-amd64-disk1.img

test -e $img || wget https://cloud-images.ubuntu.com/$dist/current/$img
qemu-img create -o backing_file=$img -f qcow2 divine-$dist.qcow2 20G
qemu-img create -f qcow2 divine-build.qcow2 20G
mkdir -p cloud-config

cat > cloud-config/meta-data <<EOF
instance-id: iid-divine-xenial
local-hostname: divine
EOF

cat > cloud-config/user-data <<EOF
#cloud-config
users:
 - name: divine
   lock_passwd: false
   plain_text_passwd: llvm
   home: /home/divine
   sudo: ALL=(ALL) NOPASSWD:ALL
runcmd:
 - apt-get install -y make
 - mkfs.ext4 /dev/sdb
 - mount /dev/sdb /mnt
 - cd /mnt
 - "curl https://divine.fi.muni.cz/install.sh | sh"
 - apt-get remove -y cloud-init
 - poweroff
EOF

genisoimage -V cidata -D -J -joliet-long -o config.iso cloud-config
rm -rf cloud-config

qemu-system-x86_64 -machine accel=kvm -display none -serial stdio \
                   -hda divine-$dist.qcow2 -hdb divine-build.qcow2 \
                   -cdrom config.iso $resources

rm -f config.iso
