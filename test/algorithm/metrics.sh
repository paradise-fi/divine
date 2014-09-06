. lib

test "$ALG_METRICS" = ON || skip

for COMP in $COMPRESSIONS
do
    all_small metrics -w 1 --compression=$COMP
    all_small metrics -w 2 --compression=$COMP
done
