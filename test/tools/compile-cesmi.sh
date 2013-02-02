. lib

divine compile --cesmi data/withltl.c data/withltl.ltl
divine info withltl$cesmiext | grep neverclaim

run metrics withltl$cesmiext --property=p_1
check statespace 24 32 0 8

run metrics withltl$cesmiext --property=p_3
check statespace 38 66 24 0
run verify withltl$cesmiext --property=p_3
check ltl_invalid

run metrics withltl$cesmiext --property=p_4
check statespace 14 18 14 4
run verify withltl$cesmiext --property=p_4
check ltl_invalid

divine compile --cesmi data/simple.c
divine info simple$cesmiext
run metrics simple$cesmiext --property=p_1
check statespace 24 32 0 8
