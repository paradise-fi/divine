. lib

divine compile --cesmi data/withltl.c data/withltl.ltl
divine info withltl.so | grep neverclaim

run metrics withltl.so --property=p_1
check statespace 24 32 0 8

run metrics withltl.so --property=p_3
check statespace 38 66 24 0
run verify withltl.so --property=p_3
check ltl_invalid

run metrics withltl.so --property=p_4
check statespace 14 18 14 4
run verify withltl.so --property=p_4
check ltl_invalid

divine compile --cesmi data/simple.c
divine info simple.so
run metrics simple.so --property=p_1
check statespace 24 32 0 8
