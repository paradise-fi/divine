. lib
. flavour vanilla 'part+*'

test "$ALG_REACHABILITY" = "ON" || skip

dve_small mpi_verify --reachability -w 1 $FLAVOUR

run mpi verify --reachability data/shuffle.dve $FLAVOUR
check reachability_valid
check statespace 181450 483850 0 0

