#!/bin/sh
set -e
rm -f divine-snapshot.tar.gz

echo preparing divine-snapshot
rm -rf divine-snapshot
mkdir divine-snapshot
# show files includes files that were darcs add-ed but not recorded
darcs show files | while read f; do
    test -f "$f" || continue
    mkdir -p divine-snapshot/`dirname "$f"`
    ln "$f" "divine-snapshot/$f"
done
set -x
tar czf divine-snapshot.tar.gz divine-snapshot
rm -rf divine-snapshot
nix-build -I nixpkgs=/home/mornfall/dev/nix/nixpkgs.clean release.nix --arg divineSrc "`pwd`/divine-snapshot.tar.gz" -A "$@"
