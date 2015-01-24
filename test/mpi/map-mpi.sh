. lib

test "$ALG_MAP" = "ON" || skip

dve_small mpi_verify --map -w 1
dve_small mpi_verify --map -w 2
