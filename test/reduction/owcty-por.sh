. lib

test "$ALG_OWCTY" = ON || skip

for COMP in $COMPRESSIONS
do
    dve_small owcty --reduce=por --compression=$COMP
done
# coin_small owcty --por
