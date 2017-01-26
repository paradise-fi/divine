#!/bin/sh

set -e
version=4.0.0
prefix=/opt/divine

if test -d $prefix; then
    echo "DIVINE appears to be already installed in $prefix"
    echo -n "your installed divine says "
    $prefix/bin/divine version 2> /dev/null | grep ^version:
    echo "the current version is:             $version"
    echo "if you wish to upgrade, please remove your current installation and try again"
    exit 1
fi

wget https://divine.fi.muni.cz/download/divine-$version.tar.gz
tar xzf divine-$version.tar.gz
cd divine-$version

make prerequisites
make
make install

echo "DIVINE's binaries are installed in $prefix/bin"

if test -d /etc/profile.d; then
    echo "PATH=$prefix/bin:"'$PATH' > /etc/profile.d/divine-path.sh
    echo "I have created /etc/profile.d/divine-path.sh to update system PATH"
    echo "After you log out and back in, it should be available as 'divine'"
fi
