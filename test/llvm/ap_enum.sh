. lib
. flavour vanilla

# this test tests that atomic proposition names get demangled
# if they dont't divine will fail

test "$ALG_OWCTY" = ON -o "$ALG_NDFS" = ON || skip

llvm_verify ltl_valid p0 <<EOF
#include <divine.h>
enum APs { _test_ap_bla_bla_123_ = 0x42 };
LTL(p0, GF _test_ap_bla_bla_123_ );

int main() {
    while (1) { AP( _test_ap_bla_bla_123_ ); }
}
EOF
