#include <rst/common.h>
#include <sys/lart.h>
#include <cstdint>
#include <cstdarg>
#include <sys/divm.h>
#include <new>
#include <string>

using namespace std::literals;
using namespace lart::sym;

template< typename T, typename ... Args >
static T *__new( Args &&...args )
{
    void *ptr = __vm_obj_make( sizeof( T ) );
    new ( ptr ) T( std::forward< Args >( args )... );
    return static_cast< T * >( ptr );
}

#if 0
void *__sym_mk_op( int _op, int type, int bitwidth ... ) {

    Op op = Op( _op );
    Type t( Type::T( type ), bitwidth );
    va_list vl;
    va_start( vl, bitwidth );

    if ( op == Op::Variable ) {
        VarID varid = va_arg( vl, VarID );
        va_end( vl );
        return __new< Variable >( t, varid );
    }

    if ( op == Op::Constant ) {
        ValueU val;
        if ( type == Type::Int ) {
            if ( bitwidth <= 32 )
                val.i32 = va_arg( vl, int32_t );
            else if ( bitwidth <= 64 )
                val.i64 = va_arg( vl, int64_t );
            else
                UNREACHABLE_F( "Integer too long: %d bits", bitwidth );
        } else {
            switch ( bitwidth ) {
                case 32:
                    val.fp32 = va_arg( vl, float );
                    break;
                case 64:
                    val.fp64 = va_arg( vl, double );
                    break;
                    /*
                case 80:
                    val.fp80 = va_arg( vl, long double );
                    break;
                    */
                default:
                    UNREACHABLE_F( "Unknow float of size: %d bits", bitwidth );
            }
        }
        va_end( vl );
        return __new< Constant >( t, val.raw );
    }

    if ( isUnary( op ) ) {
        Formula *child = va_arg( vl, Formula * );
        va_end( vl );
        return __new< Unary >( op, t, child );
    }

    if ( isBinary( op ) ) {
        Formula *left = va_arg( vl, Formula * );
        Formula *right = va_arg( vl, Formula * );
        va_end( vl );
        return __new< Binary >( op, t, left, right );
    }

    UNREACHABLE_F( "Unknown op in __sym_mk_op: %d", _op );
}
#endif
