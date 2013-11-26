. lib

test "$O_LLVM" = "ON" || skip
llvm_precompile

for I in llvm_examples/*.c llvm_examples/*.cpp; do

    if echo "$I" | grep '\.c\(pp\|c\)$'; then
        COMPILATION=$(cat $I | grep -A5 '\(Verification\|Solution\)' | fgrep 'divine compile' | head -n1 | fgrep -- '--cflags' | sed 's/^.*\*//') || true
        if echo "$COMPILATION" | fgrep -- '-std='; then
            STD=$(echo $COMPILATION | sed "s/^.*-std=\([^'\" ]*\).*$/\1/")
        fi
    fi
    unset CFLAGS
    if ! [ -z $STD ]; then
        CFLAGS="--cflags='-std=$STD'"
    fi
    echo "CFLAGS: $CFLAGS"
    divine compile --llvm $I $CFLAGS --precompiled=. >& progress
    BC=$(echo $I | sed 's,^.*/,,' | sed 's/\.c\(pp\|c\)\{0,1\}$/.bc/')
    echo "BC: $BC"
    divine info $BC 2>> progress | capture
done
