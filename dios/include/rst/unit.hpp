// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <rst/tristate.hpp>
#include <rst/base.hpp>

namespace __dios::rst::abstract {

    struct unit_t;

    extern unit_t unit_base_id;

    /* unit_t/Star domain contains a single value, everything is abstracted to a Star. */
    struct unit_t : tagged_abstract_domain_t
    {
        using unit_value_t = unit_t *;

        _LART_INLINE
        static unit_value_t unit_value() noexcept { return &unit_base_id; }

        #define __op( name, ... ) \
            _LART_INTERFACE static unit_value_t name(__VA_ARGS__) noexcept { \
                return unit_value(); \
            }

        #define __un( name ) __op( name, unit_value_t )

        #define __bin( name ) __op( name, unit_value_t, unit_value_t )

        #define __cast( name ) __op( name, unit_value_t, Bitwidth )

        _LART_INTERFACE
        static unit_t lift_any() noexcept { return unit_t(); }

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

        __op( lift_any_ptr )
        __op( lift_one_ptr, void * ptr )

        __op( lift_any_aggr, unsigned size )
        __op( lift_one_aggr, void * addr, unsigned size )

        __op( op_thaw, unit_value_t /* addr */, uint8_t /* bw */ )

        __op( op_load, unit_value_t /* addr */ )

        _LART_INTERFACE
        static void op_store( unit_value_t /* val */, unit_value_t /* addr */ ) noexcept { }

        __op( op_insertvalue, unit_value_t /* agg */, unit_value_t /* val */, uint64_t /* off */ )
        __op( op_extractvalue, unit_value_t /* agg */, uint64_t /* off */ )

        __op( op_gep, unit_value_t /* addr */, unit_value_t /* off */ )

        _LART_INTERFACE
        static Tristate to_tristate( unit_value_t ) noexcept
        {
            return { Tristate::Unknown };
        }

        __op( assume, unit_value_t, unit_value_t, bool );

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

} // namespace __dios::rst::abstract
