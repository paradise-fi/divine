. lib
. flavour vanilla

llvm_verify valid assert,user <<EOF
#include <stdlib.h>

int main() {
    exit( 0 );
    assert( 0 );
    return 0;
}
EOF

llvm_verify invalid problem Exit user <<EOF
#include <stdlib.h>

int main() {
    exit( 1 );
    return 0;
}
EOF

llvm_verify invalid assert testcase.c:4 assert <<EOF
#include <stdlib.h>
#include <assert.h>

void diediedie() { assert( 0 ); }
int main() {
    atexit( diediedie );
    exit( 0 );
    return 0;
}
EOF

llvm_verify valid <<EOF
#include <stdlib.h>

void ok() {}

int main() {
    for ( int i = 0; i < 33; ++i )
        atexit( ok );
    exit( 0 );
    return 0;
}
EOF
