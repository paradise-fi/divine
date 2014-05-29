#!/bin/sh

set -e

file=$1
base=$2
shift; shift

flatten() {
    sed -e "s|\\.|_|g" -e "s|/|_|g" -e "s|-|_|g" -e "s|+|_|g" -e "s|~|_|g"
}

if test -z "$1"; then
    i=0
    grep ^@ "$file" | while read l; do
        i=$(($i + 1))
        echo -n "$(echo $base | flatten).$i.cpp;"
    done
else
    out="$1"
    outn=$(echo $out | sed -re "s,.*\.([0-9]+)\.cpp,\1,")
    (echo "#define TCC_INSTANCE $outn"; cat $file) > $out
    i=1
    while grep -q ^@ $out && test $i -lt $outn; do
        sed -i -e "0,/^@.*/s///" $out
        i=$(($i + 1))
    done
    sed -i -e "0,/^@/s///" $out
    while grep -q ^@ $out; do
        sed -i -e "0,/^@.*/s///" $out
    done
fi
