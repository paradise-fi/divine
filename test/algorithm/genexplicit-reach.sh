. lib
. flavour

test "$ALG_EXPLICIT" = ON && test "$GEN_EXPLICIT" = ON \
    && test "$ALG_REACHABILITY" = ON || skip

all_small genexplicit_reach -w 1 $FLAVOUR
all_small genexplicit_reach -w 2 $FLAVOUR
