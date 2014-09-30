. lib

test "$ALG_REACHABILITY" = "ON" || skip

dve_small mpi_verify --reachability -w 1

run mpi verify --reachability data/shuffle.dve
check reachability_valid
check statespace 181450 483850 0 0

