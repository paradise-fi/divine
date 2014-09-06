. lib

test "$ALG_OWCTY" = ON || skip

all_small owcty -w 1 --shared
all_small owcty -w 2 --shared
