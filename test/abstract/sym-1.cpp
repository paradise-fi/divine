/* TAGS: sym min c++ */
/* VERIFY_OPTS: -o nofail:malloc --symbolic  */
#include <rst/sym.h>
#include <iostream>

using namespace lart;

int main() {
    auto **inputa = __abstract_sym_alloca( 64 );
    auto *input = __abstract_sym_load( inputa, 64 );
    std::cout << sym::toString( input ) << std::endl;

    auto *c0l = __abstract_sym_lift( 0, 64 );
    std::cout << sym::toString( c0l ) << std::endl;

    auto *ge0 = __abstract_sym_icmp_sge( input, c0l );
    std::cout << sym::toString( ge0 ) << std::endl;

    input = __abstract_sym_assume( input, ge0, true );
    std::cout << sym::toString( input ) << std::endl;

    auto *c42i = __abstract_sym_lift( 42, 32 );
    std::cout << sym::toString( c42i ) << std::endl;

    auto *c42l = __abstract_sym_zext( c42i, 64 );
    std::cout << sym::toString( c42l ) << std::endl;

    auto *add = __abstract_sym_add( input, c42l );
    std::cout << sym::toString( add ) << std::endl;

    auto **inputa2 = __abstract_sym_alloca( 64 );
    auto *input2 = __abstract_sym_load( inputa2, 64 );
    std::cout << sym::toString( input2 ) << std::endl;

    auto *icmp_eq = __abstract_sym_icmp_eq( input2, add );
    std::cout << sym::toString( icmp_eq ) << std::endl;
}
