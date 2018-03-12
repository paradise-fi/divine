/* TAGS: sym min c++ */
#include <rst/formula.h>
#include <string.h>
#include <assert.h>
#include <cstdio>
#include <cstdlib>

#ifdef __divine__
#include <dios.h>

__attribute__((constructor)) static void disbaleMallocFail() {
    __dios_configure_fault( _DiOS_SF_Malloc, _DiOS_FC_NoFail );
}
#endif

using namespace lart::sym;

void assert_str_eq( const std::string &a, const std::string &b ) {
    if ( a == b )
        return;
    std::printf( "assertion failed: %s != %s\n", a.c_str(), b.c_str() );
    std::abort();
}

int main() {
    Formula at1, at2, at3, gt, lt, a;
    at1.var = Variable( Type( Type::Int, 32 ), 1 );
    assert_str_eq( toString( at1 ), "[v 1:i32]" );

    at2.var = Variable( Type( Type::Int, 32 ), 2 );
    assert_str_eq( toString( at2 ), "[v 2:i32]" );

    at3.con = Constant( Type( Type::Int, 32 ), 3 );
    assert_str_eq( toString( at3 ), "[c 3:i32]" );

    gt.binary = Binary( Op::UGT, Type( Type::Int, 1 ), &at1, &at2 );
    assert_str_eq( toString( gt ), ">u([v 1:i32], [v 2:i32]) : i1" );

    lt.binary = Binary( Op::ULT, Type( Type::Int, 1 ), &at2, &at3 );
    assert_str_eq( toString( lt ), "<u([v 2:i32], [c 3:i32]) : i1" );

    a.binary = Binary( Op::And, Type( Type::Int, 1 ), &gt, &lt );

    assert_str_eq( toString( a ), "&(>u([v 1:i32], [v 2:i32]) : i1, <u([v 2:i32], [c 3:i32]) : i1) : i1" );
}
