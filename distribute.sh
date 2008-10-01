if test -n "$1"; then
    name="$1"
else
    ver=$(grep "^#define DIVINE_VERSION_ID" divine/version.h)
    bra=$(grep "^#define DIVINE_BRANCH " divine/version.h)
    date=$(date -u "+%Y%m%d%H%M")
    ver1=$(echo $ver | sed -e 's,#define DIVINE_VERSION_ID "\([^"]*\)",\1,')
    bra1=$(echo $bra | sed -e 's,#define DIVINE_BRANCH "\([^"]*\)",\1,')
    test -n "$bra1" && bra1="$bra1+"
    #version="$ver1$bra1$date"
    if grep -q '^#define DIVINE_RELEASE' divine/version.h; then
        name="divine-mc-$ver1"
    else
        ver1="$ver1+"
        name="divine-mc-$ver1$bra1$date"
    fi
fi
if test -e "$name"; then
    echo "$name already exists!"
    exit 2;
fi
if test -e "$name.tar.gz"; then
    echo "$name.tar.gz already exists!"
    exit 3;
fi

for f in `darcs query manifest | grep -v _attic`; do
    mkdir -p "$name/`dirname $f`"
    cp "$f" "$name/$f"
done
darcs query manifest | grep -v _attic > "$name/manifest"
chmod +x "$name/configure"
markdown $1/README > $name/README.html
tar cvzf "$name.tar.gz" "$name"
rm -rf "$name"
grep -q '^#define DIVINE_RELEASE' divine/version.h || \
    echo "WARNING: You are building a distribution tarball of unreleased version."
