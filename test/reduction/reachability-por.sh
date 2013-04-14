. lib

test "$O_DVE" = "OFF" && skip
run reachability --reduce=por data/por.dve
check reachability_deadlock
