. lib
. flavour

test "$ALG_REACHABILITY" = ON || skip
test "$GEN_DVE" = "OFF" && skip

run reachability --property=deadlock --reduce=por data/por.dve $FLAVOUR
check reachability_deadlock
dve_small reachability --reduce=por $FLAVOUR
