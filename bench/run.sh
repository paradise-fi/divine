#!/usr/bin/env bash

set -x
while [[ $1 ]]; do
    if [[ $OPT ]]; then
        eval "$OPT=\"$1\""
        unset OPT
    else
        OPT=$(echo $1 | sed 's/^--//')
    fi
    shift
done

function die {
    echo "$@" >&2
    exit 1
}

function prepareModels {
    from=$1
    to="$2/models"
    mkdir -p $to

    for i in $(ls $from); do
        if [[ $i = *.c ]] || [[ $i = *.cpp ]] || [[ $i = *.cc ]]; then
            CCS="$CCS $i"
        else
            cp $from/$i $to/
        fi
    done
    if [[ $CCS ]]; then
        OLD=$PWD
        cd $to
        mkdir libs
        cd libs
        $divine compile -l --lib -j 4
        cd ..
        for i in $CCS; do
            $divine compile -l --pre=libs $from/$i --cflags='-std=c++11'
        done
        cd $OLD
    fi
}

if ! [[ $divine ]]; then
    divine=$(which divine)
else
    divine=$(readlink -f $divine) # ensure path is absolute
fi
[[ $divine ]] || die "please specify path to divine in --divine or set it in PATH"

[[ $models ]] || models="./models"
[[ -d $models ]] || \
    die "please specify path to models directory (--models) or put models in ./models"
models=$(readlink -f $models)

[[ $results ]] || results="./results"
[[ $repeat ]] || repeat=1

mkdir -p $results
results=$(readlink -f $results)

vers=$($divine --version | grep "^Version: " | sed 's/Version: //')
build=$($divine --version | grep "^Build-Date: " | sed 's/[^:]*: \([^ ,]*\), \([^ ]*\).*/\1+\2/')

respath=$results/$build-$vers

if mkdir $respath; then
    prepareModels $models $respath
    $divine --version >> $respath/version
fi

cd $respath
mkdir -p report

echo "divine=$divine"
echo "respath=$respath"
echo "models=$models"
echo "opts=$opts"
echo "repeat=$repeat"
echo "altopts=$altopts"

for _ in $(eval echo {1..$repeat}); do
    timestamp=$(date +%s)
    for i in $(ls models); do
        if [[ -f models/$i ]]; then
            echo "running: $i"
            $divine verify $opts models/$i --report --report="text:report/$i.$timestamp.rep" 3>&1 1>&2 2>&3 | tee "report/$i.$timestamp.out"
        fi
    done
done
