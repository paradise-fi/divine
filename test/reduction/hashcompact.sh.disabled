. lib
. flavour vanilla '*+none'

test "$STORE_HC" = "OFF" && skip
test "$GEN_DVE" = "OFF" && skip
run reachability --property=deadlock data/assert2.dve --hash-compaction $FLAVOUR
check reachability_valid
