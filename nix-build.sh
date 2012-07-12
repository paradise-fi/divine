#!/bin/sh
set -ex
rm -f result
chmod a+rX -R .
TMP=/tmp/export-`dd if=/dev/urandom bs=1 count=32 | base64 | sed -e 's,[/=+],x,g' | cut -c1-32`
ln -s `pwd` "$TMP"
trap "rm -f \"$TMP\"" EXIT
nix-build release.nix --arg divineSrc "\"$TMP/\"" -A "$@"
