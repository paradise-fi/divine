/* TAGS: sym min c++ */
/* VERIFY_OPTS: -o nofail:malloc --symbolic  */
#include <rst/sym.h>
#include <iostream>

int main() {
    auto **inputa = __abstract_sym_alloca( 64 );
    auto *input = __abstract_sym_load( inputa, 64 );
    auto *c0l = __abstract_sym_lift( 0, 64 );
    auto *ge0 = __abstract_sym_icmp_sge( input, c0l );
    input = __abstract_sym_assume( input, ge0, true );
    auto *c42i = __abstract_sym_lift( 42, 32 );
    auto *c42l = __abstract_sym_zext( c42i, 64 );
    auto *add = __abstract_sym_add( input, c42l );
    auto **inputa2 = __abstract_sym_alloca( 64 );
    auto *input2 = __abstract_sym_load( inputa2, 64 );
    auto *icmp_eq = __abstract_sym_icmp_eq( input2, add );
    auto *lt0 = __abstract_sym_icmp_slt( input2, c0l );
    input2 = __abstract_sym_assume( input2, lt0, true );
    auto *ieq = __abstract_sym_icmp_eq( input, input2 );
    input = __abstract_sym_assume( input, ieq , true);
    input2 = __abstract_sym_assume( input2, ieq, true );
}
