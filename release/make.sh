#!/bin/sh

set -xe


test -f divine/utility/version.cpp

patchlevel=$(cat release/patchlevel)
patchlevel=$(($patchlevel + 1))
echo $patchlevel > release/patchlevel
version=$(cat release/version).$patchlevel
bash ./divine/update-version.sh sha1sum . /dev/null 0 > release/checksum

result=`bash ./nix/build.sh tarball` 
tb=$(cd $result/tarballs && ls)
version=$(echo $tb | sed -e s,^divine-,, -e s,\.tar\.gz$,,)
cp $result/tarballs/$tb .
echo a | darcs rec --ask-deps release -a -m "${version}"
