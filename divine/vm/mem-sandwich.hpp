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

#include <brick-types>

#include <divine/vm/mem-shadow-common.hpp>

namespace divine {
namespace vm::mem {

namespace bitlevel = brick::bitlevel;

/*
 * Extensible shadow for storing metadata.
 * This class is parametrised by a descriptor, that defines compressed and expanded format of the
 * metadata, provides compression and expansion functions and describes which shadow layers shall
 * be used.
 * This class itself is an interface between a heap and the metadata. It takes care of storing the
 * metadata in compressed format and provides their expanded form to the shadow layers when needed.
 * Almost all actual operations with the metadata are performed by the underlaying shadow layers.
 * It is called a sandwich because (a) it has different configurable layers with the storage and
 * (de)compression layer being the bread on either end, and (b) it's cute and I refuse to rename it.
 */
template< typename MasterPool, typename SandwichDescriptor >
struct SandwichShadow
{
    using Pool = brick::mem::SlavePool< MasterPool >;
    using Internal = typename Pool::Pointer;
    using Loc = InternalLoc< Internal >;
    using Descriptor = SandwichDescriptor;
    using Layers = typename Descriptor::template Layers< Internal >;
    using Compressed = typename Descriptor::Compressed;
    using Expanded = typename Descriptor::Expanded;
    static constexpr unsigned BPW = Descriptor::BitsPerWord;

    static_assert( BPW && ( ( BPW & ( BPW - 1 ) ) == 0 ),
                   "Descriptor::BitsPerWord must be a power of two" );
    static_assert( sizeof( Compressed ) * 8 >= BPW,
                   "Descriptor::Compressed does not contain all bits per word" );

    Pool _shadow,
         _shared;

    Layers _layers;

    SandwichShadow( const MasterPool &mp )
        : _shadow( mp ), _shared( mp ) {}

    void make( Internal p, int size )
    {
        constexpr unsigned divisor = 32 / BPW;
        _shadow.materialise( p, ( size / divisor ) + ( size % divisor ? 1 : 0 ) );
        _shared.materialise( p, 1 );
        _layers.make( p, size );
    }

    void free( Internal p )
    {
        _layers.free( p );
    }

    bool &shared( Internal p )
    {
        return *_shared.template machinePointer< bool >( p );
    }

    bool equal( Internal a, Internal b, int sz )
    {
        return compare( *this, a, b, sz ) == 0;
    }

    template< typename OtherSH >
    int compare( OtherSH &a_sh, typename OtherSH::Internal a_obj, Internal b_obj, int sz )
    {
        int cmp;
        const int words = ( sz + 3 ) / 4;

        auto a = Loc( a_obj, 0 );
        auto b = Loc( b_obj, 0 );
        auto sh_a = a_sh.compressed( a, words );
        auto sh_b = compressed( b, words );
        auto i_a = sh_a.begin();
        auto i_b = sh_b.begin();

        int off = 0;

        if ( sz >= 4 )
        {
            for ( ; off < bitlevel::downalign( sz, 4 ); off += 4 )
            {
                Compressed c_a = *i_a++;
                Compressed c_b = *i_b++;
                if ( ( cmp = c_a - c_b ) )
                    return cmp;
                Expanded exp_a = Descriptor::expand( c_a );
                Expanded exp_b = Descriptor::expand( c_b );
                if ( ( cmp = _layers.compare_word( a_sh, a + off, exp_a, b + off, exp_b ) ) )
                    return cmp;
            }
        }

        if ( off < sz )
        {
            Compressed c_a = *i_a;
            Compressed c_b = *i_b;
            if ( ( cmp = c_a - c_b ) )
                return cmp;
            Expanded exp_a = Descriptor::expand( c_a );
            Expanded exp_b = Descriptor::expand( c_b );
            for ( ; off < sz; ++off )
                if ( ( cmp = _layers.compare_byte( a_sh, a + off, exp_a, b + off, exp_b ) ) )
                    return cmp;
        }

        return 0;
    }

    using CompressedC = BitsetContainer< BPW, Pool >;
    CompressedC compressed( Loc l, unsigned words )
    {
        return CompressedC( _shadow, l.object, l.offset / 4, words + l.offset / 4 );
    }

    template< typename V >
    void write( Loc l, V value )
    {
        constexpr int sz = sizeof( typename V::Raw );
        constexpr int words = ( sz + 3 ) / 4;

        if ( sz >= 4 )
            ASSERT_EQ( l.offset % 4, 0 );
        else if ( sz == 2 )
            ASSERT_LT( l.offset % 4, 3 );
        else
            ASSERT_EQ( sz, 1 );

        auto sh = compressed( l, words );
        Expanded exp[ words ];

        std::transform( sh.begin(), sh.end(), exp, Descriptor::expand );
        _layers.write( l, value, exp );
        std::transform( exp, exp + words, sh.begin(), Descriptor::compress );
    }

    template< typename V >
    void read( Loc l, V &value )
    {
        constexpr int sz = sizeof( typename V::Raw );
        constexpr int words = ( sz + 3 ) / 4;

        if ( sz >= 4 )
            ASSERT_EQ( l.offset % 4, 0 );
        else if ( sz == 2 )
            ASSERT_LT( l.offset % 4, 3 );
        else
            ASSERT_EQ( sz, 1 );

        auto sh = compressed( l, words );
        Expanded exp[ words ];

        std::transform( sh.begin(), sh.end(), exp, Descriptor::expand );
        _layers.read( l, value, exp );
    }

    template< typename FromSh, typename FromHeapReader, typename ToHeapReader >
    void copy( FromSh &from_sh, typename FromSh::Loc from, Loc to, int sz,
               FromHeapReader fhr, ToHeapReader thr )
    {
        if ( sz == 0 )
            return;

        ASSERT_LT( 0, sz );
        const int words = ( sz + 3 ) / 4;

        auto sh_from = from_sh.compressed( from, words );
        auto sh_to = compressed( to, words );
        auto i_from = sh_from.begin();
        auto i_to = sh_to.begin();

        int off = 0;

        // Fast path -- source and destination are both word-aligned.
        // Compressed data are copied verbatim. This assumes that metadata do not contain any
        // position-dependent information.
        if ( sz >= 4 && from.offset % 4 == to.offset % 4 )
        {
            ASSERT_EQ( from.offset % 4, 0 );

            for ( ; off < bitlevel::downalign( sz, 4 ); off += 4 )
            {
                Expanded exp_src = Descriptor::expand( *i_from );
                Expanded exp_dst = Descriptor::expand( *i_to );
                _layers.copy_word( from_sh, from + off, exp_src, to + off, exp_dst );
                *i_to++ = *i_from++;
            }
        }

        // Slow path -- unaligned and tail of aligned copy.
        // The stored data may differ from the source (e.g. because of pointer fragmentation).
        if ( off < sz )
        {
            int off_from = from.offset + off,
                off_to = to.offset + off;
            bool written = false;

            Expanded exp_src,
                     exp_dst;

            if ( off_from % 4 )
            {
                exp_src = Descriptor::expand( *i_from++ );
                int aligned = bitlevel::downalign( off_from, 4 );
                _layers.copy_init_src( from_sh, from.object, aligned, exp_src, fhr );
            }
            if ( off_to % 4 )
            {
                exp_dst = Descriptor::expand( *i_to );
                int aligned = bitlevel::downalign( off_to, 4 );
                _layers.copy_init_dst( to.object, aligned, exp_dst, thr );
            }

            while ( off < sz )
            {
                if ( off_from % 4 == 0 )
                {
                    exp_src = Descriptor::expand( *i_from++ );
                    _layers.copy_init_src( from_sh, from.object, off_from, exp_src, fhr );
                }
                if ( off_to % 4 == 0 )
                {
                    exp_dst = Descriptor::expand( *i_to );
                    if ( sz - off < 4 )
                        _layers.copy_init_dst( to.object, off_to, exp_dst, thr );
                    written = false;
                }

                _layers.copy_byte( from_sh, from + off, exp_src, fhr, to + off, exp_dst, thr );

                ++off;
                ++off_from;
                ++off_to;

                if ( off_to % 4 == 0 )
                {
                    _layers.copy_done( to.object, off_to - 4, exp_dst );
                    *i_to++ = Descriptor::compress( exp_dst );
                    written = true;
                }
            }

            if ( ! written )
            {
                int aligned = bitlevel::downalign( off_to, 4 );
                _layers.copy_done( to.object, aligned, exp_dst );
                *i_to = Descriptor::compress( exp_dst );
            }
        }
    }

    template< typename HeapReader >
    void copy( Loc from, Loc to, int sz, HeapReader hr ) {
        copy( *this, from, to, sz, hr, hr );
    }

    struct PointerC
    {
        using c_proxy = typename CompressedC::proxy;
        struct proxy
        {
            PointerC &p;
            int off;

            proxy( PointerC &p, int o )
                : p( p ), off( o )
            {
                ASSERT( Descriptor::is_pointer_or_exception( c_now() ) ||
                        ( off + 4 < p.to && Descriptor::is_pointer( c_next() ) ) );
            }
            c_proxy c_now() const { return p.compressed[ off / 4 ]; }
            c_proxy c_next() const { return p.compressed[ off / 4 + 1 ]; }

            proxy *operator->() { return this; }
            int offset() const { return off - p.from; }
            int size() const
            {
                if ( Descriptor::is_pointer( c_now() ) )
                    return 4;
                if ( ! Descriptor::is_pointer_or_exception( c_now() ) &&
                     off + 4 < p.to &&
                     Descriptor::is_pointer( c_next() ) )
                    return 8;
                if ( Descriptor::is_pointer_exception( c_now() ) )
                    return 1;
                NOT_IMPLEMENTED();
            }
            bool operator==( const proxy &o ) const
            {
                return offset() == o.offset() && size() == o.size();
            }
            uint32_t fragment() const
            {
                ASSERT( Descriptor::is_pointer_exception( c_now() ) );
                return p.layers.pointer_exception( p.obj, bitlevel::downalign( off, 4 ) )
                    .objid[ off % 4 ];
            }
        };

        struct iterator : std::iterator< std::forward_iterator_tag, proxy >
        {
            PointerC &p;
            int off;

            iterator( PointerC &p, int o ) : p( p ), off( o ) {}

            c_proxy c_now() const { return p.compressed[ off / 4 ]; }
            c_proxy c_next() const { return p.compressed[ off / 4 + 1 ]; }

            void seek()
            {
                if ( off >= p.to )
                    return;

                if ( off % 4 )
                    ASSERT( Descriptor::is_pointer_exception( c_now() ) );

                if ( Descriptor::is_pointer_exception( c_now() ) )
                {
                    auto exc = p.layers.pointer_exception( p.obj, bitlevel::downalign( off, 4 ) );

                    do {
                        if ( exc.objid[ off % 4 ] )
                            return;
                        ++off;
                    } while ( off % 4 );
                }

                while ( off < p.to &&
                        ! Descriptor::is_pointer_or_exception( c_now() ) &&
                        ( ! ( off + 4 < p.to ) ||
                          ! Descriptor::is_pointer( c_next() ) ) )
                    off += 4;

                if ( off >= p.to )
                {
                    off = p.to;
                    return;
                }

                if ( Descriptor::is_pointer_exception( c_now() ) )
                    seek();
            }
            iterator &operator++()
            {
                off += (*this)->size();
                seek();
                return *this;
            }
            proxy operator*() const { return proxy( p, off ); }
            proxy operator->() const { return proxy( p, off ); }
            bool operator==( iterator o ) const {
                return off == o.off && p.compressed._base == o.p.compressed._base;
            }
            bool operator!=( iterator o ) const { return ! ( *this == o ); }
        };

        CompressedC compressed;
        Internal obj;
        Layers &layers;
        int from;
        int to;
        iterator begin() {
            auto b = iterator( *this, from );
            if ( b.off % 4 && ! Descriptor::is_pointer_exception( b.c_now() ) )
                b.off = std::min( bitlevel::align( b.off, 4 ), to );
            b.seek();
            return b;
        }
        iterator end() { return iterator( *this, to ); }
        PointerC( Pool &p, Layers &l, Internal i, int f, int t )
            : compressed( p, i, 0, ( t + 3 ) / 4 ), obj( i ), layers( l ), from( f ), to( t )
        {}
    };

    auto pointers( Loc l, int sz )
    {
        return PointerC( _shadow, _layers, l.object, l.offset, l.offset + sz );
    }
};

}
}

