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

#pragma once

#include <brick-types>
#include <utility>

#include <divine/vm/value.hpp>

namespace divine {
namespace vm {

namespace bitlevel = brick::bitlevel;

/*
 * ShadowByte structure
 * - 1 bit: is this an exception? if so, remaining 7 bits are exception id
 * - 1 bit: Data vs Pointer
 * - for Data: one bit per byte of 'initialised' status
 * - for Pointer: 2 bits of offset init'd
 */

union ShadowByte
{
    struct {
        /* unaligned pointers, semi-initialised bytes */
        bool exception:1;
        uint8_t id:7;
    };
    struct {
        bool _ptr_exception:1;
        bool pointer:1;
        bool is_first:1;
        /* only valid if is_first is true */
        bool obj_defined:1;
        bool off_defined:1;
        uint8_t ptr_type:2;
    };
    struct {
        bool _data_exception:1;
        bool _data_pointer:1;
        uint8_t bytes_defined:4;
    };

    ShadowByte() { exception = 0; pointer = 0; bytes_defined = 0; }
    friend std::ostream &operator<<( std::ostream &o, ShadowByte b )
    {
        if ( b.exception )
            return o << "[exception]";
        if ( b.pointer && b.is_first )
            return o << "[pointer " << (b.obj_defined ? "d" : "u")
                     << (b.off_defined ? "d" : "u") << "]";
        if ( b.pointer )
            return o << "[pointer cont]";
        return o << "[data " << int( b.bytes_defined ) << "]";
    }
};

static_assert( sizeof( ShadowByte ) == 1 );

struct ShadowException {};

template< typename PointerV >
struct Shadow
{
    ShadowByte *_first;
    ShadowException *_except;
    int _size;

    Shadow( ShadowByte *f, ShadowException *e, int s ) : _first( f ), _except( e ), _size( s ) {}

    template< typename V >
    typename std::enable_if< !V::IsPointer >::type update( int, V )
    {
    }

    template< typename V >
    typename std::enable_if< !V::IsPointer >::type query( int, V& )
    {
    }

    template< typename V >
    typename std::enable_if< V::IsPointer >::type update( int offset, V v )
    {
        if ( offset % 4 )
            NOT_IMPLEMENTED();
        static_assert( PointerBytes == 8 );
        auto &a = _first[ offset / 4 ], &b = _first[ offset / 4 + 1 ];
        a.exception = b.exception = false;
        a.pointer = b.pointer = true;
        a.is_first = true;
        b.is_first = false;
        a.obj_defined = v._obj_defined;
        a.off_defined = v._off_defined;
    }

    template< typename V >
    typename std::enable_if< V::IsPointer >::type query( int offset, V &v )
    {
        if ( offset % 4 )
            NOT_IMPLEMENTED();

        auto &a = _first[ offset / 4 ], &b = _first[ offset / 4 + 1 ];
        v._obj_defined = v._off_defined = false;
        if ( !a.pointer || !a.is_first )
            return;
        ASSERT( b.pointer );
        ASSERT( !b.is_first );
        v._obj_defined = a.obj_defined;
        v._off_defined = a.off_defined;
    }

    void update_slowpath( Shadow from_sh, int from_off, int to_off, int bytes )
    {
        int clearance = 0;

        /* scan PointerBytes-1 backwards to see if we may be damaging something */
        for ( int i = 1; i < PointerBytes - 1 && i < to_off; ++i )
        {
            ShadowByte *t = _first + (to_off - i) / 4;
            if ( t->exception )
                NOT_IMPLEMENTED();
            if ( t->pointer && t->is_first )
                clearance = i;
        }
        for ( int i = 1; i < PointerBytes - 1 && i < to_off; ++i )
        {
            ShadowByte *t = _first + (to_off - i) / 4;
            if ( t->pointer && !t->is_first && i >= PointerBytes / 2 )
                clearance = 0;
        }

        to_off -= clearance;
        bytes += clearance;

        uint8_t topbit = 8;

        struct
        {
            uint8_t defined;
            bool obj_defined:1;
            bool off_defined:1;
            bool pointer:1;
            bool exception:1;
        } v[bytes];

        int b = 0;
        while ( b < clearance )
        {
            ShadowByte *t = _first + (to_off + b) / 4;
            int k = (to_off + b) % 4;
            v[b].exception = false;
            v[b].pointer = false;
            v[b].defined = 0;
            if ( t->exception )
                NOT_IMPLEMENTED();
            if ( !t->pointer )
                v[b].defined = t->bytes_defined & (topbit >> k) ? 255 : 0;
            ++b;
        }

        b = clearance;
        while ( b < bytes )
        {
            v[b].exception = false;
            v[b].pointer = false;
            ShadowByte *f = from_sh._first + (from_off + b - clearance) / 4;
            int k = (from_off + b - clearance) % 4;
            if ( f->exception )
                NOT_IMPLEMENTED();
            if ( f->pointer && bytes - b >= PointerBytes )
            {
                ASSERT( !k );
                ASSERT( f->is_first );
                v[ b ].pointer = true;
                v[ b ].obj_defined = true;
                v[ b ].off_defined = true;
                for ( int i = 0; i < PointerBytes; ++i )
                    v[ b++ ].defined = 0;
                continue;
            }
            v[ b++ ].defined = f->pointer ? 0 : f->bytes_defined & (topbit >> k) ? 255 : 0;
        }

        b = 0;
        while ( b < bytes )
        {
            ShadowByte *t = _first + (to_off + b) / 4;
            int k = (to_off + b) % 4;
            if ( t->exception )
                NOT_IMPLEMENTED();
            if ( !k && v[ b ].pointer && bytes - b >= PointerBytes )
            {
                t->pointer = true;
                t->obj_defined = v[ b ].obj_defined;
                t->off_defined = v[ b ].off_defined;
                t->is_first = true;
                (t + 1)->pointer = true;
                (t + 1)->is_first = false;
                b += PointerBytes;
                continue;
            }
            t->pointer = false;
            t->bytes_defined &= ~(topbit >> k);
            if ( v[ b ].defined == 255 )
                t->bytes_defined |= topbit >> k;
            else if ( v[ b ].defined )
                NOT_IMPLEMENTED(); // exception
            ++ b;
        }

        ASSERT_EQ( b, bytes );

        for ( int i = 0; i < PointerBytes / 2 && to_off + b + i < _size; ++i )
        {
            ShadowByte *t = _first + (to_off + b + i) / 4;
            int k = (to_off + b + i) % 4;
            if ( t->exception )
                NOT_IMPLEMENTED();
            if ( !k && t->pointer && !t->is_first)
            {
                t->pointer = false;
                t->bytes_defined = 0;
            }
        }
    }

    void update_boundary( Shadow from_sh, int from_off, int to_off, int bytes, int polarity )
    {
        ASSERT( polarity );
        ASSERT_LT( bytes, 4 );

        if ( bytes )
        {
            ShadowByte *from = from_sh._first + from_off / 4, *to = _first + to_off / 4;
            if ( from->exception || to->exception )
                NOT_IMPLEMENTED();
            to->pointer = false; /* TODO form an exception if *to was a pointer */
            if ( from->pointer ) /* TODO form an exception instead */
                to->bytes_defined = 0;
            else
            {
                uint8_t mask = bitlevel::fill< uint8_t >( bytes );
                if ( polarity > 0 ) /* first 'bytes' bytes are affected */
                    mask = mask << ( 4 - bytes );
                to->bytes_defined &= ~mask;
                to->bytes_defined |= from->bytes_defined & mask;
            }
        }

        ShadowByte *affected = nullptr;
        if ( polarity < 0 && to_off > 0 )
            affected = _first + ( to_off - 1 ) / 4;
        if ( polarity > 0 && to_off < _size )
            affected = _first + to_off / 4;
        if ( affected && affected->exception )
            NOT_IMPLEMENTED();
        if ( affected && affected->pointer )
        {
            affected->pointer = 0;
            affected->bytes_defined = 0;
        }
    }

    void update_fastpath( Shadow from, int from_off, int to_off, int bytes )
    {
        ASSERT_EQ( from_off % 4, to_off % 4 );
        if ( from._first == _first )
            ASSERT_LEQ( bytes, abs( from_off - to_off ) );

        /* copy non-overlapping regions with equal (mis)alignment */

        int head = to_off % 4 ? 4 - to_off % 4 : 0;
        update_boundary( from, from_off, to_off, head, -1 );
        to_off += head;
        from_off += head;
        int tail = ( to_off + bytes ) % 4;
        update_boundary( from, from_off + bytes, to_off + bytes, tail, 1 );
        bytes -= tail;

        ASSERT_EQ( to_off % 4, 0 );
        ASSERT_EQ( from_off % 4, 0 );
        ASSERT_EQ( bytes % 4, 0 );

        auto f = from._first + from_off / 4;
        std::copy( f, f + bytes / 4, _first + to_off / 4 );
    }

    void update( Shadow from, int from_off, int to_off, int bytes )
    {
        return update_slowpath( from, from_off, to_off, bytes );
#if 0 /* TODO */
        if ( from_off % 4 != to_off % 4 )
            return update_slowpath( from, from_off, to_off, bytes );
        if ( from._first == _first && abs( from_off - to_off ) <= bytes )
            return update_slowpath( from, from_off, to_off, bytes );

        return update_fastpath( from, from_off, to_off, bytes );
#endif
    }
};

}

namespace t_vm {

struct Shadow
{
    using PointerV = vm::value::Pointer<>;
    using Sh = vm::Shadow< PointerV >;

    std::vector< vm::ShadowByte > _shb;
    Shadow() { _shb.resize( 100 ); }

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

    TEST( query )
    {
        set_pointer( 0 );
        PointerV p( vm::nullPointer(), false );
        ASSERT( !p.defined() );
        shadow().query( 0, p );
        ASSERT( p.defined() );
    }

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
};

}

}
