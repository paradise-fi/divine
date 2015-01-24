. lib

test "$ALG_METRICS" = "ON" || skip

all_small mpi_metrics -w 1
all_small mpi_metrics -w 2

