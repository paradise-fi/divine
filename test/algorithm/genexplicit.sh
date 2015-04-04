. lib

test "$ALG_EXPLICIT" = ON || skip

for COMP in $COMPRESSIONS; do
    all_small genexplicit -w 1
    all_small genexplicit -w 2
done
