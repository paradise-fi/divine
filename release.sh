#!/bin/bash

set -e

test "$1" = "--dry" && {
    dry=1
    shift
}

really=1

die() {
    test -z "$really" && echo -n "W: "
    echo -e "$@" 1>&2
    test -n "$really" && exit 1 || true
}

test -f divine/utility/version.cpp || die "Doesn't look like a divine source tree to me..."

if test -n "$1"; then
    name="$1"
else
    version_=$(grep "^#define DIVINE_VERSION" divine/utility/version.cpp)
    version=$(echo $version_ | sed -e 's,#define DIVINE_VERSION "\([^"]*\)",\1,')
    name="divine-$version"
fi

test -e "$name" && die "$name already exists!"
test -e "$name.tar.gz" && die "$name.tar.gz already exists!"

## CHECK ###############
test -n "$dry" && really=

echo checking repository state...

test -d _darcs || die "Doesn't look like a darcs repository..."

darcs wh >& /dev/null && \
    die "Seems like you have local changes. Please don't."

if darcs chan --from-tag $version > /dev/null 2>&1; then
    test "`darcs chan --from-tag $version --count 2> /dev/null`" = 1 || {
        darcs chan --from-tag $version
        die "You have patches after current release ($version) tag.\n" \
            "Sounds like a bad idea to me. Maybe try with -i?"
    }
else
    die "There doesn't seem to be a release ($version) tag."
fi


head -n1 NEWS | grep -q "DiVinE $version" || \
    die "Seems that first entry in NEWS is not for $version..."

## BUILD ###############
echo populating the tarball...

IFS="
"
for f in `darcs query manifest`; do
    mkdir -p "$name/`dirname $f`"
    cp "$f" "$name/$f"
done

echo updating autogenerated bits in the tarball...

darcs query manifest > "$name/manifest"
markdown $name/README > $name/README.html
chmod +x "$name/configure"

echo creating $name.tar.gz...
tar czf "$name.tar.gz" "$name"
rm -rf "$name"

## VALIDATE ###############
echo validating...
tar xzf "$name.tar.gz"
cd $name
./configure
make
make check

./_build/tools/divine --version
./_build/tools/divine --version | egrep "^DiVinE version $version( |$)" # no hashes in there
cd ..
rm -rf $name

test -z "$really" && {
    echo looks good, but not uploading -- you specified --dry
    exit 0
}

echo uploading and updating NEWS...
scp $name.tar.gz anna.fi.muni.cz:/srv/www/divine-pages/
scp NEWS anna.fi.muni.cz:/srv/www/divine-pages/
ssh anna.fi.muni.cz chmod 644 /srv/www/divine-pages/{$name.tar.gz,NEWS}

echo "now it's up to you... announce, add download links, open champagne"
