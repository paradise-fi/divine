. lib

test "$ALG_EXPLICIT" = ON && test "$GEN_EXPLICIT" = ON \
    && test "$ALG_REACHABILITY" = ON || skip

. $TESTS/algorithm/genexplicit.sh # if it fails we won't get any meaningfull error here

for COMP in $COMPRESSIONS; do
    all_small genexplicit_rech -w 1
    all_small genexplicit_rech -w 2
done
