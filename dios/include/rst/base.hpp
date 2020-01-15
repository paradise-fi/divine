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

#include <rst/common.hpp>
#include <cstdint>

namespace __dios::rst::abstract
{
    using tag_t = uint8_t;

    struct tagged_abstract_domain_t
    {
        uint8_t id;
        __noinline tagged_abstract_domain_t( uint8_t id = 0 ) noexcept : id( id ) {}
    };

    template< _VM_PointerType PT = _VM_PT_Heap >
    struct tagged_array : Array< uint8_t, PT >
    {
        using base = Array< uint8_t, PT >;
        using typename base::iterator;

        template< typename F >
        void move_tag( F f )
        {
            uint8_t tag = base::back();
            f();
            base::back() = tag;
        }

        using base::base;

        tagged_array( const tagged_abstract_domain_t &id = tagged_abstract_domain_t() )
            : base( { id.id } ) {}

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
        tagged_storage( const type &v = type(),
                        const tagged_abstract_domain_t &id = tagged_abstract_domain_t() )
            : tagged_array( id )
        {
            resize( sizeof( type ) );
            std::uninitialized_fill( &**this, &**this + 1, v );
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

    template< typename self_t >
    struct domain_mixin
    {
        using st = self_t;
        using bw = bitwidth_t;

        __lartop static st lift_one_i1( i1 v )   noexcept { return st::lift( v ); }
        __lartop static st lift_one_i8( i8 v )   noexcept { return st::lift( v ); }
        __lartop static st lift_one_i16( i16 v ) noexcept { return st::lift( v ); }
        __lartop static st lift_one_i32( i32 v ) noexcept { return st::lift( v ); }
        __lartop static st lift_one_i64( i64 v ) noexcept { return st::lift( v ); }
        __lartop static st lift_one_f32( f32 v ) noexcept { return st::lift( v ); }
        __lartop static st lift_one_f64( f64 v ) noexcept { return st::lift( v ); }

        __lartop static st lift_one_ptr( void *v ) noexcept
        {
            return st::lift( reinterpret_cast< uintptr_t >( v ) );
        }

        static st fail()
        {
            __dios_fault( _VM_F_NotImplemented, "unsupported abstract operation" );
            return st::lift_any();
        }

        __lartop static st op_add ( st, st ) noexcept { return fail(); }
        __lartop static st op_sub ( st, st ) noexcept { return fail(); }
        __lartop static st op_mul ( st, st ) noexcept { return fail(); }
        __lartop static st op_sdiv( st, st ) noexcept { return fail(); }
        __lartop static st op_udiv( st, st ) noexcept { return fail(); }
        __lartop static st op_srem( st, st ) noexcept { return fail(); }
        __lartop static st op_urem( st, st ) noexcept { return fail(); }

        __lartop static st op_fadd( st, st ) noexcept { return fail(); }
        __lartop static st op_fsub( st, st ) noexcept { return fail(); }
        __lartop static st op_fmul( st, st ) noexcept { return fail(); }
        __lartop static st op_fdiv( st, st ) noexcept { return fail(); }
        __lartop static st op_frem( st, st ) noexcept { return fail(); }

        __lartop static st op_shl ( st, st ) noexcept { return fail(); }
        __lartop static st op_ashr( st, st ) noexcept { return fail(); }
        __lartop static st op_lshr( st, st ) noexcept { return fail(); }
        __lartop static st op_and ( st, st ) noexcept { return fail(); }
        __lartop static st op_or  ( st, st ) noexcept { return fail(); }
        __lartop static st op_xor ( st, st ) noexcept { return fail(); }

        __lartop static st op_eq ( st, st ) noexcept { return fail(); }
        __lartop static st op_neq( st, st ) noexcept { return fail(); }
        __lartop static st op_ugt( st, st ) noexcept { return fail(); }
        __lartop static st op_uge( st, st ) noexcept { return fail(); }
        __lartop static st op_ult( st, st ) noexcept { return fail(); }
        __lartop static st op_ule( st, st ) noexcept { return fail(); }
        __lartop static st op_sgt( st, st ) noexcept { return fail(); }
        __lartop static st op_sge( st, st ) noexcept { return fail(); }
        __lartop static st op_slt( st, st ) noexcept { return fail(); }
        __lartop static st op_sle( st, st ) noexcept { return fail(); }

        __lartop static st op_foeq( st, st ) noexcept { return fail(); }
        __lartop static st op_fogt( st, st ) noexcept { return fail(); }
        __lartop static st op_foge( st, st ) noexcept { return fail(); }
        __lartop static st op_folt( st, st ) noexcept { return fail(); }
        __lartop static st op_fole( st, st ) noexcept { return fail(); }
        __lartop static st op_ford( st, st ) noexcept { return fail(); }
        __lartop static st op_funo( st, st ) noexcept { return fail(); }
        __lartop static st op_fueq( st, st ) noexcept { return fail(); }
        __lartop static st op_fugt( st, st ) noexcept { return fail(); }
        __lartop static st op_fuge( st, st ) noexcept { return fail(); }
        __lartop static st op_fult( st, st ) noexcept { return fail(); }
        __lartop static st op_fule( st, st ) noexcept { return fail(); }

        __lartop static st op_ffalse( st, st ) noexcept { return fail(); }
        __lartop static st op_ftrue ( st, st ) noexcept { return fail(); }

        __lartop static st op_trunc  ( st, bw ) noexcept { return fail(); }
        __lartop static st op_fptrunc( st, bw ) noexcept { return fail(); }
        __lartop static st op_sitofp ( st, bw ) noexcept { return fail(); }
        __lartop static st op_uitofp ( st, bw ) noexcept { return fail(); }
        __lartop static st op_zext   ( st, bw ) noexcept { return fail(); }
        __lartop static st op_sext   ( st, bw ) noexcept { return fail(); }
        __lartop static st op_fpext  ( st, bw ) noexcept { return fail(); }
        __lartop static st op_fptosi ( st, bw ) noexcept { return fail(); }
        __lartop static st op_fptoui ( st, bw ) noexcept { return fail(); }

        __lartop static st op_concat ( st, st ) noexcept { return fail(); }
    };
}
