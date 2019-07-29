// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
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
#include <brick-bitlevel>

namespace divine::mem
{

namespace bitlevel = brick::bitlevel;

/*
 * Shadow layer for tracking a taint bit (the least significant taint bit in vm::Value).
 *
 * Required fields in Expanded:
 *      taint : 4 (less significant bits correspond to bytes on lower addresses)
 */

template< typename NextLayer >
struct TaintLayer : public NextLayer
{
    using Internal = typename NextLayer::Internal;
    using Loc = typename NextLayer::Loc;
    using Expanded = typename NextLayer::Expanded;

    template< typename OtherSh >
    int compare_word( OtherSh &a_sh, typename OtherSh::Loc a, Expanded exp_a, Loc b, Expanded exp_b,
                      bool skip_objids )
    {
        // Due to the nature of compressed metadata comparison, the expanded forms never differ in
        // taint bits. While efficient, it is, however, implementation-dependent, hence this assert,
        // which will fire if the assumption ever ceases to hold.
        ASSERT_EQ( exp_a.taint, exp_b.taint );

        return NextLayer::compare_word( a_sh, a, exp_a, b, exp_b, skip_objids );
    }

    // Stores taints from value to expanded form of metadata 'exp' from memory location 'l'.
    template< typename V >
    void write( Loc l, V value, Expanded *exp )
    {
        NextLayer::write( l, value, exp );

        const int sz = value.size();
        int off = 0;
        int w = 0;

        if( sz >= 4 )
            for ( ; off < bitlevel::downalign( sz, 4 ); off += 4, ++w )
                exp[ w ].taint = ( value.taints() & 0x1 ) ? 0xF : 0x0;

        if ( sz % 4 )
        {
            uint8_t mask = 0x1 << ( l.offset + off ) % 4;
            for ( ; off < sz; ++off )
            {
                if ( value.taints() & 0x01 )
                    exp[ w ].taint |= mask;
                else
                    exp[ w ].taint &= ~mask;
                mask <<= 1;
            }
        }
    }

    // Loads taints from expanded form to internal value
    template< typename V >
    void read( Loc l, V &value, Expanded *exp ) const
    {
        const int sz = value.size();

        int off = 0;
        int w = 0;

        auto newtaints = value.taints() & ~0x1;

        if( sz >= 4 )
            for ( ; off < bitlevel::downalign( sz, 4 ); off += 4, ++w )
                if ( exp[ w ].taint )
                    newtaints |= 0x1;

        if ( sz % 4 )
        {
            uint8_t mask = bitlevel::ones< uint8_t >( sz % 4 ) << ( l.offset + off ) % 4;
            if ( exp[ w ].taint & mask )
                    newtaints |= 0x1;
        }

        value.taints( newtaints );

        NextLayer::read( l, value, exp );
    }

    // merge taints  of source and destination values
    template< typename FromH, typename ToH >
    static void copy_byte( FromH &from_h, ToH &to_h, typename FromH::Loc from, const Expanded &exp_src,
                           Loc to, Expanded &exp_dst )
    {
        NextLayer::copy_byte( from_h, to_h, from, exp_src, to, exp_dst );

        uint8_t src_tainted = ( exp_src.taint >> from.offset % 4 ) & 0x1;
        exp_dst.taint &= ~( 0x1 << to.offset % 4 );
        exp_dst.taint |= src_tainted << to.offset % 4;
    }
};

#if 0
/* Additional layer of taint bits. Not sandwichable. */
template< template < typename > class SlaveShadow, typename MasterPool,
          uint8_t TaintBits = 1, uint8_t LSB = 0 >
struct PooledTaintShadow : public SlaveShadow< MasterPool >
{
    using NextShadow = SlaveShadow< MasterPool >;
    using Pool = brick::mem::SlavePool< MasterPool >;
    using Internal = typename Pool::Pointer;
    using Loc = InternalLoc< Internal >;
    using TaintC = BitsetContainer< TaintBits, Pool >;
    using Type = typename TaintC::Type;
    static constexpr Type Mask = bitlevel::ones< Type >( TaintBits );

    Pool _taints;

    PooledTaintShadow( const MasterPool &mp )
        : NextShadow( mp )
        , _taints( mp ) {}

    void make( Internal p, int size )
    {
        _taints.materialise( p, ( size * TaintBits / 8 )  + ( size * TaintBits % 8 ? 1 : 0 ));
        NextShadow::make( p, size );
    }

    auto taints( Loc l, int sz )
    {
        return TaintC( _taints, l.object, l.offset, l.offset + sz );
    }

    bool equal( Internal a, Internal b, int sz )
    {
        return compare( *this, a, b, sz ) == 0;
    }

    template< typename OtherSH >
    int compare( OtherSH & a_sh, typename OtherSH::Internal a, Internal b, int sz )
    {
        int cmp = NextShadow::compare( a_sh, a, b, sz );
        if ( cmp )
            return cmp;

        auto a_ts = a_sh.taints( a, sz );
        auto b_it = taints( b, sz ).begin();
        for ( auto a_t : a_ts )
            if ( ( cmp = a_t - *b_it++ ) )
                return cmp;

        return 0;
    }

    template< typename V >
    void write( Loc l, V value )
    {
        const int sz = value.size();
        for ( auto t : taints( l, sz ) )
            t.set( ( value.taints() >> LSB ) & Mask );

        NextShadow::write( l, value );
    }

    template< typename V >
    void read( Loc l, V &value )
    {
        const int sz = value.size();
        using VT = decltype( value.taints() );
        VT newtaints = 0;
        for ( auto t : taints( l, sz ) )
            newtaints |= t;

        newtaints <<= LSB;
        newtaints |= value.taints() & ~( VT( Mask ) << LSB );
        value.taints( newtaints );

        NextShadow::read( l, value );
    }

    template< typename FromSh, typename FromHeapReader, typename ToHeapReader >
    void copy( FromSh &from_sh, typename FromSh::Loc from, Loc to, int sz,
               FromHeapReader fhr, ToHeapReader thr )
    {
        if ( sz == 0 )
            return;

        ASSERT_LT( 0, sz );

        auto from_ts = from_sh.taints( from, sz );
        auto to_ts = taints( to, sz );
        std::copy( from_ts.begin(), from_ts.end(), to_ts.begin() );

        NextShadow::copy( from_sh, from, to, sz, fhr, thr );
    }

    template< typename HeapReader >
    void copy( Loc from, Loc to, int sz, HeapReader hr ) {
        copy( *this, from, to, sz, hr, hr );
    }

};
#endif

}

#if 0
namespace t_vm {

using Pool = brick::mem::Pool<>;

template< template< typename > class Shadow >
struct NonHeap
{
    using Ptr = Pool::Pointer;
    Pool pool;
    Shadow< Pool > shadows;
    using Loc = typename Shadow< Pool >::Loc;

    NonHeap() : shadows( pool ) {}

    auto pointers( Ptr p, int sz ) { return shadows.pointers( shloc( p, 0 ), sz ); }
    Loc shloc( Ptr p, int off ) { return Loc( p, off ); }

    Ptr make( int sz )
    {
        auto r = pool.allocate( 1 );
        shadows.make( r, sz );
        return r;
    }

    template< typename T >
    void write( Ptr p, int off, T t ) {
        shadows.write( shloc( p, off ), t );
    }

    template< typename T >
    void read( Ptr p, int off, T &t ) {
        shadows.read( shloc( p, off ), t );
    }

    void copy( Ptr pf, int of, Ptr pt, int ot, int sz )
    {
        shadows.copy( shloc( pf, of ), shloc( pt, ot ), sz, [this]( auto, auto ){
                return 0; } );
    }
};

template< typename MasterPool >
struct NonShadow
{
    using Internal = typename Pool::Pointer;
    using Loc = mem::InternalLoc< Internal >;

    NonShadow( const MasterPool & ) {}
    void make( Internal, int ) {}
    void free( Internal ) {}
    template< typename OtherSH >
    int compare( OtherSH &, typename OtherSH::Internal, Internal, int ) { return 0; }
    template< typename V >
    void write( Loc, V ) {}
    template< typename V >
    void read( Loc, V & ) {}
    template< typename FromSh, typename FromHeapReader, typename ToHeapReader >
    void copy( FromSh &, typename FromSh::Loc, Loc, int, FromHeapReader, ToHeapReader ) {}
};

struct PooledTaintShadow
{
    template< uint8_t S >
    using Int = vm::value::Int< S >;
    template< typename MasterPool >
    using Sh = mem::PooledTaintShadow< NonShadow, MasterPool, 1, 1 >;
    using H = NonHeap< Sh >;
    H heap;
    H::Ptr obj;

    PooledTaintShadow() { obj = heap.make( 100 ); }

    TEST( read_write )
    {
        Int< 16 > i1( 42, 0xFFFF, false ),
                  i2;
        heap.write( obj, 0, i1 );
        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.taints(), 0x00 );

        i1.taints( 0x02 );
        heap.write( obj, 2, i1 );
        heap.read( obj, 2, i2 );
        ASSERT_EQ( i2.taints(), 0x02 );
    }

    TEST( ignore_other_taints )
    {
        Int< 16 > i1( 42, 0xFFFF, false ),
                  i2;
        i1.taints( 0x7 );
        heap.write( obj, 2, i1 );
        heap.read( obj, 2, i2 );
        ASSERT_EQ( i2.taints(), 0x02 );
    }

    TEST( read_partial )
    {
        Int< 16 > i1( 42, 0xFFFF, false );
        Int< 32 > i2;
        heap.write( obj, 2, i1 );
        i1.taints( 0x2 );
        heap.write( obj, 0, i1 );
        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.taints(), 0x02 );
    }

    TEST( copy )
    {
        Int< 16 > i1( 42, 0xFFFF, false ),
                  i2;
        heap.write( obj, 0, i1 );
        i1.taints( 0x2 );
        heap.write( obj, 2, i1 );
        heap.copy( obj, 0, obj, 4, 4 );
        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.taints(), 0x00 );
        heap.read( obj, 2, i2 );
        ASSERT_EQ( i2.taints(), 0x02 );
    }

    TEST( iterators )
    {
        Int< 16 > i1( 42, 0xFFFF, false );
        heap.write( obj, 0, i1 );
        i1.taints( 0x2 );
        heap.write( obj, 2, i1 );
        int i = 0;
        for ( auto t : heap.shadows.taints( obj, 6 ) )
        {
            uint8_t expected = ( i % 4 < 2) ? 0x00 : 0x01;
            ASSERT_EQ( t, expected );
            ++i;
        }
    }

};

struct PooledTaintShadow4bit
{
    template< uint8_t S >
    using Int = vm::value::Int< S >;
    template< typename MasterPool >
    using Sh = mem::PooledTaintShadow< NonShadow, MasterPool, 4, 1 >;
    using H = NonHeap< Sh >;
    H heap;
    H::Ptr obj;

    PooledTaintShadow4bit() { obj = heap.make( 100 ); }

    TEST( read_write )
    {
        Int< 16 > i1( 42, 0xFFFF, false ),
                  i2;
        heap.write( obj, 0, i1 );
        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.taints(), 0x00 );

        i1.taints( 0x1A );
        heap.write( obj, 2, i1 );
        heap.read( obj, 2, i2 );
        ASSERT_EQ( i2.taints(), 0x1A );
    }

    TEST( ignore_other_taints )
    {
        Int< 16 > i1( 42, 0xFFFF, false ),
                  i2;
        i1.taints( 0x1F );
        heap.write( obj, 2, i1 );
        heap.read( obj, 2, i2 );
        ASSERT_EQ( i2.taints(), 0x1E );
    }

    TEST( read_partial )
    {
        Int< 16 > i1( 42, 0xFFFF, false );
        Int< 32 > i2;
        heap.write( obj, 2, i1 );
        i1.taints( 0x1A );
        heap.write( obj, 0, i1 );
        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.taints(), 0x1A );
    }

    TEST( read_heterogeneous )
    {
        Int< 16 > i1( 42, 0xFFFF, false );
        Int< 32 > i2;
        i1.taints( 0x18 );
        heap.write( obj, 2, i1 );
        i1.taints( 0x02 );
        heap.write( obj, 0, i1 );
        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.taints(), 0x1A );
    }

    TEST( copy )
    {
        Int< 16 > i1( 42, 0xFFFF, false ),
                  i2;
        heap.write( obj, 0, i1 );
        i1.taints( 0x1A );
        heap.write( obj, 2, i1 );
        heap.copy( obj, 0, obj, 4, 4 );
        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.taints(), 0x00 );
        heap.read( obj, 2, i2 );
        ASSERT_EQ( i2.taints(), 0x1A );
    }

    TEST( iterators )
    {
        Int< 16 > i1( 42, 0xFFFF, false );
        heap.write( obj, 0, i1 );
        i1.taints( 0x1A );
        heap.write( obj, 2, i1 );
        int i = 0;
        for ( auto t : heap.shadows.taints( obj, 6 ) )
        {
            uint8_t expected = ( i % 4 < 2) ? 0x00 : 0x0D;
            ASSERT_EQ( t, expected );
            ++i;
        }
    }

};

}
#endif
