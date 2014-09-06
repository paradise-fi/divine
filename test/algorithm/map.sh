. lib

test "$ALG_MAP" = ON || skip

for COMP in $COMPRESSIONS
do
    all_small map -w 1 --compression=$COMP
    all_small map -w 2 --compression=$COMP
done
