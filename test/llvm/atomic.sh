. lib

llvm_verify valid <<EOF
#include <assert.h>
void main() {
    int val = 0, expect = 0, new = 1;
    assert( __sync_val_compare_and_swap( &val, expect, new ) == 0 );
    assert( val == 1 );
    assert( expect == 0 );
    new = 2;
    assert( __sync_val_compare_and_swap( &val, expect, new ) == 1 );
    assert( val == 1 );
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
void main() {
    int val = 1;
    assert( __sync_fetch_and_add( &val, 1 ) == 1 );
    assert( val == 2 );
    assert( __sync_fetch_and_sub( &val, 1 ) == 2 );
    assert( val == 1 );
    assert( __sync_fetch_and_or( &val, 2 ) == 1 );
    assert( val == 3 );
    assert( __sync_fetch_and_and( &val, 2 ) == 3 );
    assert( val == 2 );
}
EOF

llvm_verify valid <<EOF
#include <assert.h>
void main() {
    int val = 1;
    assert( __sync_add_and_fetch( &val, 1 ) == 2 );
    assert( val == 2 );
    assert( __sync_sub_and_fetch( &val, 1 ) == 1 );
    assert( val == 1 );
    assert( __sync_or_and_fetch( &val, 2 ) == 3 );
    assert( val == 3 );
    assert( __sync_and_and_fetch( &val, 2 ) == 2 );
    assert( val == 2 );
}
EOF
