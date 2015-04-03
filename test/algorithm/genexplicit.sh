. lib

test "$ALG_EXPLICIT" = ON && test "$GEN_EXPLICIT" = ON || skip

for COMP in $COMPRESSIONS; do
    all_small genexplicit -w 1
    all_small genexplicit -w 2
done

for COMP in $COMPRESSIONS; do
    all_small genexplicit_rech -w 1
    all_small genexplicit_rech -w 2
done
