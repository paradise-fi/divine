#!/bin/sh
sha1sum="$1"
from="$2"
where="$3"

cd $from

if test -e manifest; then
    manifest=`cat manifest`;
else
    if ! test -d _darcs || ! darcs --version > /dev/null; then
        old="0000000000000000000000000000000000000000";
        new=$old;
    else
        manifest=`darcs query manifest | grep -v _attic`;
    fi
fi

test -z "$old" && old=`cat $where`
test -z "$new" && new=`echo "$manifest" | grep -v examples | xargs $sha1sum \
    | cut -d' ' -f1 | $sha1sum | cut -d' ' -f1`

echo $new > "$where"

if test "$old" != "$new"; then
    echo "#define DIVINE_CONTEXT_SHA \"$new\"" > $where.h
    echo "#define DIVINE_BUILD_DATE \"$(date -u "+%Y-%m-%d, %H:%M UTC")\"" >> $where.h
fi
