. lib/testcase

cat > testcase.c <<EOF
#include <math.h>
#include <assert.h>

int main() {
    int x = exp2( 2 );
    assert( x == 4 );
}
EOF

divine cc testcase.c
if divine verify testcase.bc; then
    echo "should not work without -lm"
    exit 1
fi

divine cc testcase.c -lm
divine verify testcase.bc

divine cc testcase.c -l m
divine verify testcase.bc

divine verify -C,-lm testcase.c
divine verify -C,-l,m testcase.c
