. lib

test "$ALG_CSDR" = ON || skip

for COMP in $COMPRESSIONS
do
    all_small csdr -w 1 --compression=$COMP
    all_small csdr -w 2 --compression=$COMP
done
