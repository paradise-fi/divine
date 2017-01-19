#!/bin/sh

email()
{
    echo "From: xrockai@fi.muni.cz"
    echo "To: $address"
    echo "Subject: DIVINE daily $today: $patches"
    echo
    cat report.txt
}

finished()
{
    test -e doc/website/report.txt || cp report.txt doc/website/
    touch doc/website/template.html # force a rebuild
    make ${buildtype}-website

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
    make $buildtype 2>&1 || true # dump the failing stuff into the report
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
        make ${buildtype}-check 2>&1 || true
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
today=$(date +%Y-%m-%d)

# anything new?
darcs pull --dry -a --match 'not name XXX' --no-deps -s --no-set-default $upstream 2>&1 | \
    egrep -v '^Would|^Making|^No remote' | tee changes.txt
if ! grep -q ^patch changes.txt; then exit 0; fi

rm -f report.txt doc/website/report.txt
darcs pull --quiet -a --match 'not name XXX' --no-deps

patchcount=$(grep -c ^patch changes.txt)
if test $patchcount -eq 1; then patches="1 new patch"; else patches="$patchcount new patches"; fi

buildtype=semidbg
objdir="$(make show var=OBJ)/$buildtype"
list="$objdir/test/results/list"

changes > report.txt

if ! make $buildtype; then # does it build at all?
    cantbuild >> report.txt
    finished 1
fi

(echo "# Test Results"; echo) >> report.txt

rm -f $list
if ! make ${buildtype}-check JOBS=6 || egrep -q 'failed|timeout|unknown' $list; then
    failed >> report.txt
    finished 1
fi

if egrep -q 'warnings|skipped' $list; then warnings >> report.txt; fi

# everything looks good, push to current
passed >> report.txt
darcs push --quiet -a --no-set-default $downstream
finished 0
