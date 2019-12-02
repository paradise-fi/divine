// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <rst/tristate.hpp>
#include <rst/base.hpp>

namespace __dios::rst::abstract {

    /* Unit/Star domain contains a single value, everything is abstracted to a Star. */
    struct Unit : tagged_abstract_domain_t
    {
        using Return = std::conditional_t< pointer_based, Unit *, Unit >;
        using Argument = std::conditional_t< pointer_based, const Unit *, const Unit& >;

        #define __op( name, ... ) \
            _LART_INTERFACE static Return name(__VA_ARGS__) noexcept { \
                if constexpr ( pointer_based ) { \
                    void *ptr = __vm_obj_make( sizeof( Unit ), _VM_PT_Heap ); \
                    new ( ptr ) Unit(); \
                    return static_cast< Return >( ptr ); \
                } else { \
                    return Unit{}; \
                } \
            }

        #define __un( name ) __op( name, Argument )

        #define __bin( name ) __op( name, Argument, Argument )

        #define __cast( name ) __op( name, Argument, Bitwidth )

        // TODO lift bool
        /* abstraction operations */
        __op( lift_one_i8, uint8_t )
        __op( lift_one_i16, uint16_t )
        __op( lift_one_i32, uint32_t )
        __op( lift_one_i64, uint64_t )

        __op( lift_one_float, float )
        __op( lift_one_double, double )

        template< typename T >
        __op( lift_any, Abstracted< T > )

        __op( op_thaw, Argument, uint8_t /* bw */ )

        _LART_INTERFACE
        static Tristate to_tristate( Argument ) noexcept
        {
            return { Tristate::Unknown };
        }

        __op( assume, Argument, Argument, bool );

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
    };


    struct UnitAggregate : Base
    {
        using Ptr = void *;

        template< typename T >
        _LART_INTERFACE static Ptr lift_any( Abstracted< T > ) noexcept
        {
            return __new< UnitAggregate >();
        }

        _LART_INTERFACE
        static Ptr op_load( Ptr /* address */ ) noexcept
        {
            return __new< Unit< PointerBase > >();
        }

        _LART_INTERFACE
        static void op_store( Ptr /* value */, Ptr /* address */ ) noexcept { }

        _LART_INTERFACE
        static Ptr op_gep( Ptr address, Ptr /* offset */ ) noexcept { return address; }
    };

} // namespace __dios::rst::abstract
