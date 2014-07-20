#!/usr/bin/env bash

# example: ./bench/run.sh --divine ../relWithDebInfo/tools/divine --altopts "--reachability --csdr, --workers={1..4}"

# set -x
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
    new=""

    for i in $(ls $from); do
        if [[ $i = *.c ]] || [[ $i = *.cpp ]] || [[ $i = *.cc ]]; then
            CCS="$CCS $i"
        elif ! [[ -f $to/$i ]]; then
            cp $from/$i $to/
            new="$new $i"
        fi
    done
    if [[ $CCS ]]; then
        OLD=$PWD
        cd $to
        if ! [[ -d $to/libs ]]; then
            mkdir libs
            cd libs
            $divine compile -l --lib -j 4
            cd ..
        fi
        for i in $CCS; do
            target=$(echo $i | sed 's/\(.c$\|.cc$\|.cpp$\)/.bc/')
            if ! [[ -f $target ]]; then
                flags=""
                if [[ $i = *.cpp ]] || [[ $i = *.cc ]]; then
                    flags="--cflags='-std=c++11'"
                fi
                $divine compile -l --pre=libs $from/$i "$flags"
                new="$new $target"
            fi
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
elif [[ $newmodels ]]; then
    model_list=$(prepareModels $models $respath)
fi

cd $respath
mkdir -p report

echo "divine=$divine"
echo "respath=$respath"
echo "models=$models"
echo "opts=$opts"
echo "repeat=$repeat"
echo "altopts=$altopts"

altgroups=0
old_IFS=$IFS
IFS=','
for _ in $altopts; do let altgroups++; done
IFS=$old_IFS;

function group {
    local old_IFS=$IFS
    IFS=','
    i=0
    for g in $altopts; do
        if [[ $i == $1 ]]; then
            eval echo $g
        fi
        let i++
    done
    IFS=$old_IFS;
}

function altsize {
    i=0
    for _ in $(group $1); do let i++; done
    echo $i;
}


for i in $(eval echo {0..$altgroups}); do
    altix[$i]=0
    altsi[$i]=$(altsize $i)
done

function optval {
    g=$(group $1)
    i=0
    for v in $g; do
        if [[ $i == $2 ]]; then
            echo $v
        fi
        let i++
    done
}

if ! [[ $model_list ]]; then
    model_list=$(ls models)
fi

echo "model_list=$model_list"

for _ in $(eval echo {1..$repeat}); do
    while true; do
        curalts=""
        i=0
        while [[ $i -lt $altgroups ]]; do
            val=$(optval $i ${altix[$i]})
            curalts="$curalts $val"
            let i++
        done

        timestamp=$(date +%s)
        for i in $model_list; do
            if [[ -f models/$i ]]; then
                echo "running: $i"
                if [[ $dryrun ]]; then
                    echo verify $opts $curalts models/$i
                else
                    $divine verify $opts $curalts models/$i \
                        --report --report="text:report/$i.$timestamp.rep" \
                        3>&1 1>&2 2>&3 | tee "report/$i.$timestamp.out"
                fi
            fi
        done

        i=0
        carry=1
        while [[ $carry -ne 0 && $i -lt $altgroups ]]; do
            let altix[$i]++
            if [[ ${altix[$i]} -eq ${altsi[$i]} ]]; then
                altix[$i]=0
            else
                carry=0
            fi
            let i++
        done
        [[ $carry -ne 0 ]] && break
    done
done
