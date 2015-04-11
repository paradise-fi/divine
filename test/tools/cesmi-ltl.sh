. lib
. flavour

test "$GEN_CESMI" = "ON" || skip
test "$ALG_OWCTY" = "ON" || skip

divine compile --cesmi data/withltl.c data/withltl.ltl
divine info withltl$cesmiext | grep LTL

run verify withltl$cesmiext --property=p_3 $FLAVOUR
check ltl_invalid
run verify withltl$cesmiext --property=p_4 $FLAVOUR
check ltl_invalid
