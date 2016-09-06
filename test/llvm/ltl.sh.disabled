. lib
. flavour vanilla

test "$ALG_OWCTY" = ON -o "$ALG_NDFS" = ON || skip

llvm_verify ltl_valid progress <<EOF
#include <divine.h>
enum APs { a };
LTL(progress, GF a);

int main() {
    while (1) { AP( a ); }
    return 0;
}
EOF

llvm_verify ltl_invalid progress <<EOF
#include <divine.h>
enum APs { a };
LTL(progress, GFa);

int main() {
    AP( a );
    while (1);
    return 0;
}
EOF

llvm_verify_cpp ltl_valid progress <<EOF
#include <divine.h>
enum APs { a };
LTL(progress, GF a);

int main() {
    while (1) { AP( a ); }
    return 0;
}
EOF
