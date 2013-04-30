. lib

test "$O_DVE" = "OFF" && skip
run reachability data/assert2.dve --hash-compaction
check reachability_valid
