#!/bin/sh
sha1sum="$1"
from="$2"
where="$3"
empty="0000000000000000000000000000000000000000";

cd $from

interesting='\.c$|\.cpp$|\.h$|\.cc$|\.hh$';
if test -e manifest; then
    manifest=`cat manifest | egrep "$interesting"`;
else
    if ! test -d _darcs || ! darcs --version > /dev/null; then
        old="na"
        new=$empty
    else
        manifest=`darcs query manifest | grep -v _attic | egrep "$interesting"`
    fi
fi

test -z "$old" && old=`cat $where`
if which $sha1sum > /dev/null; then
    test -z "$new" && new=`echo "$manifest" | grep -v examples | xargs $sha1sum \
        | cut -d' ' -f1 | $sha1sum | cut -d' ' -f1`
else
    old="na"
    new=$empty
fi

echo $new > "$where"

if test "$old" != "$new"; then
    echo "#define DIVINE_CONTEXT_SHA \"$new\"" > $where.h
    echo "#define DIVINE_BUILD_DATE \"$(date -u "+%Y-%m-%d, %H:%M UTC")\"" >> $where.h
fi
