#!/bin/sh
set -e

out="$1"
shift

files=`echo "$@" | sed -e 's,;, ,g'`
echo "namespace murphi {" > $out
for m in $files; do
    echo 'extern const char *'"`basename ${m} .cpp`"';' >> $out
done
echo '}' >> $out
