. lib

test "$GEN_DVE" = "OFF" && skip
for COMP in $COMPRESSIONS
do
    run reachability --reduce=por data/por.dve --compression=$COMP
    check reachability_deadlock
    dve_small reachability --reduce=por --compression=$COMP
done
