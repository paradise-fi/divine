#!/bin/sh

cat <<EOF >> /etc/issue
DIVINE 4 Demo VM

login: divine
password: llvm

EOF

mkdir /tmp/vboxcd
mount /dev/disk/by-label/VBOX* /tmp/vboxcd
/tmp/vboxcd/VBoxLinuxAdditions.run --nox11

cat <<EOF >> /etc/systemd/system/divine-job.service
[Unit]
Description=DIVINE jobdef service
Requires=network.target
After=network.target

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/mkdir -p /mnt/divine-job
ExecStart=/bin/mount /dev/disk/by-label/divine-job /mnt/divine-job
ExecStart=/mnt/divine-job/run

[Install]
WantedBy=multi-user.target
EOF

cat <<EOF >> /etc/systemd/system/vboxmount-divine.service
[Unit]
Description=DIVINE VirtualBox mount service
Requires=vboxadd.service
Requires=vboxadd-service.service
After=vboxadd.servicea
After=vboxadd-service.service

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/bin/mkdir -p /mnt/divine-vbox
ExecStart=/bin/mount -t vboxsf divine-vbox /mnt/divine-vbox

[Install]
WantedBy=multi-user.target
EOF

cat <<EOF > /etc/network/interfaces.d/51-vbox.cfg
auto enp0s3
iface enp0s3 inet dhcp
EOF

systemctl daemon-reload
systemctl enable divine-job.service
systemctl enable vboxmount-divine.service
