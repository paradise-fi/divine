. lib
. flavour

test "$ALG_NDFS" = ON || skip

all_small ndfs -w 1 $FLAVOUR
all_small ndfs -w 2 $FLAVOUR
