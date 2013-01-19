. lib

divine compile --cesmi data/withltl.c data/withltl.ltl
divine info withltl.so | grep neverclaim
run metrics withltl.so --property=p_1
check statespace 24 32 0 8
run metrics withltl.so --property=p_3
check statespace 33 74 8 14

divine compile --cesmi data/simple.c
divine info simple.so
run metrics simple.so --property=p_1
check statespace 24 32 0 8
