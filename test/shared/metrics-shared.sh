. lib

test "$ALG_METRICS" = ON || skip

all_small metrics -w 1 --shared
all_small metrics -w 2 --shared

