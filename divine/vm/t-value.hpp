// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/value.hpp>


namespace divine::t_vm
{

struct TestInt
{
    using Int16 = vm::value::Int< 16 >;
    using Int32 = vm::value::Int< 32 >;
    using Int64 = vm::value::Int< 64 >;

    TEST( size )
    {
        ASSERT_EQ( sizeof( vm::value::Int< 16 >::Raw ), 2 );
        ASSERT_EQ( sizeof( vm::value::Int< 32 >::Raw ), 4 );
        ASSERT_EQ( sizeof( vm::value::Int< 64 >::Raw ), 8 );
        ASSERT_EQ( sizeof( vm::value::Int< 60 >::Raw ), 8 );
        ASSERT_EQ( sizeof( vm::value::Int< 10 >::Raw ), 2 );
    }

    TEST( arithmetic )
    {
        Int16 a( 0 ), b( 0 );
        vm::value::Bool x = ( a + b == a );
        ASSERT( x.raw() == 1 );
        ASSERT_EQ( x.defbits(), 1 );
        a = Int16( 4 );
        ASSERT( ( a != b ).cooked() );
        b = a;
        ASSERT( ( a == b ).cooked() );
        ASSERT( ( a == b ).defined() );
        b = Int16( 4, 0, false );
        ASSERT( !( a == b ).defined() );
        ASSERT( ( a == b ).cooked() );
    }

    TEST( bitwise )
    {
        Int16 a( 1, 0xFFFE, false ), b( 2, 0xFFFD, false );
        ASSERT( (a & b).cooked() == 0 );
        ASSERT( (a & b).defined() );
        ASSERT( !(a | b).defined() );
    }

    TEST ( shift_right )
    {
        vm::value::Int< 16, true > a( 115, 0x7FFF, false );
        Int16 b( 2, 0xFFFF, false );
        auto res = a >> b;
        ASSERT_EQ( res.cooked(), 115 >> 2 );
        ASSERT_EQ( res._m, 0x1FFF );

        Int16 c( 1135, 0xFFF, false ), d( 2, 0xFFFF, false );
        auto res1 = c >> d;
        ASSERT_EQ( res1.cooked(), 1135 >> 2 );
        ASSERT_EQ( res1._m, 0xC3FF );

        Int16 e( 1, 0xFEFF, false );
        ASSERT( !( c >> e ).defined() );
    }

    TEST( isptr32 )
    {
        Int32 a( 33, 0xFFFFFFFFu, true );
        ASSERT( a.pointer() );
        a = a + Int32( 3 );
        ASSERT( !a.pointer() );
    }

    TEST( isptr64 )
    {
        Int64 a( 33, 0xFFFFFFFFFFFFFFFFul, true );
        ASSERT( a.pointer() );
        a = a + Int64( 3 );
        ASSERT( !a.pointer() );
    }

    TEST( isptr_bw )
    {
        Int64 a( 33ul << 32, 0xFFFFFFFFFFFFFFFFul, true );
        ASSERT( a.pointer() );
        a = a | Int64( 5 );
        ASSERT( a.pointer() );
        a = a & Int64( 0x0000FFFF00000000ul );
        ASSERT( a.pointer() );
    }

    TEST( isptr_shift )
    {
        vm::value::Int< 64 > a( 33ul << 32, 0xFFFFFFFFFFFFFFFFul, true );
        ASSERT( a.pointer() );
        a = a >> Int64( 5 );
        ASSERT( !a.pointer() );
        a = a << Int64( 5 );
        ASSERT( a.pointer() );
    }

    TEST( isptr_shift_destroy )
    {
        vm::value::Int< 64 > a( 33ul << 32, 0xFFFFFFFFFFFFFFFFul, true );
        ASSERT( a.pointer() );
        a = a << Int64( 5 );
        ASSERT( !a.pointer() );
        a = a >> Int64( 5 );
        ASSERT( !a.pointer() );
    }

    TEST( isptr_trunc )
    {
        vm::value::Int< 64 > a( 33ul << 32, 0xFFFFFFFFFFFFFFFFul, true );
        ASSERT( a.pointer() );
        a = a >> Int64( 32 );
        ASSERT( !a.pointer() );
        Int32 b = a;
        ASSERT( b.pointer() );
    }

    TEST( notptr_trunc )
    {
        vm::value::Int< 64 > a( 33ul << 32, 0xFFFFFFFFFFFFFFFFul, true );
        ASSERT( a.pointer() );
        a = a >> Int64( 30 );
        ASSERT( !a.pointer() );
        Int32 b = a;
        ASSERT( !b.pointer() );
    }
};

struct TestPtr
{
    TEST( defined )
    {
        vm::value::Pointer p( vm::nullPointer(), true );
        ASSERT( p.defined() );
        p.defbits( -1 );
        ASSERT( p.defined() );
        p.defbits( 32 );
        ASSERT( !p.defined() );
    }

    TEST( isptr )
    {
        vm::value::Pointer p( vm::HeapPointer( 3, 0 ), true, true );
        ASSERT( p.pointer() );
        p = p + 20;
        ASSERT( p.pointer() );
        ASSERT_EQ( p.cooked().offset(), 20 );
        p = p + (-20);
        ASSERT( p.pointer() );
        ASSERT_EQ( p.cooked().object(), 3 );
        ASSERT_EQ( p.cooked().offset(), 0 );
    }

    TEST( intptr )
    {
        using I = vm::value::Int< vm::PointerBytes * 8 >;
        vm::value::Pointer p( vm::HeapPointer( 3, 0 ), true, true );
        I i( p );
        i = i + I( 20 );
        p = vm::value::Pointer( i );
        ASSERT( p.pointer() );
        ASSERT_EQ( p.cooked().object(), 3 );
        ASSERT_EQ( p.cooked().offset(), 20 );
    }

};

struct TestTaint
{
    TEST( def )
    {
        vm::value::Pointer p( vm::nullPointer(), true );
        vm::value::Int< 32 > i;
        ASSERT_EQ( p.taints(), 0 );
        ASSERT_EQ( i.taints(), 0 );
    }

    TEST( arith )
    {
        vm::value::Int< 32 > a( 7 ), b( 8 );
        a.taints( 1 );
        ASSERT_EQ( a.taints(), 1 );
        ASSERT_EQ( b.taints(), 0 );
        auto c = a + b;
        ASSERT_EQ( c.taints(), 1 );
        c = c + c;
        ASSERT_EQ( c.taints(), 1 );
        c = b + b;
        ASSERT_EQ( c.taints(), 0 );
        c = a * a;
        ASSERT_EQ( c.taints(), 1 );
        c = a * b;
        ASSERT_EQ( c.taints(), 1 );
        c = b * b;
        ASSERT_EQ( c.taints(), 0 );
    }

    TEST( multiple )
    {
        vm::value::Int< 32 > a( 7 ), b( 8 );
        a.taints( 1 );
        b.taints( 2 );
        ASSERT_EQ( a.taints(), 1 );
        ASSERT_EQ( b.taints(), 2 );
        auto c = a + b;
        ASSERT_EQ( c.taints(), 3 );
        c = c + c;
        ASSERT_EQ( c.taints(), 3 );
        c = a + a;
        ASSERT_EQ( c.taints(), 1 );
        c = a * a;
        ASSERT_EQ( c.taints(), 1 );
        c = b * b;
        ASSERT_EQ( c.taints(), 2 );
        c = a * b;
        ASSERT_EQ( c.taints(), 3 );
    }

    TEST( bitwise )
    {
        vm::value::Int< 32 > a( 7 ), b( 8 );
        a.taints( 1 );
        ASSERT_EQ( a.taints(), 1 );
        ASSERT_EQ( b.taints(), 0 );
        auto c = a & b;
        ASSERT_EQ( c.taints(), 1 );
        c = a | b;
        ASSERT_EQ( c.taints(), 1 );
        c = a ^ b;
        ASSERT_EQ( c.taints(), 1 );
        c = b;
        ASSERT_EQ( c.taints(), 0 );
    }

    TEST( ptr )
    {
        vm::value::Pointer a, b;
        a.taints( 1 );
        ASSERT_EQ( a.taints(), 1 );
        ASSERT_EQ( b.taints(), 0 );
        a = b;
        ASSERT_EQ( a.taints(), 0 );
        a.taints( 3 );
        ASSERT_EQ( a.taints(), 3 );
        vm::value::Int< 64 > x = a;
        ASSERT_EQ( x.taints(), 3 );
        b = vm::value::Pointer( x );
        ASSERT_EQ( b.taints(), 3 );
    }

    TEST( flt )
    {
        vm::value::Float< double > a, b;
        a.taints( 1 );
        ASSERT_EQ( a.taints(), 1 );
        ASSERT_EQ( b.taints(), 0 );
        auto c = a + b;
        ASSERT_EQ( c.taints(), 1 );
        c = a * b;
        ASSERT_EQ( c.taints(), 1 );
        c = c * c;
        ASSERT_EQ( c.taints(), 1 );
        c = c + a;
        ASSERT_EQ( c.taints(), 1 );
        c = c + b;
        ASSERT_EQ( c.taints(), 1 );
        c = b + b;
        ASSERT_EQ( c.taints(), 0 );
    }

    TEST( conv_1 )
    {
        vm::value::Float< double > a;
        a.taints( 1 );
        auto b = vm::value::Int< 64 >( a );
        ASSERT_EQ( b.taints(), 1 );
        auto p = vm::value::Pointer( b );
        ASSERT_EQ( p.taints(), 1 );
        auto i = vm::value::Int< 64 >( p );
        ASSERT_EQ( i.taints(), 1 );
        auto f = vm::value::Float< double >( i );
        ASSERT_EQ( f.taints(), 1 );
    }

    TEST( conv_0 )
    {
        vm::value::Float< double > a;
        auto b = vm::value::Int< 64 >( a );
        ASSERT_EQ( b.taints(), 0 );
        auto p = vm::value::Pointer( b );
        ASSERT_EQ( p.taints(), 0 );
        auto i = vm::value::Int< 64 >( p );
        ASSERT_EQ( i.taints(), 0 );
        auto f = vm::value::Float< double >( i );
        ASSERT_EQ( f.taints(), 0 );
    }
};

}
