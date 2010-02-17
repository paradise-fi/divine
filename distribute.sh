if test -n "$1"; then
    name="$1"
else
    ver=$(grep "^#define DIVINE_VERSION" divine/version.cpp)
    ver1=$(echo $ver | sed -e 's,#define DIVINE_VERSION "\([^"]*\)",\1,')
    #version="$ver1$bra1$date"
    name="divine-$ver1"
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
markdown $name/README > $name/README.html
tar cvzf "$name.tar.gz" "$name"
rm -rf "$name"
grep -q '^#define DIVINE_RELEASE' divine/version.h || \
    echo "WARNING: You are building a distribution tarball of unreleased version."
