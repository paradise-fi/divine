. lib

test "$ALG_OWCTY" = ON || skip

for COMP in $COMPRESSIONS
do
    all_small owcty -w 1 --compression=$COMP
    all_small owcty -w 2 --compression=$COMP
done
