. lib

test "$ALG_CSDR" = ON || skip

for COMP in $COMPRESSIONS
do
    llvm_small csdr -w 1 --compression=$COMP
    llvm_small csdr -w 2 --compression=$COMP
done
