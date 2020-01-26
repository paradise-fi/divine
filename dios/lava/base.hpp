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

        tagged_array() { base::resize( 1, -1 ); }
        tagged_array( const tagged_array &o, __dios::construct_shared_t s ) : base( o, s ) {}

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
        using base = tagged_array<>;

        tagged_storage( void *v, __dios::construct_shared_t s ) : base( v, s ) {}

        template< typename... args_t >
        tagged_storage( args_t && ... args )
        {
            resize( sizeof( type ) );
            new ( &**this ) type( std::forward< args_t >( args ) ... );
        }

        tagged_storage( const tagged_storage &o ) : tagged_storage( o.get() ) {}
        tagged_storage( tagged_storage &&o ) : tagged_storage( std::move( o.get() ) ) {}
        tagged_storage &operator=( const tagged_storage &o ) { get() = o.get(); return *this; }

        ~tagged_storage()
        {
            if ( begin() )
                get().~type();
        }

        const type &operator*() const { return *reinterpret_cast< const type * >( begin() ); }
        type       &operator*()       { return *reinterpret_cast< type * >( begin() ); }

        const type *operator->() const { return &**this; }
        type       *operator->()       { return &**this; }

        type       &get()       { return **this; }
        const type &get() const { return **this; }
    };

    using i1  = bool;
    using i8  = uint8_t;
    using i16 = uint16_t;
    using i32 = uint32_t;
    using i64 = uint64_t;
    using f32 = float;
    using f64 = double;

    struct array_ref { void *base; size_t size; };

    template< typename type >
    constexpr int bitwidth_v = std::is_same_v< type, bool > ? 1 : sizeof( type ) * 8;

    template< typename self_t >
    struct domain_mixin
    {
        using st = self_t;
        using sr = const self_t &;
        using bw = bitwidth_t;

        static st lift_i1( i1 v )   { return st::lift( v ); }
        static st lift_i8( i8 v )   { return st::lift( v ); }
        static st lift_i16( i16 v ) { return st::lift( v ); }
        static st lift_i32( i32 v ) { return st::lift( v ); }
        static st lift_i64( i64 v ) { return st::lift( v ); }
        static st lift_f32( f32 v ) { return st::lift( v ); }
        static st lift_f64( f64 v ) { return st::lift( v ); }

        static st lift_arr( void *v, size_t s ) { return st::lift( array_ref{ v, s } ); }
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

        static st op_add ( sr, sr ) { return fail(); }
        static st op_sub ( sr, sr ) { return fail(); }
        static st op_mul ( sr, sr ) { return fail(); }
        static st op_sdiv( sr, sr ) { return fail(); }
        static st op_udiv( sr, sr ) { return fail(); }
        static st op_srem( sr, sr ) { return fail(); }
        static st op_urem( sr, sr ) { return fail(); }

        static st op_fadd( sr, sr ) { return fail(); }
        static st op_fsub( sr, sr ) { return fail(); }
        static st op_fmul( sr, sr ) { return fail(); }
        static st op_fdiv( sr, sr ) { return fail(); }
        static st op_frem( sr, sr ) { return fail(); }

        static st op_shl ( sr, sr ) { return fail(); }
        static st op_ashr( sr, sr ) { return fail(); }
        static st op_lshr( sr, sr ) { return fail(); }
        static st op_and ( sr, sr ) { return fail(); }
        static st op_or  ( sr, sr ) { return fail(); }
        static st op_xor ( sr, sr ) { return fail(); }

        static st op_eq ( sr, sr ) { return fail(); }
        static st op_ne ( sr, sr ) { return fail(); }
        static st op_ugt( sr, sr ) { return fail(); }
        static st op_uge( sr, sr ) { return fail(); }
        static st op_ult( sr, sr ) { return fail(); }
        static st op_ule( sr, sr ) { return fail(); }
        static st op_sgt( sr, sr ) { return fail(); }
        static st op_sge( sr, sr ) { return fail(); }
        static st op_slt( sr, sr ) { return fail(); }
        static st op_sle( sr, sr ) { return fail(); }

        static st op_foeq( sr, sr ) { return fail(); }
        static st op_fogt( sr, sr ) { return fail(); }
        static st op_foge( sr, sr ) { return fail(); }
        static st op_folt( sr, sr ) { return fail(); }
        static st op_fole( sr, sr ) { return fail(); }
        static st op_ford( sr, sr ) { return fail(); }
        static st op_funo( sr, sr ) { return fail(); }
        static st op_fueq( sr, sr ) { return fail(); }
        static st op_fugt( sr, sr ) { return fail(); }
        static st op_fuge( sr, sr ) { return fail(); }
        static st op_fult( sr, sr ) { return fail(); }
        static st op_fule( sr, sr ) { return fail(); }

        static st op_ffalse( sr, sr ) { return fail(); }
        static st op_ftrue ( sr, sr ) { return fail(); }

        static st op_trunc  ( sr, bw ) { return fail(); }
        static st op_fptrunc( sr, bw ) { return fail(); }
        static st op_sitofp ( sr, bw ) { return fail(); }
        static st op_uitofp ( sr, bw ) { return fail(); }
        static st op_zext   ( sr, bw ) { return fail(); }
        static st op_zfit   ( sr, bw ) { return fail(); }
        static st op_sext   ( sr, bw ) { return fail(); }
        static st op_fpext  ( sr, bw ) { return fail(); }
        static st op_fptosi ( sr, bw ) { return fail(); }
        static st op_fptoui ( sr, bw ) { return fail(); }

        static st op_concat ( sr, sr ) { return fail(); }
        static st op_extract( sr, bw, bw ) { return fail(); }

        static st op_gep( sr, sr, uint64_t ) { return fail(); }
        static st op_store( sr, sr, bw ) { return fail(); }
        static st op_load( sr, bw ) { return fail(); }
    };
}
