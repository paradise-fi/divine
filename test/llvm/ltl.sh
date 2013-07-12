. lib

llvm_verify ltl_valid progress <<EOF
#include <divine.h>
enum AP { a };
LTL(progress, GF a);

void main() {
    while (1) { ap( a ); }
}
EOF

llvm_verify ltl_invalid progress <<EOF
#include <divine.h>
enum AP { a };
LTL(progress, GFa);

void main() {
    ap( a );
    while (1);
}
EOF
