. lib

test "$O_LLVM" = "ON" || skip
llvm_precompile

for I in llvm_examples/*.c llvm_examples/*.cpp; do

    divine compile --llvm $I $CFLAGS --precompiled=. >& progress
    BC=$(echo $I | sed 's,^.*/,,' | sed 's/\.c\(pp\|c\)\{0,1\}$/.bc/')
    echo "BC: $BC"
    divine info $BC 2>> progress | capture
done
