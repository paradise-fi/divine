. lib

test "$ALG_REACHABILITY" = ON || skip
test "$GEN_DVE" = "OFF" && skip

for COMP in $COMPRESSIONS
do
    run reachability --property=deadlock --reduce=por data/por.dve --compression=$COMP
    check reachability_deadlock
    dve_small reachability --reduce=por --compression=$COMP
done
