// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <rst/tristate.hpp>
#include <rst/base.hpp>

namespace __dios::rst::abstract {

    struct Unit : Base
    {
        using Bitwidth = uint8_t;

        /* abstraction operations */
        static Unit * lift_one( uint8_t ) noexcept;
        static Unit * lift_one( uint16_t ) noexcept;
        static Unit * lift_one( uint32_t ) noexcept;
        static Unit * lift_one( uint64_t ) noexcept;

        static Unit * lift_one( float ) noexcept;
        static Unit * lift_one( double ) noexcept;

        static Unit * lift_any() noexcept;

        static Tristate to_tristate( Unit * ) noexcept;

        static Unit * assume( Unit *, Unit *, bool ) noexcept;

        /* arithmetic operations */
        static Unit * op_add( Unit *, Unit * ) noexcept;
        static Unit * op_fadd( Unit *, Unit * ) noexcept;
        static Unit * op_sub( Unit *, Unit * ) noexcept;
        static Unit * op_fsub( Unit *, Unit * ) noexcept;
        static Unit * op_mul( Unit *, Unit * ) noexcept;
        static Unit * op_fmul( Unit *, Unit * ) noexcept;
        static Unit * op_udiv( Unit *, Unit * ) noexcept;
        static Unit * op_sdiv( Unit *, Unit * ) noexcept;
        static Unit * op_fdiv( Unit *, Unit * ) noexcept;
        static Unit * op_urem( Unit *, Unit * ) noexcept;
        static Unit * op_srem( Unit *, Unit * ) noexcept;
        static Unit * op_frem( Unit *, Unit * ) noexcept;

        /* logic operations */
        static Unit * op_fneg( Unit * ) noexcept;

        /* bitwise operations */
        static Unit * op_shl( Unit *, Unit * ) noexcept;
        static Unit * op_lshr( Unit *, Unit * ) noexcept;
        static Unit * op_ashr( Unit *, Unit * ) noexcept;
        static Unit * op_and( Unit *, Unit * ) noexcept;
        static Unit * op_or( Unit *, Unit * ) noexcept;
        static Unit * op_xor( Unit *, Unit * ) noexcept;

        /* comparison operations */
        static Unit * op_ffalse( Unit *, Unit * ) noexcept;
        static Unit * op_foeq( Unit *, Unit * ) noexcept;
        static Unit * op_fogt( Unit *, Unit * ) noexcept;
        static Unit * op_foge( Unit *, Unit * ) noexcept;
        static Unit * op_folt( Unit *, Unit * ) noexcept;
        static Unit * op_fole( Unit *, Unit * ) noexcept;
        static Unit * op_fone( Unit *, Unit * ) noexcept;
        static Unit * op_ford( Unit *, Unit * ) noexcept;
        static Unit * op_funo( Unit *, Unit * ) noexcept;
        static Unit * op_fueq( Unit *, Unit * ) noexcept;
        static Unit * op_fugt( Unit *, Unit * ) noexcept;
        static Unit * op_fuge( Unit *, Unit * ) noexcept;
        static Unit * op_fult( Unit *, Unit * ) noexcept;
        static Unit * op_fule( Unit *, Unit * ) noexcept;
        static Unit * op_fune( Unit *, Unit * ) noexcept;
        static Unit * op_ftrue( Unit *, Unit * ) noexcept;
        static Unit * op_eq( Unit *, Unit * ) noexcept;
        static Unit * op_ne( Unit *, Unit * ) noexcept;
        static Unit * op_ugt( Unit *, Unit * ) noexcept;
        static Unit * op_uge( Unit *, Unit * ) noexcept;
        static Unit * op_ult( Unit *, Unit * ) noexcept;
        static Unit * op_ule( Unit *, Unit * ) noexcept;
        static Unit * op_sgt( Unit *, Unit * ) noexcept;
        static Unit * op_sge( Unit *, Unit * ) noexcept;
        static Unit * op_slt( Unit *, Unit * ) noexcept;
        static Unit * op_sle( Unit *, Unit * ) noexcept;

        /* cast operations */
        static Unit * op_fpext( Unit *, Bitwidth ) noexcept;
        static Unit * op_fptosi( Unit *, Bitwidth ) noexcept;
        static Unit * op_fptoui( Unit *, Bitwidth ) noexcept;
        static Unit * op_fptrunc( Unit *, Bitwidth ) noexcept;
        static Unit * op_inttoptr( Unit *, Bitwidth ) noexcept;
        static Unit * op_ptrtoint( Unit *, Bitwidth ) noexcept;
        static Unit * op_sext( Unit *, Bitwidth ) noexcept;
        static Unit * op_sitofp( Unit *, Bitwidth ) noexcept;
        static Unit * op_trunc( Unit *, Bitwidth ) noexcept;
        static Unit * op_uitofp( Unit *, Bitwidth ) noexcept;
        static Unit * op_zext( Unit *, Bitwidth ) noexcept;
    };

} // namespace __dios::rst::abstract
