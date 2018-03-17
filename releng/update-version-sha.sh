#!/bin/sh
sha1sum="$1"
from="$2"
where="$3"
version="$4"
buildtype="$5"
empty="0000000000000000000000000000000000000000";

cd $from

interesting='.*(\.c$|\.cpp$|\.hpp$|\.h$|\.cc$|\.hh$|\.flags$)';
boring='\.orig$|~$';
if test -e releng/manifest; then
    manifest=`cat releng/manifest`;
else
    if ! test -d _darcs || ! darcs --version > /dev/null; then
        manifest=`find . -type f ! -path './_build*' ! -path './_darcs*'` # assume...
    else
        manifest=`darcs show files`
    fi
fi

relsha=`cat releng/checksum`

sha() {
    echo "$manifest" | egrep "^./$1/$interesting" | egrep -v "$boring" \
        | while read f; do cat $f | $sha1sum | cut -d' ' -f1; done \
        | sort | $sha1sum | cut -d' ' -f1
}

test -z "$old" && old=`cat $where.cached 2> /dev/null`
testsha=`echo | $sha1sum 2> /dev/null`
if test -n "$testsha" ; then
    test -z "$new" && \
        new="`sha divine` `sha dios`"
else
    old="na"
    new=$empty
fi

if test "$old" != "$new" || ! test -f $where; then
    src=`echo $new | cut -d' ' -f1`
    runtime=`echo $new | cut -d' ' -f2`
    echo "const char *DIVINE_VERSION = \"$version\";" > $where
    echo "const char *DIVINE_SOURCE_SHA = \"$src\";" >> $where
    echo "const char *DIVINE_RUNTIME_SHA = \"$runtime\";" >> $where
    echo "const char *DIVINE_BUILD_DATE = \"$(date -u "+%Y-%m-%d, %H:%M UTC")\";" >> $where
    echo "const char *DIVINE_BUILD_TYPE = \"$buildtype\";" >> $where
    echo "const char *DIVINE_RELEASE_SHA = \"$relsha\";" >> $where
    if test "$where" != /dev/null; then echo $new > $where.cached; fi
fi
