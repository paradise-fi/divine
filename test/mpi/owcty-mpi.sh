. lib

test "$ALG_OWCTY" = "ON" || skip

all_small mpi_verify --owcty -w 1
all_small mpi_verify --owcty -w 2

