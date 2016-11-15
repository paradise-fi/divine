#!/bin/sh

email()
{
    echo "From: xrockai@fi.muni.cz"
    echo "To: $address"
    echo "Subject: DIVINE daily $(date +%Y-%m-%d): $patches"
    echo
    cat report.txt
}

finished()
{
    perl -i.orig -e '$rep = `cat report.txt`; while (<>) { s/\@report\@/$rep/; print $_; }' \
         doc/website/status.md
    make debug-website
    mv doc/website/status.md.orig doc/website/status.md

    email | sendmail $address
    exit $1
}

changes()
{
    echo "# Latest Changes"
    echo
    perl releng/darcs2md.pl < changes.txt
}

cantbuild()
{
    make debug 2>&1 || true # dump the failing stuff into the report
    echo
    echo "Build failed. Stopping here."
}

failed()
{
    if test -e $list; then
        echo "Some tests failed:"
        echo
        grep -v passed $list | perl -pe 's,([a-z]+):(.*) (.*),    $3: [$1] $2,'
        echo
        echo "Remaining $(grep -c passed $list) tests passed. Stopping here."
    else
        echo "Make check failed:"
        echo
        make debug-check 2>&1 || true
    fi
}

warnings()
{
    echo "Some tests skipped or passed with warnings:"
    echo
    egrep 'warnings|skipped' $list | perl -pe 's,([a-z]+):(.*) (.*),    $3: [$1] $2,'
    echo
}

passed()
{
    echo "$(grep -c passed $list) tests passed. The above patches are now part of"
    echo "<http://divine.fi.muni.cz/current>."
}

set -ve
umask 0022

address=$1
upstream=$2
downstream=$3

# anything new?
darcs pull --dry -a --match 'not name XXX' --no-deps -s --no-set-default $upstream 2>&1 | \
    egrep -v '^Would|^Making|^No remote' | tee changes.txt
if ! grep -q ^patch changes.txt; then exit 0; fi

darcs pull --quiet -a --match 'not name XXX' --no-deps

patchcount=$(grep -c ^patch changes.txt)
if test $patchcount -eq 1; then patches="1 new patch"; else patches="$patchcount new patches"; fi

objdir="$(make show var=OBJ)/debug"
list="$objdir/test/results/list"

changes > report.txt

if ! make debug; then # does it build at all?
    cantbuild >> report.txt
    finished 1
fi

(echo "# Test results"; echo) >> report.txt

rm -f $list
if ! make debug-check || egrep -q 'failed|timeout|unknown' $list; then
    failed >> report.txt
    finished 1
fi

if egrep -q 'warnings|skipped' $list; then warnings >> report.txt; fi

# everything looks good, push to current
passed >> report.txt
darcs push --quiet -a --no-set-default $downstream
finished 0
