// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
 * (c) 2018 Adam Matoušek <xmatous3@fi.muni.cz>
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

#include <brick-types>

#include <divine/vm/value.hpp>
#include <divine/vm/types.hpp>
#include <divine/vm/memory.hpp>

namespace divine::t_vm
{

struct CompressPDT
{
    struct Empty {};
    using ShDesc = mem::CompressPDT< Empty >;

    TEST( reasonable_zero )
    {
        ShDesc::Compressed c = 0;
        ShDesc::Expanded exp = ShDesc::expand( c );
        ASSERT_EQ( exp, 0x0000 );
    }

    TEST( bitlevel )
    {
        ShDesc::Expanded exp;
        ASSERT_EQ( exp, 0x0000 );
        exp.pointer = true;
        ASSERT_EQ( exp, 0x0080 );
        exp.taint = 3;
        ASSERT_EQ( exp, 0x0083 );
        exp.defined = 0xC;
        ASSERT_EQ( exp, 0xC083 );
        exp.data_exception = true;
        ASSERT_EQ( exp, 0xC283 );
        exp.pointer_exception = true;
        ASSERT_EQ( exp, 0xC383 );
    }

    TEST( compress_expand )
    {
        for ( uint8_t taint = 0; taint < 16; ++taint )
        {
            for ( uint8_t def = 0; def < 16; ++def )
            {
                ShDesc::Expanded exp;
                exp.taint = taint;
                exp.defined = def;
                auto reexp = ShDesc::expand( ShDesc::compress( exp ) );
                exp.taint &= exp.defined;
                ASSERT_EQ( exp._raw, reexp._raw );
            }

            ShDesc::Expanded exp;
            exp.taint = taint;

            exp.defined = 0xF;
            exp.pointer = true;
            auto reexp = ShDesc::expand( ShDesc::compress( exp ) );
            ASSERT_EQ( exp._raw, reexp._raw );

            exp.defined = 0;
            exp.pointer = false;

            exp.data_exception = true;
            reexp = ShDesc::expand( ShDesc::compress( exp ) );
            ASSERT_EQ( exp._raw, reexp._raw );

            exp.pointer_exception = true;
            reexp = ShDesc::expand( ShDesc::compress( exp ) );
            ASSERT_EQ( exp._raw, reexp._raw );
        }
    }
};

struct CompressPD
{
    struct Empty {};
    using ShDesc = mem::CompressPD< Empty >;

    TEST( reasonable_zero )
    {
        ShDesc::Compressed c = 0;
        ShDesc::Expanded exp = ShDesc::expand( c );
        ASSERT_EQ( exp, 0x0000 );
    }

    TEST( bitlevel )
    {
        ShDesc::Expanded exp;
        ASSERT_EQ( exp, 0x00 );
        exp.pointer = true;
        ASSERT_EQ( exp, 0x10 );
        exp.defined = 0xC;
        ASSERT_EQ( exp, 0x1C );
        exp.data_exception = true;
        ASSERT_EQ( exp, 0x5C );
        exp.pointer_exception = true;
        ASSERT_EQ( exp, 0x7C );
    }

    TEST( compress_expand )
    {
        // This is actually trivial, since compression and expansion are no-op
        for ( uint8_t def = 0; def < 16; ++def )
        {
            ShDesc::Expanded exp;
            exp.defined = def;
            auto reexp = ShDesc::expand( ShDesc::compress( exp ) );
            ASSERT_EQ( exp._raw, reexp._raw );
        }

        ShDesc::Expanded exp;

        exp.defined = 0xF;
        exp.pointer = true;
        auto reexp = ShDesc::expand( ShDesc::compress( exp ) );
        ASSERT_EQ( exp._raw, reexp._raw );

        exp.defined = 0;
        exp.pointer = false;

        exp.data_exception = true;
        reexp = ShDesc::expand( ShDesc::compress( exp ) );
        ASSERT_EQ( exp._raw, reexp._raw );

        exp.pointer_exception = true;
        reexp = ShDesc::expand( ShDesc::compress( exp ) );
        ASSERT_EQ( exp._raw, reexp._raw );
    }
};

using Pool = brick::mem::Pool<>;

template< typename Next >
struct TestHeap : Next
{
    using typename Next::Loc;
    using Ptr = typename Next::Internal;
    using Next::_objects;

    auto pointers( Ptr p, int sz ) { return Next::pointers( loc( p, 0 ), sz ); }
    auto pointers( Ptr p, int from, int sz ) { return Next::pointers( loc( p, from ), sz ); }
    bool tainted( Ptr p, int from, int sz ) { return Next::tainted( loc( p, from ), sz ); }
    Loc loc( Ptr p, int off ) { return Loc( p, 0, off ); }

    Ptr make( int sz )
    {
        auto r = _objects.allocate( sz );
        Next::materialise( r, sz );
        return r;
    }

    template< typename T >
    void write( Ptr p, int off, T t )
    {
        Next::write( loc( p, off ), t );
        *_objects.template machinePointer< typename T::Raw >( p, off ) = t.raw();
    }

    template< typename T >
    void read( Ptr p, int off, T &t )
    {
        t.raw( *_objects.template machinePointer< typename T::Raw >( p, off ) );
        Next::read( loc( p, off ), t );
    }

    uint8_t *unsafe_ptr2mem( Ptr i ) const
    {
        return _objects.template machinePointer< uint8_t >( i );
    }

    void copy( Ptr pf, int of, Ptr pt, int ot, int sz )
    {
        auto data_from = _objects.template machinePointer< uint8_t >( pf, of ),
             data_to   = _objects.template machinePointer< uint8_t >( pt, ot );
        Next::copy( *this, loc( pf, of ), *this, loc( pt, ot ), sz, false );
        std::copy( data_from, data_from + sz, data_to );
    }
};

struct CompoundShadow
{
    using PointerV = vm::value::Pointer;
    using H = TestHeap< mem::ShadowLayers< vm::HeapBase< 8 > > >;
    H heap;
    H::Ptr obj;

    CompoundShadow() { obj = heap.make( 100 ); }

#if 0
    void set_pointer( int off, bool offd = true, bool objd = true )
    {
        _shb[ off ].exception = false;
        _shb[ off ].pointer = true;
        _shb[ off ].is_first = true;
        _shb[ off ].obj_defined = objd;
        _shb[ off ].off_defined = offd;
        _shb[ off + 1 ].pointer = true;
        _shb[ off + 1 ].is_first = false;
    }

    void check_pointer( int off )
    {
        ASSERT( !_shb[ off ].exception );
        ASSERT( _shb[ off ].pointer );
        ASSERT( _shb[ off ].is_first );
        ASSERT( _shb[ off ].obj_defined );
        ASSERT( _shb[ off ].off_defined );
        ASSERT( _shb[ off + 1 ].pointer );
        ASSERT( !_shb[ off + 1 ].is_first );
    }

    void set_data( int off, uint8_t d = 0xF )
    {
        _shb[ off ].exception = false;
        _shb[ off ].pointer = false;
        _shb[ off ].bytes_defined = d;
    }

    void check_data( int off, uint8_t d )
    {
        ASSERT( !_shb[ off ].exception );
        ASSERT( !_shb[ off ].pointer );
        ASSERT_EQ( _shb[ off ].bytes_defined, d );
    }

    Sh shadow() { return Sh{ &_shb.front(), nullptr, 400 }; }
#endif

    TEST( read_int )
    {
        vm::value::Int< 32 > i1( 32, 0xFFFF'FFFF, false ), i2;
        heap.write( obj, 0, i1 );
        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.defbits(), 0xFFFF'FFFF );
        ASSERT( i2.defined() );
    }

    TEST( read_short )
    {
        vm::value::Int< 16 > i1( 32, 0xFFFF, false ), i2;
        heap.write( obj, 2, i1 );
        heap.read( obj, 2, i2 );
        ASSERT_EQ( i2.defbits(), 0xFFFF );
        ASSERT( i2.defined() );
    }

    TEST( copy_int )
    {
        vm::value::Int< 32 > i1( 32, 0xFFFF'FFFF, false ), i2;
        heap.write( obj, 0, i1 );
        heap.copy( obj, 0, obj, 4, 4 );
        heap.read( obj, 4, i2 );
        ASSERT( i2.defined() );
    }

    TEST( copy_short )
    {
        vm::value::Int< 16 > i1( 32, 0xFFFF, false ), i2;
        heap.write( obj, 0, i1 );
        heap.copy( obj, 0, obj, 2, 2 );
        heap.read( obj, 2, i2 );
        ASSERT( i2.defined() );
    }

    TEST( read_ptr )
    {
        PointerV p1( vm::CodePointer( 3, 0 ) ), p2;
        heap.write( obj, 0, p1 );
        heap.read< PointerV >( obj, 0, p2 );
        ASSERT( p2.defined() );
        ASSERT( p2.pointer() );
    }

    TEST( read_2_ptr )
    {
        PointerV p1( vm::CodePointer( 3, 0 ) ), p2;
        heap.write( obj, 0, p1 );
        heap.write( obj, 8, p1 );
        heap.read< PointerV >( obj, 0, p2 );
        ASSERT( p2.defined() );
        ASSERT( p2.pointer() );
        heap.read< PointerV >( obj, 8, p2 );
        ASSERT( p2.defined() );
        ASSERT( p2.pointer() );
    }

    TEST( copy_ptr )
    {
        PointerV p1( vm::CodePointer( 3, 0 ) ), p2;
        ASSERT( p1.pointer() );
        heap.write( obj, 0, p1 );
        heap.copy( obj, 0, obj, 8, 8 );
        heap.read< PointerV >( obj, 8, p2 );
        ASSERT( p2.defined() );
        ASSERT( p2.pointer() );
    }

    TEST( pointers )
    {
        PointerV p1( vm::HeapPointer( 10, 0 ) );
        heap.write( obj, 16, p1 );
        heap.write( obj, 64, p1 );
        heap.write( obj, 80, p1 );
        int count = 0;
        for ( auto x : heap.pointers( obj, 100 ) )
        {
            ++count;
            PointerV p;
            heap.read< PointerV >( obj, x.offset(), p );
            ASSERT( p.defined() );
            ASSERT( p.pointer() );
        }
        ASSERT_EQ( count, 3 );
    }

    TEST( pointers_fragment )
    {
        PointerV p1( vm::HeapPointer( 10, 0 ) );
        heap.write( obj, 0, p1 );
        heap.copy( obj, 0, obj, 11, 8 );
        int count = 0;
        for ( auto x : heap.pointers( obj, 100 ) )
        {
            if ( x.size() != 1 )
                continue;
            ++count;
            ASSERT_EQ( x.fragment(), 10 );
        }
        ASSERT_EQ( count, 4 );
    }

    TEST( read_partially_initialized )
    {
        const uint32_t mask = 0x0AFF;
        vm::value::Int< 16 > i1( 32, mask, false ), i2, i3;
        ASSERT_EQ( i1.defbits(), mask );
        heap.write( obj, 0, i1 );
        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.defbits(), mask );

        heap.write( obj, 2, i1 );
        heap.read( obj, 2, i3 );
        ASSERT_EQ( i3.defbits(), mask );

        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.defbits(), mask );
    }

    TEST( read_truly_partially_initialized )
    {
        const uint32_t mask = 0x0AFE;
        vm::value::Int< 16 > i1( 32, mask, false ), i2, i3;
        ASSERT_EQ( i1.defbits(), mask );
        heap.write( obj, 0, i1 );
        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.defbits(), mask );

        heap.write( obj, 2, i1 );
        heap.read( obj, 2, i3 );
        ASSERT_EQ( i3.defbits(), mask );

        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.defbits(), mask );
    }

    TEST( copy_partially_initialized_short )
    {
        vm::value::Int< 16 > i1( 32, 0x0AFF, false ), i2;
        heap.write( obj, 0, i1 );
        heap.copy( obj, 0, obj, 2, 2 );
        heap.read( obj, 2, i2 );
        ASSERT_EQ( i2.defbits(), 0x0AFF );
    }

    TEST( copy_partially_initialized_long )
    {
        vm::value::Int< 64 > l1( 99, 0xDEADBEEF'0FF0FFFF, false ), l2;
        heap.write( obj, 16, l1 );
        heap.copy( obj, 16, obj, 32, 8 );
        heap.read( obj, 32, l2 );
        ASSERT_EQ( l2.defbits(), 0xDEADBEEF'0FF0FFFF );
    }

    TEST( ptr_unaligned )
    {
        PointerV p1( vm::HeapPointer( 123, 456 ) );
        PointerV p2;

        heap.write( obj, 0, p1 );
        heap.copy( obj, 0, obj, 21, 8 );
        heap.copy( obj, 21, obj, 8, 8 );
        heap.read( obj, 8, p2 );

        ASSERT( p2.pointer() );
        ASSERT_EQ( p2.cooked().object(), 123 );
        ASSERT_EQ( p2.cooked().offset(), 456 );

        heap.read( obj, 20, p2 );
        ASSERT( ! p2.pointer() );

        heap.read( obj, 24, p2 );
        ASSERT( ! p2.pointer() );
    }

    TEST( ptr_break_and_restore )
    {
        PointerV p1( vm::HeapPointer( 123, 42 ) );
        PointerV p2;
        vm::value::Int< 8 > v1( 74 );

        heap.write( obj, 0, p1 );
        heap.copy( obj, 4, obj, 16, 1 );
        heap.copy( obj, 5, obj, 23, 1 );
        heap.copy( obj, 6, obj, 12, 1 );
        heap.copy( obj, 7, obj, 20, 1 );
        heap.write( obj, 0, v1 );
        heap.copy( obj, 0, obj, 21, 2 );

        heap.copy( obj, 0, obj, 5, 1 );
        heap.copy( obj, 23, obj, 5, 1 );

        heap.read( obj, 0, p2 );
        ASSERT( p2.pointer() );
        ASSERT_EQ( p2.cooked().object(), 123 );

        heap.copy( obj, 16, obj, 4, 1 );
        heap.copy( obj, 12, obj, 6, 1 );
        heap.copy( obj, 20, obj, 7, 1 );

        heap.read( obj, 0, p2 );
        ASSERT( p2.pointer() );
        ASSERT_EQ( p2.cooked().object(), 123 );
        ASSERT_EQ( p2.cooked().offset(), 74 );

        heap.read( obj, 16, p2 );
        ASSERT( ! p2.pointer() );

        heap.read( obj, 20, p2 );
        ASSERT( ! p2.pointer() );
    }

    TEST( ptr_incorrect_restore )
    {
        PointerV p1( vm::HeapPointer( 123, 42 ) );
        PointerV p2( vm::HeapPointer( 666, 42 ) );

        heap.write( obj, 0, p1 );
        heap.write( obj, 8, p2 );
        /* dddd ABCD dddd KLMN uuuu uuuu uuuu uuuu */

        heap.copy( obj, 5, obj, 17, 1 );
        heap.copy( obj, 7, obj, 20, 1 );
        /* dddd ABCD dddd KLMN uBuu Duuu uuuu uuuu */

        heap.copy( obj, 13, obj, 5, 1 );
        heap.copy( obj, 15, obj, 7, 1 );
        /* dddd ALCN dddd KLMN uBuu Duuu uuuu uuuu */

        heap.copy( obj, 17, obj, 13, 1 );
        heap.copy( obj, 20, obj, 15, 1 );
        /* dddd ALCN dddd KBMD uBuu Duuu uuuu uuuu */

        heap.read( obj, 0, p1 );
        heap.read( obj, 8, p2 );

        ASSERT( ! p1.pointer() );
        ASSERT( ! p2.pointer() );
    }

    TEST( ptr_copy_invalidate_exception )
    {
        PointerV p1( vm::HeapPointer( 123, 42 ) );
        heap.write( obj, 0, p1 );
        heap.copy( obj, 4, obj, 8, 4 );
        heap.copy( obj, 0, obj, 6, 4 );
        heap.copy( obj, 0, obj, 8, 4 );
        heap.copy( obj, 8, obj, 2, 6 );
        ASSERT( heap._ptr_exceptions->empty() );
    }

    TEST( ptr_write_invalidate_exception )
    {
        PointerV p1( vm::HeapPointer( 123, 42 ) );
        vm::value::Int< 32 > v1( 666 );
        heap.write( obj, 0, p1 );
        heap.write( obj, 8, p1 );
        heap.copy( obj, 0, obj, 4, 2 );
        heap.copy( obj, 0, obj, 12, 2 );

        heap.write( obj, 0, p1 );
        heap.write( obj, 12, v1 );

        ASSERT( heap._ptr_exceptions->empty() );
    }

    TEST( read_write_taint_short )
    {
        vm::value::Int< 16 > i1( 42, 0xFFFF, false ),
                             i2;
        heap.write( obj, 0, i1 );
        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.taints(), 0x00 );

        i1.taints( 0x01 );
        heap.write( obj, 2, i1 );
        heap.read( obj, 2, i2 );
        ASSERT_EQ( i2.taints(), 0x01 );
    }

    TEST( read_write_taint_int )
    {
        vm::value::Int< 32 > i1( 42, 0xFFFF, false ),
                             i2;
        heap.write( obj, 0, i1 );
        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.taints(), 0x00 );

        i1.taints( 0x01 );
        heap.write( obj, 4, i1 );
        heap.read( obj, 4, i2 );
        ASSERT_EQ( i2.taints(), 0x01 );
    }

    TEST( ignore_other_taints )
    {
        vm::value::Int< 16 > i1( 42, 0xFFFF, false ),
                             i2;
        i1.taints( 0xE );
        heap.write( obj, 0, i1 );
        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.taints(), 0x00 );

        i1.taints( 0x7 );
        heap.write( obj, 2, i1 );
        heap.read( obj, 2, i2 );
        ASSERT_EQ( i2.taints(), 0x01 );
    }

    TEST( read_partial_taint )
    {
        vm::value::Int< 16 > i1( 42, 0xFFFF, false );
        vm::value::Int< 32 > i2;
        heap.write( obj, 2, i1 );
        i1.taints( 0x1 );
        heap.write( obj, 0, i1 );
        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.taints(), 0x01 );
    }

    TEST( copy_taint )
    {
        vm::value::Int< 16 > i1( 42, 0xFFFF, false ),
                             i2;
        heap.write( obj, 0, i1 );
        i1.taints( 0x1 );
        heap.write( obj, 2, i1 );
        heap.copy( obj, 0, obj, 4, 4 );
        heap.read( obj, 2, i2 );
        ASSERT_EQ( i2.taints(), 0x01 );
        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.taints(), 0x00 );
    }

    TEST( taint_everywhere )
    {
        vm::value::Int< 32 > i1( 42, 0xFFFFFFFF, false );
        vm::value::Int< 8 > i2;
        i1.taints( 0x1 );

        heap.write( obj, 0, i1 );
        heap.copy( obj, 0, obj, 6, 4 );

        for ( unsigned i = 0; i < 4; ++i )
        {
            i2.taints( 0x0 );
            heap.read( obj, i, i2 );
            ASSERT_EQ( i2.taints(), 0x1 );

            i2.taints( 0x0 );
            heap.read( obj, 6 + i, i2 );
            ASSERT_EQ( i2.taints(), 0x1 );
        }
    }

    TEST( taint_range )
    {
        vm::value::Int< 8 > i1( 42, 0xFF, false );
        i1.taints( 0x1 );

        heap.write( obj, 6, i1 );
        for ( int from = 0; from < 8; ++from )
            for ( int sz = 1; sz < ( (from % 4) ? (4 - from % 4) : 12 ); ++sz )
                ASSERT_EQ( heap.tainted( obj, from, sz ),
                           from <= 6 && from + sz > 6,
                           " [", from, ",", from + sz, ")" );
    }

#if 0
    TEST( copy_aligned_ptr )
    {
        set_pointer( 0 );
        shadow().update( shadow(), 0, 8, 8 );
        check_pointer( 8 / 4 );
    }

    TEST( copy_aligned_2ptr )
    {
        /* pppp pppp uuuu pppp pppp uuuu uuuu uuuu uuuu uuuu */
        set_pointer( 0 );
        set_data( 2, 0 );
        set_pointer( 3 );
        for ( int i = 5; i < 10; ++ i )
            set_data( i, 0 );
        /* pppp pppp uuuu pppp pppp pppp pppp uuuu pppp pppp */
        shadow().update( shadow(), 0, 20, 20 );
        check_pointer( 0 );
        check_data( 2, 0 );
        check_pointer( 3 );
        check_pointer( 5 );
        check_data( 7, 0 );
        check_pointer( 8 );
    }

    TEST( copy_unaligned_bytes )
    {
        /* uddd uuuu uuuu uuuu */
        set_data( 0, 7 );
        shadow().update( shadow(), 0, 11, 4 );
        /* uddd uuuu uuuu dddu */
        check_data( 0, 7 );
        check_data( 1, 0 );
        check_data( 2, 0 );
        check_data( 3, 14 );
    }

    TEST( copy_unaligned_ptr )
    {
        /* uddd pppp pppp uuuu uuuu */
        set_data( 0, 7 );
        set_pointer( 1 );
        set_data( 4, 0 );
        set_data( 5, 0 );
        /* uddd uudd pppp pppp uuuu */
        shadow().update( shadow(), 2, 6, 10 );
        check_data( 0, 7 );
        check_data( 1, 3 );
        check_pointer( 2 );
        check_data( 5, 0 );
    }

    TEST( copy_unaligned_ptr_sp )
    {
        /* uddd pppp pppp uuuu uuuu uuuu */
        set_data( 0, 7 );
        set_pointer( 1 );
        set_data( 4, 0 );
        set_data( 5, 0 );
        set_data( 6, 0 );
        /* uddd pppp pppp uudd pppp pppp */
        shadow().update_slowpath( shadow(), 2, 14, 10 );
        check_data( 0, 7 );
        check_pointer( 1 );
        check_data( 3, 3 );
        check_pointer( 4 );
    }

    TEST( copy_unaligned_ptr_fp )
    {
        /* uddd pppp pppp uuuu uuuu uuuu */
        set_data( 0, 7 );
        set_pointer( 1 );
        set_data( 4, 0 );
        set_data( 5, 0 );
        set_data( 6, 0 );
        /* uddd pppp pppp uudd pppp pppp */
        shadow().update( shadow(), 2, 14, 10 );
        check_data( 0, 7 );
        check_pointer( 1 );
        check_data( 3, 3 );
        check_pointer( 4 );
    }

    TEST( copy_destroy_ptr_1 )
    {
        /* uddd uuuu pppp pppp */
        set_data( 0, 7 );
        set_data( 1, 0 );
        set_pointer( 2 );
        /* uddd uddd uuuu uuuu */
        shadow().update( shadow(), 0, 4, 8 );
        check_data( 0, 7 );
        check_data( 1, 7 );
        check_data( 2, 0 );
        check_data( 3, 0 );
    }

    TEST( copy_destroy_ptr_2 )
    {
        /* uddd uuuu pppp pppp */
        set_data( 0, 7 );
        set_data( 1, 0 );
        set_pointer( 2 );
        /* uddd uuud dduu uuuu */
        shadow().update( shadow(), 0, 6, 8 );
        check_data( 0, 7 );
        check_data( 1, 1 );
        check_data( 2, 12 );
        check_data( 3, 0 );
    }

    TEST( copy_destroy_ptr_3 )
    {
        /* uddd uuuu pppp pppp */
        set_data( 0, 7 );
        set_data( 1, 0 );
        set_pointer( 2 );
        shadow().update( shadow(), 0, 8, 8 );
        /* uddd uuuu uddd uuuu */
        check_data( 0, 7 );
        check_data( 1, 0 );
        check_data( 2, 7 );
        check_data( 3, 0 );
    }

    TEST( copy_brush_ptr_1 )
    {
        /* uddd uuuu pppp pppp */
        set_data( 0, 7 );
        set_data( 1, 0 );
        set_pointer( 2 );
        shadow().update( shadow(), 0, 4, 4 );
        /* uddd uddd pppp pppp */
        check_data( 0, 7 );
        check_data( 1, 7 );
        check_pointer( 2 );
    }

    TEST( copy_brush_ptr_2 )
    {
        /* pppp pppp uuuu uudd */
        set_pointer( 0 );
        set_data( 2, 0 );
        set_data( 3, 3 );
        shadow().update( shadow(), 12, 8, 4 );
        /* pppp pppp uudd uudd */
        check_pointer( 0 );
        check_data( 2, 3 );
        check_data( 3, 3 );
    }

    TEST( copy_brush_ptr_3 )
    {
        /* uudd uuuu pppp pppp */
        set_data( 0, 3 );
        set_data( 1, 0 );
        set_pointer( 2 );
        shadow().update( shadow(), 2, 4, 2 );
        /* uudd dduu pppp pppp */
        check_data( 0, 3 );
        check_data( 1, 12 );
        check_pointer( 2 );
    }

    TEST( copy_brush_ptr_4 )
    {
        /* uudd uuuu uuuu pppp pppp */
        set_data( 0, 3 );
        set_data( 1, 0 );
        set_data( 2, 0 );
        set_pointer( 3 );
        shadow().update( shadow(), 2, 8, 2 );
        /* uudd uuuu dduu pppp pppp */
        check_data( 0, 3 );
        check_data( 1, 0 );
        check_data( 2, 12 );
        check_pointer( 3 );
    }
#endif
};

struct IntervalMetadataMap
{
    using H = TestHeap< mem::ShadowLayers< vm::HeapBase< 8 > > >;
    using Pool = typename H::Pool;
    using Internal = typename H::Internal;
    using IMP = divine::mem::IntervalMetadataMap< int32_t, uint32_t, Pool >;
    using Interval = typename IMP::key_type;

    H heap;
    H::Ptr obj;
    IMP map;

    const auto & _map() const
    {
        ASSERT( map._storage.maps().find( obj ) != map._storage.maps().end() );
        return map._storage.maps().find( obj )->second;
    }
    void _validate() const {
        bool prevSet = false;
        int32_t prev;
        for ( const auto &p : _map() ) {
            ASSERT( p.first.from < p.first.to );
            ASSERT( !prevSet || prev <= p.first.from );
            prev = p.first.to;
            prevSet = true;
        }
    }
    bool _intersect( int32_t from, int32_t to )
    {
        ASSERT( from < to );
        auto next = _map().upper_bound( Interval( from ) );
        if ( next == _map().end() )
        {
            if ( _map().empty() )
                return false;
            if ( from < std::prev( next )->first.to )
                return true;
            return false;
        }
        if ( next == _map().begin() )
        {
            ASSERT( from <= next->first.from );
            return to > next->first.from;
        }
        if ( from < next->first.from && next->first.from < to )
            return true;
        if ( from < std::prev( next )->first.to )
            return true;
        return false;
    }

    void _dump_map( std::string label = "" )
    {
        std::cout << label << " { ";
        for ( const auto &p : _map() )
            std::cout << p.first << ": " << p.second << ", ";
        std::cout << "}\n";
    }

    std::string _serialise()
    {
        std::ostringstream out;
        for ( const auto &p : _map() )
            out << "[" << p.first.from << "," << p.first.to << ")=" << p.second;
        return out.str();
    }

    void check_interval( int32_t peek, int32_t assert_from, int32_t assert_to, uint32_t assert_data )
    {
        auto it = map.at( obj, peek );
        ASSERT( it != nullptr );
        ASSERT_EQ( it->first.from, assert_from );
        ASSERT_EQ( it->first.to, assert_to );
        ASSERT_EQ( it->second, assert_data );
    }

    IntervalMetadataMap() : map( heap._objects )
    {
        obj = heap.make( 100 );
        map._storage.materialise( obj );
        map._storage.maps()[ obj ];
    }

    TEST( snapshot )
    {
        map.insert( obj, 0, 16, 0xAD );
        map._storage.snapshot();

        ASSERT( map._storage._snap(obj) );
        ASSERT( map._storage._snap_range(obj).first );
        ASSERT( map.at(obj, 0) != nullptr );
        ASSERT_EQ( map.at(obj, 0)->second, 0xAD );
        ASSERT_EQ( map.at(obj, 0)->first.from, 0 );
        ASSERT_EQ( map.at(obj, 0)->first.to, 16 );
    }

    TEST( upper_bound )
    {
        map.insert( obj, 0, 15, 0xAD );
        ASSERT( map._storage.upper_bound( obj, Interval( 12 ) ) == map._storage.end( obj ) );

        auto ub = map._storage.upper_bound( obj, Interval( -2 ) );
        ASSERT( ub == map._storage.begin( obj ) );
        ASSERT_EQ( ub->first.from, 0 );
        ASSERT_EQ( ub->first.to, 15 );
        ASSERT_EQ( ub->second, 0xAD );

        ub = map._storage.upper_bound( obj, Interval( 0 ) );
        ASSERT( ub == map._storage.end( obj ) );
        ASSERT( ub != map._storage.begin( obj ) );
        --ub;
        ASSERT( ub == map._storage.begin( obj ) );
        ASSERT_EQ( ub->first.from, 0 );
        ASSERT_EQ( ub->first.to, 15 );
        ASSERT_EQ( ub->second, 0xAD );
    }

    TEST( upper_bound_snap )
    {
        map.insert( obj, 0, 15, 0xAD );
        ASSERT( map._storage.upper_bound( obj, Interval( 12 ) ) == map._storage.end( obj ) );

        map._storage.snapshot();

        auto ub = map._storage.upper_bound( obj, Interval( -2 ) );
        ASSERT( ub == map._storage.begin( obj ) );
        ASSERT_EQ( ub->first.from, 0 );
        ASSERT_EQ( ub->first.to, 15 );
        ASSERT_EQ( ub->second, 0xAD );

        ub = map._storage.upper_bound( obj, Interval( 0 ) );
        ASSERT( ub == map._storage.end( obj ) );
        ASSERT( ub != map._storage.begin( obj ) );
        --ub;
        ASSERT( ub == map._storage.begin( obj ) );
        ASSERT_EQ( ub->first.from, 0 );
        ASSERT_EQ( ub->first.to, 15 );
        ASSERT_EQ( ub->second, 0xAD );
    }

    TEST( disjoint_insert )
    {
        ASSERT( !_intersect( 0, 1 ) );
        map.insert( obj, 0, 1, 0xDA7A1 );
        ASSERT_EQ( _map().size(), 1 );
        _validate();

        // before
        ASSERT( !_intersect( -10, -8 ) );
        map.insert( obj, -10, -8, 0xDA7A2 );
        ASSERT_EQ( _map().size(), 2 );
        _validate();

        // after
        ASSERT( !_intersect( 8, 12 ) );
        map.insert( obj, 8, 12, 0xDA7A3 );
        ASSERT_EQ( _map().size(), 3 );
        _validate();

        // inbetween
        ASSERT( !_intersect( 3, 5 ) );
        map.insert( obj, 3, 5, 0xDA7A4 );
        ASSERT_EQ( _map().size(), 4 );
        _validate();

        // close to, but not overlaping
        ASSERT( !_intersect( -7, -6 ) );
        map.insert( obj, -7, -6, 0xDA7A5 );
        ASSERT_EQ( _map().size(), 5 );
        _validate();

        // close to, but not overlaping
        ASSERT( !_intersect( -3, -1 ) );
        map.insert( obj, -3, 0, 0xDA7A6 );
        ASSERT_EQ( _map().size(), 6 );
        _validate();

        // close to end
        ASSERT( !_intersect( 13, 15 ) );
        map.insert( obj, 13, 15, 0xDA7A7 );
        ASSERT_EQ( _map().size(), 7 );
        _validate();

        ASSERT_EQ( map.at(obj, 0)->second, 0xDA7A1 );
        ASSERT_EQ( map.at(obj, -10)->second, 0xDA7A2 );
        ASSERT_EQ( map.at(obj, -9)->second, 0xDA7A2 );
        ASSERT( map.at(obj, -8) == nullptr );
        ASSERT_EQ( map.at(obj, 9)->second, 0xDA7A3 );
        ASSERT_EQ( map.at(obj, 4)->second, 0xDA7A4 );
        ASSERT_EQ( map.at(obj, -7)->second, 0xDA7A5 );
        ASSERT_EQ( map.at(obj, -1)->second, 0xDA7A6 );
        ASSERT_EQ( map.at(obj, 13)->second, 0xDA7A7 );
        ASSERT( map.at(obj, 15) == nullptr );
        ASSERT( map.at(obj, 20) == nullptr );
        ASSERT( map.at(obj, 7) == nullptr );
        ASSERT( map.at(obj, -20) == nullptr );
    }

    TEST( overlap_inserts ) {
        // no overlap
        ASSERT( !_intersect( 4, 8 ) );
        map.insert( obj, 4, 8, 0xFF01 );
        ASSERT_EQ( _map().size(), 1 );
        _validate();
        ASSERT_EQ( map.at( obj, 5 )->first.from, 4 );
        ASSERT_EQ( map.at( obj, 5 )->first.to, 8 );
        ASSERT_EQ( map.at( obj, 5 )->second, 0xFF01 );
        // [4, 8)

        // no overlap, after end
        ASSERT( !_intersect( 32, 35 ) );
        map.insert( obj, 32, 35, 0xFF02 );
        ASSERT_EQ( _map().size(), 2 );
        _validate();
        ASSERT_EQ( map.at( obj, 34 )->first.from, 32 );
        ASSERT_EQ( map.at( obj, 34 )->first.to, 35 );
        // [4, 8) [32, 35)

        // overlaps with end
        ASSERT( _intersect( 5, 10 ) );
        map.insert( obj, 5, 10 , 0xFF03);
        ASSERT_EQ( _map().size(), 3 );
        _validate();
        ASSERT_EQ( map.at( obj, 5 )->first.from, 5 );
        ASSERT_EQ( map.at( obj, 5 )->first.to, 10 );
        ASSERT_EQ( map.at( obj, 5 )->second, 0xFF03 );
        ASSERT( map.at( obj, 5 ) == map.at( obj, 7 ) );
        ASSERT( map.at( obj, 5 ) == map.at( obj, 9 ) );
        ASSERT( map.at( obj, 10 ) == nullptr );
        ASSERT_EQ( map.at( obj, 4 )->first.from, 4 );
        ASSERT_EQ( map.at( obj, 4 )->first.to, 5 );
        ASSERT_EQ( map.at( obj, 4 )->second, 0xFF01 );
        // [4, 5) [5, 10) [32, 35)

        // begin-aligned
        ASSERT( _intersect( 5, 8 ) );
        map.insert( obj, 5, 8 , 0xFF04);
        ASSERT_EQ( _map().size(), 4 );
        _validate();
        ASSERT_EQ( map.at( obj, 5 )->first.from, 5 );
        ASSERT_EQ( map.at( obj, 5 )->first.to, 8 );
        ASSERT_EQ( map.at( obj, 5 )->second, 0xFF04 );
        ASSERT_EQ( map.at( obj, 8 )->first.from, 8 );
        ASSERT_EQ( map.at( obj, 8 )->first.to, 10 );
        ASSERT_EQ( map.at( obj, 9 )->second, 0xFF03 );
        // [4, 5) [5, 8), [8, 10) [32, 35)

        // complete precise ovelap
        ASSERT( _intersect( 4, 10 ) );
        map.insert( obj, 4, 10 , 0xFF05);
        ASSERT_EQ( _map().size(), 2 );
        _validate();
        ASSERT_EQ( map.at( obj, 5 )->first.from, 4 );
        ASSERT_EQ( map.at( obj, 5 )->first.to, 10 );
        ASSERT_EQ( map.at( obj, 5 )->second, 0xFF05 );
        ASSERT( map.at( obj, 5 ) == map.at( obj, 9 ) );
        // [4, 10) [32, 35)

        // complete overlap, end-aligned
        ASSERT( _intersect( 30, 35 ) );
        map.insert( obj, 30, 35 , 0xFF06);
        ASSERT_EQ( _map().size(), 2 );
        _validate();
        ASSERT_EQ( map.at( obj, 34 )->first.from, 30 );
        ASSERT_EQ( map.at( obj, 34 )->first.to, 35 );
        ASSERT_EQ( map.at( obj, 34 )->second, 0xFF06 );
        // [4, 10) [30, 35)

        // right after end
        ASSERT( !_intersect( 10, 12 ) );
        map.insert( obj, 10, 12 , 0xFF07);
        ASSERT_EQ( _map().size(), 3 );
        _validate();
        ASSERT_EQ( map.at( obj, 5 )->first.from, 4 );
        ASSERT_EQ( map.at( obj, 5 )->first.to, 10 );
        ASSERT_EQ( map.at( obj, 10 )->first.from, 10 );
        ASSERT_EQ( map.at( obj, 10 )->first.to, 12 );
        ASSERT( map.at( obj, 12 ) == nullptr );
        // [4, 10) [10, 12) [30, 35)

        // right before end
        ASSERT( !_intersect( 25, 30 ) );
        map.insert( obj, 25, 30 , 0xFF08);
        ASSERT_EQ( _map().size(), 4 );
        _validate();
        ASSERT_EQ( map.at( obj, 30 )->first.from, 30 );
        ASSERT_EQ( map.at( obj, 30 )->first.to, 35 );
        ASSERT_EQ( map.at( obj, 28 )->first.from, 25 );
        ASSERT_EQ( map.at( obj, 28 )->first.to, 30 );
        ASSERT_EQ( map.at( obj, 30 )->second, 0xFF06 );
        ASSERT_EQ( map.at( obj, 28 )->second, 0xFF08 );
        // [4, 10) [10, 12) [25, 30) [30, 35)

        // inbetween
        ASSERT( !_intersect( 15, 20 ) );
        map.insert( obj, 15, 20 , 0xFF09);
        ASSERT_EQ( _map().size(), 5 );
        _validate();
        ASSERT_EQ( map.at( obj, 16 )->first.from, 15 );
        ASSERT_EQ( map.at( obj, 16 )->first.to, 20 );
        // [4, 10) [10, 12) [15, 20) [25, 30) [30, 35)

        // overlapping two
        ASSERT( _intersect( 11, 16 ) );
        map.insert( obj, 11, 16 , 0xFF0A);
        ASSERT_EQ( _map().size(), 6 );
        _validate();
        ASSERT_EQ( map.at( obj, 10 )->first.from, 10 );
        ASSERT_EQ( map.at( obj, 10 )->first.to, 11);
        ASSERT_EQ( map.at( obj, 14 )->first.from, 11 );
        ASSERT_EQ( map.at( obj, 14 )->first.to, 16);
        ASSERT_EQ( map.at( obj, 16 )->first.from, 16 );
        ASSERT_EQ( map.at( obj, 16 )->first.to, 20);
        // [4, 10) [10, 11) [11, 16) [16, 20) [25, 30) [30, 35)

        // right inbetween
        ASSERT( !_intersect( 20, 25 ) );
        map.insert( obj, 20, 25 , 0xFF0B);
        ASSERT_EQ( _map().size(), 7 );
        _validate();
        ASSERT_EQ( map.at( obj, 16 )->first.from, 16 );
        ASSERT_EQ( map.at( obj, 16 )->first.to, 20);
        ASSERT_EQ( map.at( obj, 20 )->first.from, 20 );
        ASSERT_EQ( map.at( obj, 20 )->first.to, 25 );
        ASSERT_EQ( map.at( obj, 25 )->first.from, 25 );
        ASSERT_EQ( map.at( obj, 25 )->first.to, 30 );
        // [4, 10) [10, 11) [11, 16) [16, 20) [20, 25) [25, 30) [30, 35)

        // no overlap, before first
        ASSERT( !_intersect( -5, -2 ) );
        map.insert( obj, -5, -2 , 0xFF0C);
        ASSERT_EQ( _map().size(), 8 );
        _validate();
        // [-5, -2) [4, 10) [10, 11) [11, 16) [16, 20) [20, 25) [25, 30) [30, 35)

        // spanning multiple succeeding with start overlap
        ASSERT( _intersect( 18, 40 ) );
        map.insert( obj, 18, 40 , 0xFF0D);
        _validate();
        ASSERT_EQ( _map().size(), 6 );
        ASSERT_EQ( map.at( obj, 16 )->first.from, 16 );
        ASSERT_EQ( map.at( obj, 16 )->first.to, 18 );
        ASSERT_EQ( map.at( obj, 30 )->first.from, 18 );
        ASSERT_EQ( map.at( obj, 30 )->first.to, 40 );
        ASSERT( map.at( obj, 18 ) == map.at( obj, 30 ) );
        ASSERT( map.at( obj, 20 ) == map.at( obj, 30 ) );
        ASSERT( map.at( obj, 25 ) == map.at( obj, 30 ) );
        ASSERT( map.at( obj, 35 ) == map.at( obj, 30 ) );
        // [-5, -2) [4, 10) [10, 11) [11, 16) [16, 18) [18, 40)

        // spanning multiple succeeding with end overlap
        ASSERT( _intersect( -10, 14 ) );
        map.insert( obj, -10, 14 , 0xFF0E);
        _validate();
        ASSERT_EQ( _map().size(), 4 );
        ASSERT_EQ( map.at( obj, 4 )->first.from, -10 );
        ASSERT_EQ( map.at( obj, 4 )->first.to, 14 );
        ASSERT( map.at( obj, -5 ) == map.at( obj, 4 ) );
        ASSERT( map.at( obj, -10 ) == map.at( obj, 4 ) );
        ASSERT( map.at( obj, 10 ) == map.at( obj, 4 ) );
        ASSERT( map.at( obj, 11 ) == map.at( obj, 4 ) );
        ASSERT_EQ( map.at( obj, 30 )->first.from, 18 );
        ASSERT_EQ( map.at( obj, 30 )->first.to, 40 );
        // [-10, 14) [14, 16) [16, 18) [18, 40)

        // spanning multiple succeeding with both overlaps
        map.insert( obj, 2, 25 , 0xFF0F);
        ASSERT_EQ( _map().size(), 3 );
        _validate();
        ASSERT_EQ( map.at( obj, 14 )->first.from, 2 );
        ASSERT_EQ( map.at( obj, 14 )->first.to, 25 );
        ASSERT_EQ( map.at( obj, 25 )->first.from, 25 );
        ASSERT_EQ( map.at( obj, 25 )->first.to, 40 );
        ASSERT_EQ( map.at( obj, 0 )->first.from, -10 );
        ASSERT_EQ( map.at( obj, 0 )->first.to, 2 );
        // [-10, 2) [2, 25) [25, 40)

        // splitting the last interval in two
        map.insert( obj, 30, 35 , 0xFF10);
        ASSERT_EQ( _map().size(), 5 );
        _validate();
        ASSERT_EQ( map.at( obj, 27 )->first.from, 25 );
        ASSERT_EQ( map.at( obj, 27 )->first.to, 30 );
        ASSERT_EQ( map.at( obj, 32 )->first.from, 30 );
        ASSERT_EQ( map.at( obj, 32 )->first.to, 35 );
        ASSERT_EQ( map.at( obj, 37 )->first.from, 35 );
        ASSERT_EQ( map.at( obj, 37 )->first.to, 40 );
        ASSERT_EQ( map.at( obj, 27 )->second, 0xFF0D );
        ASSERT_EQ( map.at( obj, 32 )->second, 0xFF10 );
        ASSERT_EQ( map.at( obj, 37 )->second, 0xFF0D );
        // [-10, 2) [2, 25) [25, 30) [30, 35) [35, 40)

        // splitting the first interval in two
        map.insert( obj, -5, 0 , 0xFF11);
        ASSERT_EQ( _map().size(), 7 );
        _validate();
        ASSERT_EQ( map.at( obj, -9 )->first.from, -10 );
        ASSERT_EQ( map.at( obj, -9 )->first.to, -5 );
        ASSERT_EQ( map.at( obj, -4 )->first.from, -5 );
        ASSERT_EQ( map.at( obj, -4 )->first.to, 0 );
        ASSERT_EQ( map.at( obj, 1 )->first.from, 0 );
        ASSERT_EQ( map.at( obj, 1 )->first.to, 2 );
        ASSERT_EQ( map.at( obj, -9 )->second, 0xFF0E );
        ASSERT_EQ( map.at( obj, -4 )->second, 0xFF11 );
        ASSERT_EQ( map.at( obj, 1 )->second, 0xFF0E );
        // [-10, -5) [-5, 0) [0, 2) [2, 25) [25, 30) [30, 35) [35, 40)

        // spanning multiple, aligned begin and end
        map.insert( obj, 0, 35, 0xFF12 );
        _validate();
        ASSERT_EQ( _map().size(), 4 );
        ASSERT_EQ( map.at( obj, -1 )->first.from, -5 );
        ASSERT_EQ( map.at( obj, -1 )->first.to, 0 );
        ASSERT_EQ( map.at( obj, -1 )->second, 0xFF11 );
        ASSERT_EQ( map.at( obj, 0 )->first.from, 0 );
        ASSERT_EQ( map.at( obj, 0 )->first.to, 35 );
        ASSERT_EQ( map.at( obj, 0 )->second, 0xFF12 );
        ASSERT( map.at( obj, 0 ) == map.at( obj, 2 ) );
        ASSERT( map.at( obj, 0 ) == map.at( obj, 25 ) );
        ASSERT( map.at( obj, 0 ) == map.at( obj, 33 ) );
        ASSERT_EQ( map.at( obj, 35 )->first.from, 35 );
        ASSERT_EQ( map.at( obj, 35 )->first.to, 40 );
        ASSERT_EQ( map.at( obj, 35 )->second, 0xFF0D );
        // [-10, -5) [-5, 0) [0, 35) [35, 40)
    }

    TEST( overlap_insert2 )
    {
        map.insert( obj, 10, 20, 0xFE01 );
        _validate();
        ASSERT_EQ( _map().size(), 1 );
        ASSERT_EQ( map.at( obj, 15 )->second, 0xFE01 );
        // [10, 20)

        // no overlap, before the only entry
        map.insert( obj, 0, 5, 0xFE02 );
        _validate();
        ASSERT_EQ( _map().size(), 2 );
        ASSERT_EQ( map.at( obj, 15 )->second, 0xFE01 );
        ASSERT_EQ( map.at( obj, 1 )->second, 0xFE02 );
        // [0, 5) [10, 20)

        // no overlap, before the first entry
        map.insert( obj, -10, -5, 0xFE03 );
        _validate();
        ASSERT_EQ( _map().size(), 3 );
        // [-10, -5) [0, 5) [10, 20)

        // begin between, one destroyed, end overlaps
        map.insert( obj, -2, 15, 0xFE04 );
        _validate();
        ASSERT_EQ( _map().size(), 3 );
        ASSERT_EQ( map.at( obj, 15 )->second, 0xFE01 );
        ASSERT_EQ( map.at( obj, 0 )->second, 0xFE04 );
        // [-10, -5) [-2, 15) [15, 20)

        // no overlap, after last
        map.insert( obj, 30, 40, 0xFE05 );
        _validate();
        ASSERT_EQ( _map().size(), 4 );
        // [-10, -5) [-2, 15) [15, 20) [30, 40)

        // begin overlaps, two destroyed, end between
        map.insert( obj, -7, 25, 0xFE06 );
        _validate();
        ASSERT_EQ( _map().size(), 3 );
        ASSERT_EQ( map.at( obj, 15 )->second, 0xFE06 );
        ASSERT_EQ( map.at( obj, 0 )->second, 0xFE06 );
        ASSERT_EQ( map.at( obj, -9 )->second, 0xFE03 );
        ASSERT( map.at( obj, 25 ) == nullptr );
        // [-10, -7) [-7, 25) [30, 40)

        // everything destroyed
        map.insert( obj, -20, 50, 0xFE07 );
        _validate();
        ASSERT_EQ( _map().size(), 1 );
        ASSERT_EQ( map.at( obj, -10 )->second, 0xFE07 );
        ASSERT_EQ( map.at( obj, 25 )->second, 0xFE07 );
        ASSERT_EQ( map.at( obj, 40 )->second, 0xFE07 );
        // [-20, 50)
    }

    TEST( copy_one_whole_to_whole )
    {
        uint32_t datum = 0xFE01;
        auto src = map.insert( obj, 10, 15, datum );

        // "tight" source
        map.copy( map, obj, 10, obj, 100, 5 );
        check_interval( 102, 100, 105, datum );
        src->second = ++datum;
        ASSERT_EQ( _map().size(), 2 );

        // source with "loose" end
        map.copy( map, obj, 9, obj, 49, 9 );
        check_interval( 52, 50, 55, datum );
        ASSERT( map.at( obj, 16 ) == nullptr );
        ASSERT( map.at( obj, 57 ) == nullptr );
        ASSERT( map.at( obj, 55 ) == nullptr );
        src->second = ++datum;
        ASSERT_EQ( _map().size(), 3 );

        // source with "loose" beginning
        map.copy( map, obj, 8, obj, 18, 7 );
        check_interval( 22, 20, 25, datum );
        ASSERT( map.at( obj, 19 ) == nullptr );
        src->second = ++datum;
        ASSERT_EQ( _map().size(), 4 );

        // "tight" destination
        map.copy( map, obj, 10, obj, 15, 5 );
        check_interval( 16, 15, 20, datum );
        src->second = ++datum;
        ASSERT_EQ( _map().size(), 5 );

        map.copy( map, obj, 15, obj, 0, 5 );
        check_interval( 2, 0, 5, datum - 1 );
        src->second = ++datum;
        ASSERT_EQ( _map().size(), 6 );

        map.copy( map, obj, 10, obj, 5, 5 );
        check_interval( 7, 5, 10, datum );
        src->second = ++datum;
        ASSERT_EQ( _map().size(), 7 );
    }

    TEST( copy_more_whole_to_whole )
    {
        uint32_t datum = 0xFE01;
        auto src = map.insert( obj, 10, 15, datum );

        map.copy( map, obj, 10, obj, 15, 5 );
        src->second = ++datum;

        map.copy( map, obj, 10, obj, 0, 10 );
        src->second = ++datum;

        map.copy( map, obj, 0, obj, 20, 20 );
        src->second = ++datum;

        ASSERT_EQ( _map().size(), 8 );
        int i = 0;
        for ( uint32_t n : { 2, 1, 4, 1, 2, 1, 3, 1 } )
        {
            check_interval( i * 5 + 2, i * 5, i * 5 + 5, 0xFE00 + n );
            ++i;
        }
    }

    TEST( chomp_right_by_copy )
    {
        uint32_t datum = 0xFE01;
        auto src = map.insert( obj, 0, 5, datum );
        map.insert( obj, 10, 30, 0xAAAA );

        // ends aligned
        map.copy( map, obj, 0, obj, 25, 5 );
        ASSERT_EQ( _map().size(), 3 );
        check_interval( 28, 25, 30, datum );
        check_interval( 24, 10, 25, 0xAAAA );
        src->second = ++datum;

        // ends unaligned
        map.copy( map, obj, 0, obj, 28, 5 );
        ASSERT_EQ( _map().size(), 4 );
        check_interval( 29, 28, 33, datum );
        check_interval( 27, 25, 28, datum - 1 );
        check_interval( 24, 10, 25, 0xAAAA );
        src->second = ++datum;

        // erase by copy
        map.copy( map, obj, 50, obj, 20, 20 );
        ASSERT( map.at( obj, 20 ) == nullptr );
        ASSERT( map.at( obj, 26 ) == nullptr );
        ASSERT( map.at( obj, 30 ) == nullptr );
        ASSERT_EQ( _map().size(), 2 );
        check_interval( 15, 10, 20, 0xAAAA );

        map.copy( map, obj, -1, obj, 13, 8 );
        check_interval( 10, 10, 13, 0xAAAA );
        ASSERT( map.at( obj, 13 ) == nullptr );
        check_interval( 14, 14, 19, datum );
        ASSERT( map.at( obj, 19 ) == nullptr );
        ASSERT( map.at( obj, 20 ) == nullptr );
        ASSERT_EQ( _map().size(), 3 );
    }

    TEST( chomp_left_by_copy )
    {
        uint32_t datum = 0xFE01;
        map.insert( obj, 10, 30, 0xAAAA );
        auto src = map.insert( obj, 0, 5, datum );

        // begins aligned
        map.copy( map, obj, 0, obj, 10, 5 );
        ASSERT_EQ( _map().size(), 3 );
        check_interval( 11, 10, 15, datum );
        check_interval( 20, 15, 30, 0xAAAA );
        src->second = ++datum;

        // ends unaligned
        map.copy( map, obj, 0, obj, 8, 5 );
        ASSERT_EQ( _map().size(), 4 );
        check_interval( 9, 8, 13, datum );
        check_interval( 13, 13, 15, datum - 1 );
        check_interval( 20, 15, 30, 0xAAAA );
        src->second = ++datum;

        // erase by copy
        map.copy( map, obj, 50, obj, 5, 15 );
        ASSERT( map.at( obj, 5 ) == nullptr );
        ASSERT( map.at( obj, 9 ) == nullptr );
        ASSERT( map.at( obj, 13 ) == nullptr );
        ASSERT( map.at( obj, 19 ) == nullptr );
        ASSERT_EQ( _map().size(), 2 );
        check_interval( 20, 20, 30, 0xAAAA );

        map.copy( map, obj, -2, obj, 19, 8 );
        ASSERT( map.at( obj, 19 ) == nullptr );
        ASSERT( map.at( obj, 20 ) == nullptr );
        check_interval( 22, 21, 26, datum );
        ASSERT( map.at( obj, 26 ) == nullptr );
        check_interval( 29, 27, 30, 0xAAAA );
        ASSERT_EQ( _map().size(), 3 );
    }

    TEST( split_by_copy ) {
        uint32_t datum = 0xFE01;
        map.insert( obj, 10, 50, 0xAAAA );
        auto src = map.insert( obj, 0, 5, datum );

        // tight
        map.copy( map, obj, 0, obj, 12, 5 );
        check_interval( 11, 10, 12, 0xAAAA );
        check_interval( 12, 12, 17, datum );
        check_interval( 20, 17, 50, 0xAAAA );
        ASSERT_EQ( _map().size(), 4 );
        src->second = ++datum;

        // loose
        map.copy( map, obj, -2, obj, 20, 9 );
        check_interval( 11, 10, 12, 0xAAAA );
        check_interval( 12, 12, 17, datum - 1);
        check_interval( 18, 17, 20, 0xAAAA );
        ASSERT( map.at( obj, 20 ) == nullptr );
        ASSERT( map.at( obj, 21 ) == nullptr );
        check_interval( 22, 22, 27, datum );
        ASSERT( map.at( obj, 27 ) == nullptr );
        ASSERT( map.at( obj, 28 ) == nullptr );
        check_interval( 30, 29, 50, 0xAAAA );
        ASSERT_EQ( _map().size(), 6 );
    }

    TEST( copy_partial )
    {
        uint32_t datum = 101;
        auto src = map.insert( obj, 4, 8, datum );
        ASSERT_EQ( "[4,8)=101", _serialise() );

        // right tight
        map.copy( map, obj, 6, obj, 12, 2 );
        ASSERT_EQ( "[4,8)=101[12,14)=101", _serialise() );
        src->second = ++datum;

        // right loose
        map.copy( map, obj, 13, obj, 8, 2 );
        ASSERT_EQ( "[4,8)=102[8,9)=101[12,14)=101", _serialise() );

        // left tight
        map.copy( map, obj, 4, obj, 2, 2 );
        ASSERT_EQ( "[2,4)=102[4,8)=102[8,9)=101[12,14)=101", _serialise() );
        src->second = ++datum;

        // left loose
        map.copy( map, obj, 1, obj, 10, 2 );
        ASSERT_EQ( "[2,4)=102[4,8)=103[8,9)=101[11,12)=102[12,14)=101", _serialise() );

        // both
        map.copy( map, obj, 6, obj, 16, 3 );
        ASSERT_EQ( "[2,4)=102[4,8)=103[8,9)=101[11,12)=102[12,14)=101[16,18)=103[18,19)=101",
                   _serialise() );
        src->second = ++datum;

        // inner
        map.copy( map, obj, 5, obj, 20, 2 );
        ASSERT_EQ( "[2,4)=102[4,8)=104[8,9)=101[11,12)=102[12,14)=101[16,18)=103[18,19)=101[20,22)=104",
                   _serialise() );
    }

    TEST( copy_partial_with_neighbour )
    {
        map.insert( obj, 10, 20, 42 );
        map.insert( obj, 22, 24, 666 );
        map.insert( obj, 6, 8, 66 );
        ASSERT_EQ( "[6,8)=66[10,20)=42[22,24)=666", _serialise() );

        // right
        map.copy( map, obj, 15, obj, 40, 7 );
        ASSERT_EQ( "[6,8)=66[10,20)=42[22,24)=666[40,45)=42", _serialise() );

        // left
        map.copy( map, obj, 8, obj, 50, 7 );
        ASSERT_EQ( "[6,8)=66[10,20)=42[22,24)=666[40,45)=42[52,57)=42", _serialise() );
    }
};

}
