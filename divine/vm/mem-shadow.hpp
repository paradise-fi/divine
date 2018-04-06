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
#include <divine/vm/mem-base.hpp>
#include <divine/vm/mem-sandwich.hpp>
#include <divine/vm/mem-pointers.hpp>
#include <divine/vm/mem-definedness.hpp>
#include <divine/vm/mem-taint.hpp>

namespace divine {
namespace vm::mem {

namespace bitlevel = brick::bitlevel;

union ExpandedMeta // Representation the shadow layers operate on
{
    struct
    {
        uint16_t taint : 4,
                 pointer_type : 3,
                 pointer : 1,
                 pointer_exception : 1,
                 data_exception : 1,
                 _free_ : 2,
                 defined : 4;
    };
    uint16_t _raw;
    constexpr ExpandedMeta() : _raw( 0 ) {}
    constexpr ExpandedMeta( uint16_t raw ) : _raw( raw ) {}
    operator uint16_t() const { return _raw; }
};

/* Descriptor of sandwich shadow with a pointer layer, a definedness layer and a taint layer. */
template< typename Next >
struct Compress : Next
{
    // 16 bits:  | _ , _ , _ , _ : _ , _ , _ , _ | _ , _ , _ , _ : _ , _ , _ , _ |
    // Expanded: | [definedness] : _ , _ , DE, PE| P , [ ptype ] : [ t a i n t ] |

    // 8 bits : | _ , _ , _ , _ : _ , _ , _ , _ |
    // Pointer: | 1 , [ ptype ] : [ t a i n t ] |
    // Data:    | 0 , [   0000000 - 1010000   ] | 81 values for definedness + taint
    // Def exc: | 0 , 1 , 1 , 0 : [ t a i n t ] |
    // Ptr exc: | 0 , 1 , 1 , 1 : [ t a i n t ] |
    // (free):  | 0 , 1 , 0 , 1 : [0001 - 1111] | 15 available codes (currently ignored)

    // In [definedness] and [taint], less significant bits correspond to bytes on lower addresses.

    static constexpr unsigned BitsPerWord = 8;

    using Compressed = uint8_t; // Representation stored in the pool
    using Expanded = mem::ExpandedMeta;

    constexpr static Compressed compress( Expanded exp )
    {
        if ( exp.pointer )
            return exp._raw;

        if ( exp.data_exception )
            return ( ( exp._raw & 0x0300 ) >> 4) | 0x40 | exp.taint;

        uint8_t def = exp.defined,
                taint = exp.taint,
                dt = 0;
        for ( int i = 0; i < 4; ++i ) {

            dt *= 3;
            dt += ( def & 0x1 ) + ( taint & def & 0x1 );
            def >>= 1;
            taint >>= 1;
        }

        return dt;
    }

    constexpr static Expanded expand( Compressed c )
    {
        Expanded ec;
        ec._raw = c;

        if ( c & 0x80 ) // pointer
            return ec._raw | 0xF000;

        if ( ( c & 0x60 ) == 0x60 ) // pointer or definedness exception
            return ( ec._raw | ( ec._raw << 4 ) ) & 0x030F;

        // Data (undef - def - tainted)
        uint8_t def = 0,
                taint = 0;
        for ( int i = 0; i < 4; ++i ) {
            def <<= 1;
            taint <<= 1;
            uint8_t dt = c % 3;
            def |= dt & 0x1;
            taint |= dt >> 1;
            c /= 3;
        }
        def |= taint;

        return ( def << 12 ) | taint;
    }

    // Shall be true if 'c' encodes all metadata (i.e. is not an exception)
    constexpr static bool is_trivial( Compressed c )
    {
        return ( c & 0x60 ) != 0x60;
    }

    // These predicates exist in order to avoid expanding compressed data when searching for
    // pointers.
    constexpr static bool is_pointer( Compressed c )
    {
        return c & 0x80;
    }
    constexpr static bool is_pointer_or_exception( Compressed c )
    {
        return is_pointer( c ) || is_pointer_exception( c );
    }
    constexpr static bool is_pointer_exception( Compressed c )
    {
        return ( c & 0xF0 ) == 0x70;
    }
};

template< typename Pool >
using Layers = TaintLayer<
               DefinednessLayer<
               PointerLayer<
               ShadowBase<
               Compress<
               Base< Pool > > > > > >;

template< typename MasterPool >
using CompoundShadow = SandwichShadow< MasterPool, Layers< MasterPool > >;

}

namespace t_vm {

struct CompoundShadowDescriptor
{
    struct Empty {};
    using ShDesc = vm::mem::Compress< Empty >;

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
        exp.pointer_type = 1;
        ASSERT_EQ( exp, 0x0090 );
        exp.taint = 3;
        ASSERT_EQ( exp, 0x0093 );
        exp.defined = 0xC;
        ASSERT_EQ( exp, 0xC093 );
        exp.data_exception = true;
        ASSERT_EQ( exp, 0xC293 );
        exp.pointer_exception = true;
        ASSERT_EQ( exp, 0xC393 );
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
            for ( uint8_t ptype = 0; ptype < 8; ++ptype )
            {
                ShDesc::Expanded exp;
                exp.taint = taint;
                exp.defined = 0xF;
                exp.pointer = true;
                exp.pointer_type = ptype;
                auto reexp = ShDesc::expand( ShDesc::compress( exp ) );
                ASSERT_EQ( exp._raw, reexp._raw );
            }

            ShDesc::Expanded exp;
            exp.taint = taint;

            exp.data_exception = true;
            auto reexp = ShDesc::expand( ShDesc::compress( exp ) );
            ASSERT_EQ( exp._raw, reexp._raw );

            exp.pointer_exception = true;
            reexp = ShDesc::expand( ShDesc::compress( exp ) );
            ASSERT_EQ( exp._raw, reexp._raw );
        }
    }
};

using Pool = brick::mem::Pool<>;

template< template< typename > class Shadow >
struct TestHeap
{
    using Ptr = Pool::Pointer;
    Pool pool;
    Shadow< Pool > shadows;
    using Loc = typename Shadow< Pool >::Loc;

    TestHeap() : shadows( pool ) {}

    auto pointers( Ptr p, int sz ) { return shadows.pointers( shloc( p, 0 ), sz ); }
    Loc shloc( Ptr p, int off ) { return Loc( p, off ); }

    Ptr make( int sz )
    {
        auto r = pool.allocate( sz );
        shadows.make( r, sz );
        return r;
    }

    template< typename T >
    void write( Ptr p, int off, T t ) {
        shadows.write( shloc( p, off ), t );
        *pool.template machinePointer< typename T::Raw >( p, off ) = t.raw();
    }

    template< typename T >
    void read( Ptr p, int off, T &t ) {
        t.raw( *pool.template machinePointer< typename T::Raw >( p, off ) );
        shadows.read( shloc( p, off ), t );
    }

    void copy( Ptr pf, int of, Ptr pt, int ot, int sz )
    {
        auto data_from = pool.template machinePointer< uint8_t >( pf, of ),
             data_to   = pool.template machinePointer< uint8_t >( pt, ot );
        shadows.copy( shloc( pf, of ), shloc( pt, ot ), sz, [this]( auto i, auto o ){
                return *pool.template machinePointer< uint32_t >( i, o ); } );
        std::copy( data_from, data_from + sz, data_to );
    }
};

struct CompoundShadow
{
    using PointerV = vm::value::Pointer;
    using H = TestHeap< vm::mem::CompoundShadow >;
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
        PointerV p1( vm::nullPointer() ), p2;
        heap.write( obj, 0, p1 );
        heap.read< PointerV >( obj, 0, p2 );
        ASSERT( p2.defined() );
        ASSERT( p2.pointer() );
    }

    TEST( read_2_ptr )
    {
        PointerV p1( vm::nullPointer() ), p2;
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
        PointerV p1( vm::nullPointer() ), p2;
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
        heap.write( obj, 92, p1 );
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
        ASSERT( heap.shadows._layers._ptr_exceptions->empty() );
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

        ASSERT( heap.shadows._layers._ptr_exceptions->empty() );
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

}

}
