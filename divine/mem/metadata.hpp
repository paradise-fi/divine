// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016-2018 Petr Ročkai <code@fixp.eu>
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
#include <divine/mem/bitset.hpp>

namespace divine::mem
{

namespace bitlevel = brick::bitlevel;

union ExpandedMetaPDT // Representation the shadow layers operate on
{
    struct
    {
        uint16_t taint : 4, // 15 possible colors for tainting
                 _free1_ : 3,
                 pointer : 1, // pointer or just a pointer fragment
                 pointer_exception : 1,
                 data_exception : 1,
                 _free2_ : 2,
                 defined : 4; // definedness flags for 4 bytes of a word
    };
    uint16_t _raw;
    constexpr ExpandedMetaPDT() : _raw( 0 ) {}
    constexpr ExpandedMetaPDT( uint16_t raw ) : _raw( raw ) {}
    operator uint16_t() const { return _raw; }
};

/* Descriptor of sandwich shadow with a pointer layer, a definedness layer and a taint layer. */
template< typename Next >
struct CompressPDT : Next
{
    // 16 bits:  | _ , _ , _ , _ : _ , _ , _ , _ | _ , _ , _ , _ : _ , _ , _ , _ |
    // Expanded: | [definedness] : _ , _ , DE, PE| P , _ , _ , _ : [ t a i n t ] |

    // 8 bits : | _ , _ , _ , _ : _ , _ , _ , _ |
    // Pointer: | 1 , 0 , 0 , 0 : [ t a i n t ] |
    // Data:    | 0 , [   0000000 - 1010000   ] | 81 values for definedness + taint
    // Def exc: | 0 , 1 , 1 , 0 : [ t a i n t ] |
    // Ptr exc: | 0 , 1 , 1 , 1 : [ t a i n t ] |

    // In [definedness] and [taint], less significant bits correspond to bytes on lower addresses.

    static constexpr unsigned BitsPerWord = 8;

    using Compressed = uint8_t; // Representation stored in the pool
    using Expanded = mem::ExpandedMetaPDT;

    /* Compresses expanded form of metadata. Compressed form size is required
     * to be power of two.
     *
     * Moreover compression is required to be lossless, with a regard to valid
     * combinations of metadata.
     * */
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

    /* Expands compressed form of metadata.
     *
     * Expansion (decompresion) of zero is required to be reasonable form of
     * expanded metadata, i.e. it corresponds to newly allocated memory location.
     * The expanded form represent undefined word without any exceptions or taints.
     */
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

union ExpandedMetaPD // Representation the shadow layers operate on
{
    struct
    {
        uint8_t defined : 4,
                pointer : 1,
                pointer_exception : 1,
                data_exception : 1,
                _free_ : 1;
    };
    uint8_t _raw;
    constexpr ExpandedMetaPD() : _raw( 0 ) {}
    constexpr ExpandedMetaPD( uint8_t raw ) : _raw( raw ) {}
    operator uint8_t() const { return _raw; }
};
/* Descriptor of sandwich shadow with a pointer layer and a definedness layer.
 * Not very memory-efficient, as we do not allow 5bit compressed forms, but expanded and compressed
 * forms are the same. */
template< typename Next >
struct CompressPD : Next
{
    // 8 bits :                | _ , _ , _ , _ : _ , _ , _ , _ |
    // Compressed = Expanded : | _ , DE, PE, P : [definedness] |

    // In [definedness] less significant bits correspond to bytes on lower addresses.

    static constexpr unsigned BitsPerWord = 8;

    using Compressed = uint8_t; // Representation stored in the pool
    using Expanded = mem::ExpandedMetaPD;

    constexpr static Compressed compress( Expanded exp )
    {
        return exp._raw;
    }

    constexpr static Expanded expand( Compressed c )
    {
        return c;
    }

    // Shall be true if 'c' encodes all metadata (i.e. is not an exception)
    constexpr static bool is_trivial( Compressed c )
    {
        return ( c & 0x60 ) == 0;
    }

    // These predicates exist in order to avoid expanding compressed data when searching for
    // pointers.
    constexpr static bool is_pointer( Compressed c )
    {
        return c & 0x10;
    }
    constexpr static bool is_pointer_or_exception( Compressed c )
    {
        return c & 0x30;
    }
    constexpr static bool is_pointer_exception( Compressed c )
    {
        return c & 0x20;
    }
};

/*
 * Extensible metadata storage.
 *
 * This class itself is an interface between upper layers (interface and data
 * storage) of the heap and the metadata storage portion. It takes care of
 * storing the metadata in a compressed format and provides their expanded form
 * to the individual metadata layers as needed.  Almost all actual operations
 * with the metadata are performed by the layers below.
 */
template< typename Next >
struct Metadata : Next
{
    using typename Next::Pool;
    using MetaPool = brick::mem::SlavePool< Pool >;
    using typename Next::Internal;
    using typename Next::Loc;
    using typename Next::Compressed;
    using typename Next::Expanded;
    static constexpr unsigned BPW = Next::BitsPerWord;

    static_assert( BPW && ( ( BPW & ( BPW - 1 ) ) == 0 ),
                   "Next::BitsPerWord must be a power of two" );
    static_assert( sizeof( Compressed ) * 8 >= BPW,
                   "Next::Compressed does not contain all bits per word" );

    mutable MetaPool _meta;
    Metadata() : _meta( Next::_objects ) {}
    auto &meta() { return _meta; }
    void materialise( Internal i, int size ) { _meta.materialise( i, meta_size( size ) ); }

    static constexpr int meta_size( int size )
    {
        constexpr unsigned divisor = 32 / BPW;
        return ( size / divisor ) + ( size % divisor ? 1 : 0 );
    }

    // Compares expanded metadata through all layers.
    template< typename OtherSH >
    int compare( OtherSH &a_sh, typename OtherSH::Internal a_obj, Internal b_obj, int sz,
                 bool skip_objids )
    {
        int cmp;
        const int words = ( sz + 3 ) / 4;

        auto a = Loc( a_obj, 0, 0 );
        auto b = Loc( b_obj, 0, 0 );
        auto sh_a = a_sh.compressed( a, words );
        auto sh_b = compressed( b, words );
        auto i_a = sh_a.begin();
        auto i_b = sh_b.begin();

        int off = 0;

        // This assumes that whole objects are being compared, i.e. that there are no actual data
        // after 'sz' bytes.
        for ( ; off < bitlevel::align( sz, 4 ); off += 4 )
        {
            Compressed c_a = *i_a++;
            Compressed c_b = *i_b++;
            if ( ( cmp = c_a - c_b ) )
                return cmp;
            if ( Next::is_trivial( c_a ) )
                continue;
            Expanded exp_a = Next::expand( c_a );
            Expanded exp_b = Next::expand( c_b );
            if ( ( cmp = Next::compare_word( a_sh, a + off, exp_a, b + off, exp_b, skip_objids ) ) )
                return cmp;
        }

        return Next::compare( a_sh, a_obj, b_obj, sz, skip_objids );
    }

    template< typename S, typename F >
    void hash( Internal i, int size, S &state, F ptr_cb ) const
    {
        auto s = meta_size( size );
        state.realign();
        if ( s < 4 )
            for ( int j = 0; j < s; ++ j )
                state.update_aligned( _meta.template machinePointer< uint8_t >( i )[ j ] );
        else
            state.update_aligned( _meta.template machinePointer< uint8_t >( i ), s );
        Next::hash( i, size, state, ptr_cb );
    }

    using CompressedC = BitsetContainer< BPW, MetaPool >;
    CompressedC compressed( Loc l, unsigned words ) const
    {
        return CompressedC( _meta, l.object, l.offset / 4, words + l.offset / 4 );
    }

    // Stores metadata from internal representation (value) to shadow memory (l).
    //
    // After successful execution of writes of lower layers, metadata layer
    // compresses and stores resulting expanded metadata.
    template< typename V >
    void write( Loc l, V value )
    {
        int sz = value.size();
        int words = ( sz + 3 ) / 4;

        if ( sz >= 3 )
            ASSERT_EQ( l.offset % 4, 0 );
        else if ( sz == 2 )
            ASSERT_LT( l.offset % 4, 3 );
        else
            ASSERT_EQ( sz, 1 );

        auto sh = compressed( l, words );
        Expanded exp[ words ];

        std::transform( sh.begin(), sh.end(), exp, Next::expand );
        Next::write( l, value, exp );
        std::transform( exp, exp + words, sh.begin(), Next::compress );
    }


    // Fill internal representation (value) by data from shadow memory (l).
    //
    // Firstly it expands and decompress metadata and afterwards it let lower
    // layers to fill value from decompressed metadata.
    template< typename V >
    void read( Loc l, V &value ) const
    {
        int sz = value.size();
        int words = ( sz + 3 ) / 4;

        if ( sz >= 3 )
            ASSERT_EQ( l.offset % 4, 0 );
        else if ( sz == 2 )
            ASSERT_LT( l.offset % 4, 3 );
        else
            ASSERT_EQ( sz, 1 );

        auto sh = compressed( l, words );
        Expanded exp[ words ];

        std::transform( sh.begin(), sh.end(), exp, Next::expand );
        Next::read( l, value, exp );
    }


    // copy handles copying between shadow memory.
    //
    // 'from_h' are source shadow memory layers and 'from' is an internal
    // address of source shadow memory
    // 'to_h' are destination shadow memory layers and 'to' is an internal
    // address of destination shadow memory
    // 'sz' is size of copied shadow memory
    //
    // 'internal' ??? TODO
    //
    // Copy determines whether it can perform fast copy when source and
    // destination are both word-aligned. In such case copy can operate on
    // words (requires copy_word from below layers), else slow copying is
    // performed. Slow copying operates on bytes (requires copy_init_src,
    // copy_init_dst, copy_byte, copy_done from lower layers).
    //
    // Slow copy is state-full, in means that lower layers might store
    // information about copying progress via above mentioned functions:
    //
    // 'copy_init_src', 'copy_init_dst' initializes copying in lower layers
    // (e.g. layers with exceptions initializes exception related data if and
    // exception is present in copied memory)
    //
    // 'copy_byte' mutates state of current copying
    //
    // 'copy_done' finalizes copying process -- layers compute and store final
    // metadata and sets final expanded form, optionally sets an exception
    template< typename FromH, typename ToH >
    static void copy( FromH &from_h, typename FromH::Loc from, ToH &to_h, Loc to, int sz, bool internal )
    {
        if ( sz == 0 )
            return;

        ASSERT_LT( 0, sz );
        const int words = ( sz + 3 ) / 4;

        auto sh_from = from_h.compressed( from, words );
        auto sh_to = to_h.compressed( to, words );
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
                // If any of metadata contains an exception we need perform copy also in lower layers
                if ( ! Next::is_trivial( *i_from ) || ! Next::is_trivial( *i_to ) )
                {
                    Expanded exp_src = Next::expand( *i_from );
                    Expanded exp_dst = Next::expand( *i_to );
                    Next::copy_word( from_h, to_h, from + off, exp_src, to + off, exp_dst );
                }
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
                exp_src = Next::expand( *i_from++ );
                int aligned = bitlevel::downalign( off_from, 4 );
                Next::copy_init_src( from_h, to_h, from.object, aligned, exp_src );
            }
            if ( off_to % 4 )
            {
                exp_dst = Next::expand( *i_to );
                int aligned = bitlevel::downalign( off_to, 4 );
                Next::copy_init_dst( to_h, to.object, aligned, exp_dst );
            }

            while ( off < sz )
            {
                if ( off_from % 4 == 0 )
                {
                    exp_src = Next::expand( *i_from++ );
                    Next::copy_init_src( from_h, to_h, from.object, off_from, exp_src );
                }
                if ( off_to % 4 == 0 )
                {
                    exp_dst = Next::expand( *i_to );
                    if ( sz - off < 4 )
                        Next::copy_init_dst( to_h, to.object, off_to, exp_dst );
                    written = false;
                }

                Next::copy_byte( from_h, to_h, from + off, exp_src, to + off, exp_dst );

                ++off;
                ++off_from;
                ++off_to;

                if ( off_to % 4 == 0 )
                {
                    Next::copy_done( to_h, to.object, off_to - 4, exp_dst );
                    *i_to++ = Next::compress( exp_dst );
                    written = true;
                }
            }

            if ( ! written )
            {
                int aligned = bitlevel::downalign( off_to, 4 );
                Next::copy_done( to_h, to.object, aligned, exp_dst );
                *i_to = Next::compress( exp_dst );
            }
        }

        Next::copy( from_h, from, to_h, to, sz, internal );
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
                ASSERT( Next::is_pointer_or_exception( c_now() ) ||
                        ( off + 4 < p.to && Next::is_pointer( c_next() ) ) );
            }
            c_proxy c_now() const { return p.compressed[ off / 4 ]; }
            c_proxy c_next() const { return p.compressed[ off / 4 + 1 ]; }

            proxy *operator->() { return this; }
            int offset() const { return off - p.from; }
            int size() const
            {
                if ( Next::is_pointer( c_now() ) )
                    return 4;
                if ( ! Next::is_pointer_or_exception( c_now() ) &&
                     off + 4 < p.to &&
                     Next::is_pointer( c_next() ) )
                    return 8;
                if ( Next::is_pointer_exception( c_now() ) )
                    return 1;
                NOT_IMPLEMENTED();
            }
            bool operator==( const proxy &o ) const
            {
                return offset() == o.offset() && size() == o.size();
            }

            auto exception() const
            {
                ASSERT( Next::is_pointer_exception( c_now() ) );
                return p.layers.pointer_exception( p.obj, bitlevel::downalign( off, 4 ) );
            }

            uint32_t fragment() const { return exception().objid[ off % 4 ]; }
        };

        // TODO remove deprecated std::iterator
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
                    ASSERT( Next::is_pointer_exception( c_now() ) );

                if ( Next::is_pointer_exception( c_now() ) )
                {
                    auto exc = p.layers.pointer_exception( p.obj, bitlevel::downalign( off, 4 ) );

                    do {
                        if ( exc.objid[ off % 4 ] )
                            return;
                        ++off;
                    } while ( off % 4 );
                }

                while ( off < p.to &&
                        ! Next::is_pointer_or_exception( c_now() ) &&
                        ( ! ( off + 4 < p.to ) ||
                          ! Next::is_pointer( c_next() ) ) )
                    off += 4;

                if ( off >= p.to )
                {
                    off = p.to;
                    return;
                }

                if ( Next::is_pointer_exception( c_now() ) )
                    seek();
                else if ( p.to - off < 4 )
                    off = p.to;
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
        Next &layers;
        int from;
        int to;
        iterator begin() {
            auto b = iterator( *this, from );
            if ( b.off % 4 && ! Next::is_pointer_exception( b.c_now() ) )
                b.off = std::min( bitlevel::align( b.off, 4 ), to );
            b.seek();
            return b;
        }
        iterator end() { return iterator( *this, to ); }
        PointerC( MetaPool &p, Next &l, Internal i, int f, int t )
            : compressed( p, i, 0, ( t + 3 ) / 4 ), obj( i ), layers( l ), from( f ), to( t )
        {}
    };

    // Returns a range of pointers in the memory chunk from location 'l' of length 'sz'.
    auto pointers( Loc l, int sz )
    {
        return PointerC( _meta, *this, l.object, l.offset, l.offset + sz );
    }
};

}
