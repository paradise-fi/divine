. lib

test "$ALG_OWCTY" = ON -o "$ALG_NDFS" = ON || skip

llvm_verify ltl_valid progress <<EOF
#include <divine.h>
enum APs { a };
LTL(progress, GF a);

void main() {
    while (1) { AP( a ); }
}
EOF

llvm_verify ltl_invalid progress <<EOF
#include <divine.h>
enum APs { a };
LTL(progress, GFa);

void main() {
    AP( a );
    while (1);
}
EOF
