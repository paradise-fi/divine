. lib

test "$ALG_EXPLICIT" = ON && test "$GEN_EXPLICIT" = ON \
    && test "$ALG_REACHABILITY" = ON || skip

for VIS in "" "--shared"; do
    for COMP in $COMPRESSIONS; do
        all_small genexplicit_reach --compression=$COMP $VIS -w 1
        all_small genexplicit_reach --compression=$COMP $VIS -w 2
    done
done
