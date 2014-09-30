. lib

test "$ALG_REACHABILITY" = "ON" || skip

all_small verify --reachability -w 1 --shared
all_small verify --reachability -w 2 --shared

