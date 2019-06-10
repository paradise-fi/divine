// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <rst/unit.hpp>

#include <rst/common.hpp>

#include <cstdint>

namespace {
    using Unit = __dios::rst::abstract::Unit;

    template< typename T, typename ... Args >
    _LART_INLINE T* __new( Args &&...args ) noexcept {
        void *ptr = __vm_obj_make( sizeof( T ), _VM_PT_Heap );
        new ( ptr ) T( std::forward< Args >( args )... );
        return static_cast< T* >( ptr );
    }

    template< typename T >
    T unit() noexcept {
        __lart_stash( Unit::lift_any() );
        return __dios::rst::abstract::taint< T >();
    }
} // anonymous namespace

extern "C" {
    _LART_SCALAR uint8_t __unit_val_i8() { return unit< uint8_t >(); }
    _LART_SCALAR uint16_t __unit_val_i16() { return unit< uint16_t >(); }
    _LART_SCALAR uint32_t __unit_val_i32() { return unit< uint32_t >(); }
    _LART_SCALAR uint64_t __unit_val_i64() { return unit< uint64_t >(); }
}

namespace __dios::rst::abstract {

    #define __op( name, ... ) \
        _LART_INTERFACE Unit * Unit::name(__VA_ARGS__) noexcept { \
            return __new< Unit >(); \
        }

    #define __un( name ) __op( name, Unit * )

    #define __bin( name ) __op( name, Unit *, Unit * )

    #define __cast( name ) __op( name, Unit *, Bitwidth )

    /* abstraction operations */
    __op( lift_one, uint8_t )
    __op( lift_one, uint16_t )
    __op( lift_one, uint32_t )
    __op( lift_one, uint64_t )

    __op( lift_one, float )
    __op( lift_one, double )

    __op( lift_any )

    _LART_INTERFACE
    Tristate Unit::to_tristate( Unit * ) noexcept
    {
        return { Tristate::Unknown };
    }

    __op( assume, Unit *, Unit *, bool );

    /* arithmetic operations */
    __bin( op_add )
    __bin( op_fadd )
    __bin( op_sub )
    __bin( op_fsub )
    __bin( op_mul )
    __bin( op_fmul )
    __bin( op_udiv )
    __bin( op_sdiv )
    __bin( op_fdiv )
    __bin( op_urem )
    __bin( op_srem )
    __bin( op_frem )

    /* logic operations */
    __un( op_fneg )

    /* bitwise operations */
    __bin( op_shl )
    __bin( op_lshr )
    __bin( op_ashr )
    __bin( op_and )
    __bin( op_or )
    __bin( op_xor )

    /* comparison operations */
    __bin( op_ffalse );
    __bin( op_foeq );
    __bin( op_fogt );
    __bin( op_foge );
    __bin( op_folt );
    __bin( op_fole );
    __bin( op_fone );
    __bin( op_ford );
    __bin( op_funo );
    __bin( op_fueq );
    __bin( op_fugt );
    __bin( op_fuge );
    __bin( op_fult );
    __bin( op_fule );
    __bin( op_fune );
    __bin( op_ftrue );
    __bin( op_eq );
    __bin( op_ne );
    __bin( op_ugt );
    __bin( op_uge );
    __bin( op_ult );
    __bin( op_ule );
    __bin( op_sgt );
    __bin( op_sge );
    __bin( op_slt );
    __bin( op_sle );

    /* cast operations */
    __cast( op_fpext );
    __cast( op_fptosi );
    __cast( op_fptoui );
    __cast( op_fptrunc );
    __cast( op_inttoptr );
    __cast( op_ptrtoint );
    __cast( op_sext );
    __cast( op_sitofp );
    __cast( op_trunc );
    __cast( op_uitofp );
    __cast( op_zext );

    #undef __op
    #undef __un
    #undef __bin

} // namespace __dios::rst::abstract
