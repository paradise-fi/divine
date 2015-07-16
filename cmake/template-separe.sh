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
        $(which echo) -n "$(echo $base | flatten).$i.cpp;"
    done
else
    out="$1"
    esed="sed -r"
    $esed 2>/dev/null || esed="sed -E"
    outn=$(echo $out | $esed -e "s,.*\.([0-9]+)\.cpp,\1,")
    echo "#define TCC_INSTANCE $outn" > $out
    lines=$(wc -l < $file)

    i=0
    line=1

    while test $line -le $lines; do
        l=$(sed -e "${line}p;d" < $file)

        if echo "$l" | grep -q ^@; then
            i=$(($i + 1))
            if test $i -eq $outn; then echo "$l" | sed -e s,^@,, >> $out; fi
        else
            echo "$l" >> $out
        fi
        line=$(($line + 1))
    done
fi
