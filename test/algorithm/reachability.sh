. lib

test "$ALG_REACHABILITY" = ON || skip

for COMP in $COMPRESSIONS
do
    all_small reachability -w 1 --compression=$COMP
    all_small reachability -w 2 --compression=$COMP
done
