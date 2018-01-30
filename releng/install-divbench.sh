set -ex

from=$1; shift
to=$1  ; shift
name=$1; shift
skip=$1
date=$(date +%Y-%m-%d.%H%M)

bin=$name.$date
test -n "$skip" && touch $to/../sched/$bin
cp $from $to/$bin
lib=$to/lib.$bin
libs=$(ldd $from | fgrep -v vdso.so | cut -d= -f2 | cut -d' ' -f2)

mkdir $lib
for l in $libs; do
    cp $l $lib
    patchelf --set-rpath $lib $lib/$(basename $l)
done

patchelf --set-rpath $lib $to/$bin
echo installed as $to/$bin
