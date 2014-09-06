. lib

test "$ALG_NDFS" = ON || skip

for COMP in $COMPRESSIONS
do
    all_small ndfs -w 1 --compression=$COMP
    all_small ndfs -w 2 --compression=$COMP
done
