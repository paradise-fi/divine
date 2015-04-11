. lib

test "$ALG_EXPLICIT" = ON || skip

for VIS in "" "--shared"; do
    for COMP in $COMPRESSIONS; do
        all_small genexplicit --compression=$COMP $VIS -w 1
        all_small genexplicit --compression=$COMP $VIS -w 2
    done
done
