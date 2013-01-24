. lib

divine compile --cesmi data/withltl.c data/withltl.ltl
test -f withltl.so && out=withltl.so
test -f withltl.dll && out=withltl.dll

divine info $out | grep neverclaim

run metrics $out --property=p_1
check statespace 24 32 0 8

run metrics $out --property=p_3
check statespace 38 66 24 0
run verify $out --property=p_3
check ltl_invalid

run metrics $out --property=p_4
check statespace 14 18 14 4
run verify $out --property=p_4
check ltl_invalid

divine compile --cesmi data/simple.c
test -f simple.so && out=simple.so
test -f simple.dll && out=simple.dll

divine info $out
run metrics $out --property=p_1
check statespace 24 32 0 8
