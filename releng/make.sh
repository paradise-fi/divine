#!/bin/sh

set -xe
test -f divine/ui/version.cpp

patchlevel=$(cat releng/patchlevel)
patchlevel=$(($patchlevel + 1))
echo $patchlevel > releng/patchlevel
version=$(cat releng/version).$patchlevel
bash ./releng/update-version-sha.sh sha1sum . /dev/null 0 > releng/checksum

if darcs wh releng/checksum > /dev/null; then
    darcs rec -a --author "Release Bot <divine@fi.muni.cz>" \
              -m "releng: Update for $version" releng
    darcs tag -m $version
    darcs dist -d divine-${version}
elif test $? = 1; then
    echo "no changes, not making a release" 1>&2
    exit 1
else
    exit 2
fi
