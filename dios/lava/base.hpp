// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
 * (c) 2020 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <sys/cdefs.h>
#include <sys/fault.h>
#include <util/array.hpp>
#include <cstdint>

namespace __lava
{
    using bitwidth_t = uint8_t;
    using tag_t = uint8_t;

    using pointer_type = _VM_PointerType;

    template< pointer_type PT = _VM_PT_Heap >
    using array = __dios::Array< uint8_t, PT >;

    template< pointer_type PT = _VM_PT_Heap >
    struct tagged_array : array< PT >
    {
        using base = array< PT >;
        using typename base::iterator;

        template< typename F >
        void move_tag( F f )
        {
            uint8_t tag = base::back();
            f();
            base::back() = tag;
        }

        using base::base;

        tagged_array( const tagged_array &o, typename base::construct_shared_t s ) : base( o, s ) {}

        auto  end()       { return base::end() - 1; }
        auto  end() const { return base::end() - 1; }
        auto cend() const { return base::end() - 1; }

        auto  rbegin()       { return base::rbegin() + 1; }
        auto  rbegin() const { return base::rbegin() + 1; }
        auto crbegin() const { return base::rbegin() + 1; }

        auto       &back()       { return *( end() - 1 ); }
        const auto &back() const { return *( end() - 1 ); }
        uint8_t    &tag()        { return base::back(); }
        uint8_t     tag()  const { return base::back(); }

        int  size()  const { return base::size() - 1; }
        bool empty() const { return size() == 0; }
        void _grow() { base::push_back( 0 ); }
        base       &raw()       { return *this; }
        const base &raw() const { return *this; }

        void push_back( uint8_t v )          { move_tag( [&]{ base::back() = v; _grow(); } ); }
        void emplace_back( uint8_t v )       { move_tag( [&]{ base::back() = v; _grow(); } ); }
        void resize( int sz )                { move_tag( [&]{ base::resize( sz + 1 ); } ); }
        void clear()                         { move_tag( [&]{ base::resize( 1 ); } ); }
        void pop_back()                      { move_tag( [&]{ base::pop_back(); } ); }
        void erase( iterator it )            { move_tag( [&]{ base::erase( it ); } ); }
        void erase( iterator b, iterator e ) { move_tag( [&]{ base::erase( b, e ); } ); }
        void insert( iterator i, uint8_t v ) { move_tag( [&]{ base::insert( i, v ); } ); }
    };

    template< typename type >
    struct tagged_storage : tagged_array<>
    {
        using tagged_array<>::tagged_array;

        tagged_storage( const type &v = type() )
        {
            resize( sizeof( type ) );
            new ( &**this ) type( v );
        }

        const type &operator*() const { return *reinterpret_cast< const type * >( begin() ); }
        type       &operator*()       { return *reinterpret_cast< type * >( begin() ); }

        const type *operator->() const { return &**this; }
        type       *operator->()       { return &**this; }

        type &get() { return **this; }
    };

    using i1  = bool;
    using i8  = uint8_t;
    using i16 = uint16_t;
    using i32 = uint32_t;
    using i64 = uint64_t;
    using f32 = float;
    using f64 = double;

    template< typename type >
    constexpr int bitwidth_v = std::is_same_v< type, bool > ? 1 : sizeof( type ) * 8;

    template< typename self_t >
    struct domain_mixin
    {
        using st = self_t;
        using bw = bitwidth_t;

        static st lift_i1( i1 v )   { return st::lift( v ); }
        static st lift_i8( i8 v )   { return st::lift( v ); }
        static st lift_i16( i16 v ) { return st::lift( v ); }
        static st lift_i32( i32 v ) { return st::lift( v ); }
        static st lift_i64( i64 v ) { return st::lift( v ); }
        static st lift_f32( f32 v ) { return st::lift( v ); }
        static st lift_f64( f64 v ) { return st::lift( v ); }

        static st lift_ptr( void *v )
        {
            return st::lift( reinterpret_cast< uintptr_t >( v ) );
        }

        static st fail()
        {
            __dios_fault( _VM_F_NotImplemented, "unsupported abstract operation" );
            __builtin_trap();
            // return st::any();
        }

        static st op_add ( st, st ) { return fail(); }
        static st op_sub ( st, st ) { return fail(); }
        static st op_mul ( st, st ) { return fail(); }
        static st op_sdiv( st, st ) { return fail(); }
        static st op_udiv( st, st ) { return fail(); }
        static st op_srem( st, st ) { return fail(); }
        static st op_urem( st, st ) { return fail(); }

        static st op_fadd( st, st ) { return fail(); }
        static st op_fsub( st, st ) { return fail(); }
        static st op_fmul( st, st ) { return fail(); }
        static st op_fdiv( st, st ) { return fail(); }
        static st op_frem( st, st ) { return fail(); }

        static st op_shl ( st, st ) { return fail(); }
        static st op_ashr( st, st ) { return fail(); }
        static st op_lshr( st, st ) { return fail(); }
        static st op_and ( st, st ) { return fail(); }
        static st op_or  ( st, st ) { return fail(); }
        static st op_xor ( st, st ) { return fail(); }

        static st op_eq ( st, st ) { return fail(); }
        static st op_neq( st, st ) { return fail(); }
        static st op_ugt( st, st ) { return fail(); }
        static st op_uge( st, st ) { return fail(); }
        static st op_ult( st, st ) { return fail(); }
        static st op_ule( st, st ) { return fail(); }
        static st op_sgt( st, st ) { return fail(); }
        static st op_sge( st, st ) { return fail(); }
        static st op_slt( st, st ) { return fail(); }
        static st op_sle( st, st ) { return fail(); }

        static st op_foeq( st, st ) { return fail(); }
        static st op_fogt( st, st ) { return fail(); }
        static st op_foge( st, st ) { return fail(); }
        static st op_folt( st, st ) { return fail(); }
        static st op_fole( st, st ) { return fail(); }
        static st op_ford( st, st ) { return fail(); }
        static st op_funo( st, st ) { return fail(); }
        static st op_fueq( st, st ) { return fail(); }
        static st op_fugt( st, st ) { return fail(); }
        static st op_fuge( st, st ) { return fail(); }
        static st op_fult( st, st ) { return fail(); }
        static st op_fule( st, st ) { return fail(); }

        static st op_ffalse( st, st ) { return fail(); }
        static st op_ftrue ( st, st ) { return fail(); }

        static st op_trunc  ( st, bw ) { return fail(); }
        static st op_fptrunc( st, bw ) { return fail(); }
        static st op_sitofp ( st, bw ) { return fail(); }
        static st op_uitofp ( st, bw ) { return fail(); }
        static st op_zext   ( st, bw ) { return fail(); }
        static st op_sext   ( st, bw ) { return fail(); }
        static st op_fpext  ( st, bw ) { return fail(); }
        static st op_fptosi ( st, bw ) { return fail(); }
        static st op_fptoui ( st, bw ) { return fail(); }

        static st op_concat ( st, st ) { return fail(); }
        static st op_extract( st, bw, bw ) { return fail(); }

        static st op_gep( st, st, uint64_t ) { return fail(); }
        static st op_store( st, st, bw ) { return fail(); }
        static st op_load( st, bw ) { return fail(); }
    };
}
