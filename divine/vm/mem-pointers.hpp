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

#include <divine/vm/mem-shadow-common.hpp>

namespace divine {
namespace vm::mem {

namespace bitlevel = brick::bitlevel;

struct PointerException
{
    uint32_t objid[ 4 ];   // Object IDs of the original pointers
    uint8_t metadata[ 4 ]; // Position of byte in the orig. pointers, type of pointer

    uint8_t index( uint8_t p ) const { return metadata[ p ] & 0x07; }
    void index( uint8_t p, uint8_t i ) { metadata[ p ] &= ~0x07; metadata[ p ] |= i & 0x07; }

    uint8_t type( uint8_t p ) const { return metadata[ p ] >> 4 & 0x07; }
    void type( uint8_t p, uint8_t t ) { metadata[ p ] &= ~0x70; metadata[ p ] |= t << 4; }

    bool redundant() const
    {
        for ( int i = 0; i < 4; ++i)
            if ( index( i ) != i || type( i ) != type( 0 ) )
                return false;

        return objid[ 0 ] == objid[ 1 ]
            && objid[ 0 ] == objid[ 2 ]
            && objid[ 0 ] == objid[ 3 ];
    }

    bool valid() const
    {
        for ( int i = 0; i < 4; ++i )
            if ( objid[ i ] != 0 )
                return true;
        return false;
    }

    void invalidate()
    {
        std::fill( objid, objid + 4, 0 );
    }

    static PointerException null()
    {
        PointerException e;
        e.invalidate();
        return e;
    }
};

inline std::ostream &operator<<( std::ostream &o, const PointerException &e )
{
    if ( !e.valid() )
    {
        o << "INVALID ";
    }

    o << "pointer exc.: {\n";

    for ( int i = 0; i < 4; ++i )
    {
        o << "    { " << i << ": ";
        if ( e.objid[ i ] )
            o << "byte " << +e.index( i ) << " of "
              << PointerType( e.type( i ) ) << " ptr. to " << e.objid[ i ];
        else
            o << "data";
        o << " }\n";
    }

    o << "  }\n";

    return o;
}

/*
 * Shadow layer for tracking pointers and their types, including fragmented pointers.
 * Required fields in Expanded:
 *      pointer : 1
 *      pointer_type : 3
 *      pointer_exception : 1
 */

template< typename NextLayer >
struct PointerLayer : public NextLayer
{
    using Internal = typename NextLayer::Internal;
    using Loc = typename NextLayer::Loc;
    using Expanded = typename NextLayer::Expanded;

    class PointerExceptions : public ExceptionMap< PointerException, Internal >
    {
    public:
        using Base = ExceptionMap< PointerException, Internal >;

    private:
        using Lock = typename Base::Lock;
        using Base::_exceptions;
        using Base::_mtx;

    public:
        void dump() const
        {
            std::cout << "pointer exceptions: {\n";
            for ( auto &e : _exceptions )
            {
                std::cout << "  {" << e.first.object._raw << " + "
                   << e.first.offset << ": " << e.second << "  }\n";
            }
            std::cout << "}\n";
        }
    };

    std::shared_ptr< PointerExceptions > _ptr_exceptions;
    PointerException current_ptr_from,
                     current_ptr_to;

    PointerLayer() : _ptr_exceptions( new PointerExceptions ) {}

    template< typename OtherSh >
    int compare_word( OtherSh &a_sh, typename OtherSh::Loc a, Expanded exp_a, Loc b, Expanded exp_b )
    {
        ASSERT_EQ( exp_a.pointer_exception, exp_b.pointer_exception );
        // This function correctly assumes that is is called only when there is an exception and the
        // exception types match. However, it also assumes that the only other type of exception is
        // data exception, so that there is either a pointer exception or data exception (and
        // therefore it cannot be a pointer). This is true at the moment, but should it cease to
        // hold, this assert will fire and it will be necessary to rewrite this code.
        ASSERT( ! exp_a.pointer && ! exp_b.pointer );

        if ( exp_a.pointer_exception )
        {
            auto pe_a = a_sh._layers.pointer_exception( a.object, a.offset );
            auto pe_b = pointer_exception( b.object, b.offset );
            for ( int i = 0; i < 4 ; ++i )
            {
                if ( pe_a.objid[ i ] == 0 && pe_b.objid[ i ] == 0 )
                    continue;
                if ( pe_a.objid[ i ] == 0 )
                    return -1;
                if ( pe_b.objid[ i ] == 0 )
                    return 1;
                int cmp = pe_a.type( i ) - pe_b.type( i );
                if ( cmp )
                    return cmp;
                if ( ( cmp = pe_a.index( i ) - pe_b.index( i ) ) )
                    return cmp;
            }
        }

        return NextLayer::compare_word( a_sh, a, exp_a, b, exp_b );
    }

    void free( Internal p )
    {
        _ptr_exceptions->free( p );

        NextLayer::free( p );
    }

    template< typename V >
    void write( Loc l, V value, Expanded *exp )
    {
        NextLayer::write( l, value, exp );

        constexpr int sz = sizeof( typename V::Raw );
        constexpr int words = ( sz + 3 ) / 4;

        for ( int w = 0; w < words; ++w )
        {
            if ( exp[ w ].pointer_exception )
                _ptr_exceptions->invalidate( l.object, bitlevel::downalign( l.offset, 4 ) + 4 * w );
        }

        if ( sz == PointerBytes && value.pointer() )
        {
            exp[ 0 ].pointer = false;
            exp[ 0 ].pointer_exception = false;
            exp[ 1 ].pointer = true;
            exp[ 1 ].pointer_exception = false;
            /* TODO! */
            //exp[ 1 ].pointer_type = value.cooked().type();
            exp[ 1 ].pointer_type = 0;
        }
        else
        {
            for ( int w = 0; w < words; ++w )
            {
                exp[ w ].pointer = false;
                exp[ w ].pointer_exception = false;
            }
        }
    }

    template< typename V >
    void read( Loc l, V &value, Expanded *exp )
    {
        constexpr int sz = sizeof( typename V::Raw );

        if ( sz == PointerBytes && exp[ 1 ].pointer && ! exp[ 0 ].pointer )
        {
            value.pointer( true );
            /* TODO! */
            //value.cooked().type( exp[ 1 ].pointer_type );
        }
        else
            value.pointer( false );

        NextLayer::read( l, value, exp );
    }

    // Fast copy
    template< typename FromSh >
    void copy_word( FromSh &from_sh, typename FromSh::Loc from, Expanded exp_src,
                    Loc to, Expanded exp_dst )
    {
        if ( exp_src.pointer_exception )
            _ptr_exceptions->set( to.object, to.offset, from_sh._layers._ptr_exceptions->at(
                            from.object, from.offset ) );
        else if ( exp_dst.pointer_exception )
            _ptr_exceptions->invalidate( to.object, to.offset );

        NextLayer::copy_word( from_sh, from, exp_src, to, exp_dst );
    }

    // Slow copy
    template< typename FromSh, typename FromHeapReader >
    void copy_init_src( FromSh &from_sh, typename FromSh::Internal obj, int off,
                        const Expanded &exp, FromHeapReader fhr )
    {
        NextLayer::copy_init_src( from_sh, obj, off, exp, fhr );

        current_ptr_from = from_sh._layers._read_ptr( obj, off, exp, fhr );
    }

    template< typename ToHeapReader >
    void copy_init_dst( Internal obj, int off, const Expanded &exp, ToHeapReader thr )
    {
        NextLayer::copy_init_dst( obj, off, exp, thr );

        current_ptr_to = _read_ptr( obj, off, exp, thr );
    }

    template< typename FromSh, typename FromHeapReader, typename ToHeapReader >
    void copy_byte( FromSh &from_sh, typename FromSh::Loc from, const Expanded &exp_src,
                    FromHeapReader fhr, Loc to, Expanded &exp_dst, ToHeapReader thr )
    {
        NextLayer::copy_byte( from_sh, from, exp_src, fhr, to, exp_dst, thr );

        current_ptr_to.objid[ to.offset % 4 ] = current_ptr_from.objid[ from.offset % 4 ];
        current_ptr_to.metadata[ to.offset % 4 ] = current_ptr_from.metadata[ from.offset % 4 ];
    }

    void copy_done( Internal obj, int off, Expanded &exp )
    {
        NextLayer::copy_done( obj, off, exp );

        bool was_ptr_exc = exp.pointer_exception;

        if ( ! current_ptr_to.valid() )
        {
            exp.pointer = false;
            exp.pointer_exception = false;
        }
        else if ( current_ptr_to.redundant() )
        {
            exp.pointer = true;
            exp.pointer_exception = false;
        }
        else
        {
            _ptr_exceptions->set( obj, off, current_ptr_to );
            exp.pointer = false;
            exp.pointer_exception = true;
        }

        if ( was_ptr_exc && ! exp.pointer_exception )
            _ptr_exceptions->invalidate( obj, off );
    }

    PointerException pointer_exception( Internal obj, int off )
    {
        ASSERT_EQ( off % 4, 0 );
        return _ptr_exceptions->at( obj, off );
    }

    template< typename HeapReader >
    PointerException _read_ptr( Internal obj, int off, const Expanded &exp, HeapReader hr )
    {
        if ( exp.pointer_exception )
            return pointer_exception( obj, off );
        if ( ! exp.pointer )
            return PointerException::null();

        PointerException e;
        uint32_t objid = hr( obj, off );
        std::fill( e.objid, e.objid + 4, objid );
        for ( int i = 0; i < 4; ++i )
        {
            e.index( i, i );
            e.type( i, exp.pointer_type );
        }

        return e;
    }
};

}

namespace t_vm {

struct PointerException {
    using PtrExc = vm::mem::PointerException;

    TEST( null )
    {
        PtrExc e = PtrExc::null();
        ASSERT( ! e.valid() );
    }

    TEST( valid )
    {
        PtrExc e;
        e.objid[ 0 ] = 0xABCD;
        ASSERT( e.valid() );
        ASSERT( ! e.redundant() );
    }

    TEST( invalidate )
    {
        PtrExc e;
        e.objid[ 0 ] = 0xABCD;
        e.invalidate();
        ASSERT( ! e.valid() );
    }

    TEST( redundant )
    {
        PtrExc e;
        for ( int i = 0; i < 4; ++i )
        {
            e.objid[ i ] = 0xABCD;
            e.index( i, i );
            e.type( i, 0x3 );
        }
        ASSERT( e.valid() );
        ASSERT( e.redundant() );
    }

    TEST( not_redundant_objid )
    {
        PtrExc e;
        for ( int i = 0; i < 4; ++i )
        {
            e.objid[ i ] = 0xABCD + i;
            e.index( i, i );
            e.type( i, 0x3 );
        }
        ASSERT( e.valid() );
        ASSERT( ! e.redundant() );
    }

    TEST( not_redundant_index )
    {
        PtrExc e;
        for ( int i = 0; i < 4; ++i )
        {
            e.objid[ i ] = 0xABCD;
            e.index( i, 3 - i );
            e.type( i, 0x3 );
        }
        ASSERT( e.valid() );
        ASSERT( ! e.redundant() );
    }

    TEST( not_redundant_type )
    {
        PtrExc e;
        for ( int i = 0; i < 4; ++i )
        {
            e.objid[ i ] = 0xABCD;
            e.index( i, i );
            e.type( i, i );
        }
        ASSERT( e.valid() );
        ASSERT( ! e.redundant() );
    }

};

}

}

