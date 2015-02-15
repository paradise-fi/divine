. lib

test "$GEN_CESMI" = "ON" || skip
test "$ALG_METRICS" = "ON" || skip

divine compile --cesmi data/withltl.c data/withltl.ltl
divine info withltl$cesmiext | grep LTL

run metrics withltl$cesmiext --property=p_1
check statespace 24 32 0 8

run metrics withltl$cesmiext --property=p_3
if test "$OPT_LTL3BA" = ON; then check statespace 38 66 24 0; else check statespace 48 89 24 0; fi
run metrics withltl$cesmiext --property=p_4
check statespace 14 18 14 4

divine compile --cesmi data/simple.c
divine info simple$cesmiext
run metrics simple$cesmiext --property=p_1
check statespace 24 32 0 8
