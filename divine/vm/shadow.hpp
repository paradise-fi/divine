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
#include <unordered_map>

#include <divine/vm/value.hpp>

namespace divine {
namespace vm {

namespace bitlevel = brick::bitlevel;
namespace mem = brick::mem;

template< typename InternalPtr >
struct InternalLoc : public brick::types::Ord
{
    InternalPtr object;
    int offset;
    InternalLoc( InternalPtr o, int off = 0 )
        : object( o ), offset( off )
    {}
    InternalLoc operator-( int i ) const { InternalLoc r = *this; r.offset -= i; return r; }
    InternalLoc operator+( int i ) const { InternalLoc r = *this; r.offset += i; return r; }
    bool operator==( const InternalLoc & o) const
    {
        return object == o.object && offset == o.offset;
    }
    bool operator<=(const InternalLoc & o) const
    {
        return object < o.object || (object == o.object && offset <= o.offset);
    }
    bool operator<(const InternalLoc & o) const
    {
        return object < o.object || (object == o.object && offset < o.offset);
    }
};

template< typename Proxy, typename Pool >
struct BitContainer
{
    struct iterator : std::iterator< std::forward_iterator_tag, Proxy >
    {
        uint8_t *_base; int _pos;
        Proxy operator*() const { return Proxy( _base, _pos ); }
        Proxy operator->() const { return Proxy( _base, _pos ); }
        iterator &operator++() { _pos ++; return *this; }
        iterator &operator+=( int off ) { _pos += off; return *this; }
        iterator operator+( int off ) const { auto r = *this; r._pos += off; return r; }
        iterator( uint8_t *b, int p ) : _base( b ), _pos( p ) {}
        bool operator!=( iterator o ) const { return _base != o._base || _pos != o._pos; }
        bool operator==( iterator o ) const { return _base == o._base && _pos == o._pos; }
        bool operator<( iterator o ) const { return _pos < o._pos; }
    };
    using Ptr = typename Pool::Pointer;

    Ptr _base;
    Pool &_pool;
    int _from, _to;

    BitContainer( Pool &p, Ptr b, int f, int t )
        : _base( b ), _pool( p ), _from( f ), _to( t )
    {}
    iterator begin() { return iterator( _pool.template machinePointer< uint8_t >( _base ), _from ); }
    iterator end() { return iterator( _pool.template machinePointer< uint8_t >( _base ), _to ); }

    Proxy operator[]( int i )
    {
        ASSERT_LT( _from + i, _to );
        return Proxy( _pool.template machinePointer< uint8_t >( _base ), _from + i );
    }
};

template< uint8_t PerBits >
struct _BitProxy
{
    static_assert( PerBits % 8 == 0, "BitProxy only supports whole bytes." );

    uint8_t *_base; int _pos;
    uint8_t mask() const { return uint8_t( 0x80 ) >> ( _pos % PerBits ); }
    uint8_t &word() const { return *( _base + ( _pos / PerBits ) ); };
    _BitProxy &operator=( const _BitProxy &o ) { return *this = bool( o ); }
    _BitProxy &operator=( bool b )
    {
        set( b );
        return *this;
    }
    bool get() const
    {
        return word() & mask();
    }
    void set( bool b )
    {
        uint8_t m = b * 0xff;
        word() &= ~mask();
        word() |= ( mask() & m );
    }
    operator bool() const { return get(); }
    bool operator==( const _BitProxy &o ) const { return get() == o.get(); }
    bool operator!=( const _BitProxy &o ) const { return get() != o.get(); }
    _BitProxy *operator->() { return this; }
    _BitProxy( uint8_t *b, int p )
        : _base( b ), _pos( p )
    {
    }
};

using BitProxy = _BitProxy< 8 >;

/* Type of each 4-byte word */
namespace ShadowType {
enum T
{
    Data = 0,         /// Generic data or offset of a pointer
    Pointer,          /// Object ID (4 most significant bytes) of a pointer
    DataException,    /// As 'Data', but with partially initialized bytes
    PointerException  /// Word containing parts of pointers, may contain bytes
                      /// of data (and these may be partially initialized).
};
}

inline std::ostream &operator<<( std::ostream &o, ShadowType::T t )
{
    switch ( t )
    {
        case ShadowType::Data: return o << "d";
        case ShadowType::Pointer: return o << "p";
        case ShadowType::DataException: return o << "D";
        case ShadowType::PointerException: return o << "P";
        default: return o << "?";
    }
}

struct DataException
{
    union {
        uint8_t bitmask[ 4 ];
        uint32_t bitmask_word;
    };

    bool valid() const { return bitmask_word != 0; }
    void invalidate() { bitmask_word = 0; }

    bool operator==( const DataException &o ) const { return bitmask_word == o.bitmask_word; }
    bool operator!=( const DataException &o ) const { return bitmask_word != o.bitmask_word; }
    bool operator-( const DataException &o ) const { return bitmask_word - o.bitmask_word; }

    bool bitmask_is_trivial() const {
        return bitmask_is_trivial( bitmask );
    }

    static bool bitmask_is_trivial( const uint8_t *m ) {
        for ( int i = 0; i < 4; ++i )
            if ( m[ i ] != 0x00 && m[ i ] != 0xff )
                return false;
        return true;
    }
};

inline std::ostream &operator<<( std::ostream &o, const DataException &e )
{
    if ( ! e.valid() )
    {
        o << "INVALID ";
    }

    o << " data exc.: ";
    for ( int i = 0; i < 4; ++i )
        o << std::hex << std::setw( 2 ) << std::internal
            << std::setfill( '0' ) << int( e.bitmask[ i ] ) << std::dec;

    return o;
}

struct PointerException
{
    uint32_t objid[ 4 ]; // Object IDs of the original pointers
    uint8_t index[ 4 ];  // Position of byte in the orig. pointers

    bool redundant() const
    {
        const uint8_t redundant_sequence[] { 3, 2, 1, 0 };
        return std::equal( index, index + 4, redundant_sequence )
            && objid[ 0 ] == objid[ 1 ]
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
            o << "byte " << +e.index[ i ] << " of ptr. to " << e.objid[ i ];
        else
            o << "data";
        o << " }\n";
    }

    o << "  }\n";

    return o;
}

template< typename MasterPool >
struct PooledShadow
{
    using Pool = mem::SlavePool< MasterPool >;
    using Internal = typename Pool::Pointer;
    using Loc = InternalLoc< Internal >;

    Pool _type,
         _defined,
         _shared;

    /* Relation between _type and _defined:
     * _type      _defined  byte contains
     * (per 4 B)  (per 1 B)
     * Data           1     Initialized data
     * Pointer        1     Initialized part of continuous object id
     * DataExcept     1     Initialized data byte
     * PointerExc     1     Initialized data byte or part of non-continuous object id
     * Data           0     Uninitialized data byte
     * Pointer        0     --
     * DataExcept     0     Uninitialized or per-bit initialized data
     * PointerExc     0     Uninitialized or per-bit initialized data
     */

    PooledShadow( const MasterPool &mp )
        : _type( mp ), _defined( mp ), _shared( mp ),
          _def_exceptions( new DataExceptions ), _ptr_exceptions( new PointerExceptions )
    {}

    template< typename ExceptionType >
    class ExceptionMap
    {
    public:
        using ExcMap = std::map< Loc, ExceptionType >;
        using Lock = std::lock_guard< std::mutex >;

        ExceptionMap &operator=( const ExceptionType & o ) = delete;

        ExceptionType at( Internal obj, int wpos )
        {
            ASSERT_EQ( wpos % 4, 0 );

            Lock lk( _mtx );

            auto it = _exceptions.find( Loc( obj, wpos ) );
            ASSERT( it != _exceptions.end() );
            ASSERT( it->second.valid() );
            return it->second;
        }

        void set( Internal obj, int wpos, const ExceptionType &exc )
        {
            ASSERT_EQ( wpos % 4, 0 );

            Lock lk( _mtx );
            _exceptions[ Loc( obj, wpos ) ] = exc;
        }

        void invalidate( Internal obj, int wpos )
        {
            ASSERT_EQ( wpos % 4, 0 );

            Lock lk( _mtx );

            auto it = _exceptions.find( Loc( obj, wpos ) );

            ASSERT( it != _exceptions.end() );
            ASSERT( it->second.valid() );

            it->second.invalidate();
        }

        void free( Internal obj )
        {
            Lock lk( _mtx );

            auto lb = _exceptions.lower_bound( Loc( obj, 0 ) );
            auto ub = _exceptions.upper_bound( Loc( obj, (1 << _VM_PB_Off) - 1 ) );
            while (lb != ub)
            {
                lb->second.invalidate();
                ++lb;
            }
        }

        bool empty()
        {
            Lock lk( _mtx );
            return std::all_of( _exceptions.begin(), _exceptions.end(),
                                []( const auto & e ) { return ! e.second.valid(); } );
        }

    protected:
        ExcMap _exceptions;
        mutable std::mutex _mtx;
    };

    class DataExceptions : public ExceptionMap< DataException >
    {
    public:
        using Base = ExceptionMap< DataException >;

    private:
        using Lock = typename Base::Lock;
        using Base::_exceptions;
        using Base::_mtx;

    public:
        void dump() const
        {
            std::cout << "exceptions: {\n";
            for ( auto &e : _exceptions )
            {
                std::cout << "  {" << e.first.object._raw << " + "
                   << e.first.offset << ": " << e.second << "}\n";
            }
            std::cout << "}\n";
        }

        using Base::set;

        void set( Internal obj, int wpos, const uint8_t *mask )
        {
            ASSERT_EQ( wpos % 4, 0 );

            Lock lk( _mtx );
            auto & exc = _exceptions[ Loc( obj, wpos ) ];
            std::copy( mask, mask + 4, exc.bitmask );
        }

        void get( Internal obj, int wpos, uint8_t *mask_dst )
        {
            ASSERT_EQ( wpos % 4, 0 );

            Lock lk( _mtx );

            auto it = _exceptions.find( Loc( obj, wpos ) );

            ASSERT( it != _exceptions.end() );
            ASSERT( it->second.valid() );

            std::copy( it->second.bitmask, it->second.bitmask + 4, mask_dst );
        }

        /** Which bits of 'pos'th byte in pool object 'obj' are initialized */
        uint8_t defined( Internal obj, int pos )
        {
            Lock lk( _mtx );

            int wpos = ( pos / 4 ) * 4;
            auto it = _exceptions.find( Loc( obj, wpos ) );
            if ( it != _exceptions.end() && it->second.valid() )
            {
                return it->second.bitmask[ pos % 4 ];
            }
            return 0x00;
        }
    };

    class PointerExceptions : public ExceptionMap< PointerException >
    {
    public:
        using Base = ExceptionMap< PointerException >;

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

    std::shared_ptr< DataExceptions > _def_exceptions;
    std::shared_ptr< PointerExceptions > _ptr_exceptions;

    struct TypeProxy
    {
        uint8_t *_base; int _pos;
        int shift() const { return 2 * ( ( _pos % 16 ) / 4 ); }
        uint8_t mask() const { return uint8_t( 0b11 ) << shift(); }
        uint8_t &word() const { return *( _base + _pos / 16 ); }
        TypeProxy &operator=( const TypeProxy &o ) { return *this = ShadowType::T( o ); }
        TypeProxy &operator=( ShadowType::T st )
        {
            set(st);
            return *this;
        }
        ShadowType::T get() const
        {
            return ShadowType::T( ( word() & mask() ) >> shift() );
        }
        void set( ShadowType::T st )
        {
            word() &= ~mask();
            word() |= uint8_t( st ) << shift();
        }
        bool is_exception() const
        {
            return get() == ShadowType::PointerException || get() == ShadowType::DataException;
        }
        operator ShadowType::T() const { return get(); }
        bool operator==( const TypeProxy &o ) const { return get() == o.get(); }
        bool operator!=( const TypeProxy &o ) const { return get() != o.get(); }
        TypeProxy *operator->() { return this; }
        TypeProxy( uint8_t *b, int p )
            : _base( b ), _pos( p )
        {}
    };

    using TypeC = BitContainer< TypeProxy, Pool >;

    struct DefinedC
    {
        struct proxy
        {
            DefinedC *_parent;
            uint8_t *_base;
            uint8_t *_type_base;
            int _pos;
            DataExceptions &exceptions() const { return _parent->_exceptions; }
            uint8_t mask() const { return uint8_t( 0x80 ) >> ( _pos % 8 ); }
            uint8_t &word() const { return *( _base + ( _pos / 8 ) ); };
            proxy *operator->() { return this; }
            operator uint8_t() const
            {
                if ( word() & mask() )
                    return 0xFF;

                if ( TypeProxy( _type_base, _pos ).is_exception() )
                    return exceptions().defined( _parent->_base, _pos );

                return 0x00;
            }
            bool raw() const
            {
                return word() & mask();
            }
            bool operator==( const proxy &o ) const { return uint8_t( *this ) == uint8_t( o ); }
            bool operator!=( const proxy &o ) const { return ! ( *this == o ); }
            proxy( DefinedC *c, uint8_t *b, uint8_t *tb, int p )
                : _parent( c ), _base( b ), _type_base( tb ), _pos( p )
            {}
        };

        struct iterator : std::iterator< std::forward_iterator_tag, proxy >
        {
            DefinedC *_parent;
            uint8_t *_base;
            uint8_t *_type_base;
            int _pos;

            iterator &operator++() { ++_pos; return *this; }
            iterator &operator+=( int off ) { _pos += off; return *this; }
            proxy operator*() const { return proxy( _parent, _base, _type_base, _pos ); }
            proxy operator->() const { return proxy( _parent, _base, _type_base, _pos ); }
            bool operator==( iterator o ) const {
                return _parent == o._parent
                    && _base == o._base
                    && _type_base == o._type_base
                    && _pos == o._pos;
            }
            bool operator!=( iterator o ) const { return ! ( *this == o ); }
            bool operator<( iterator o ) const { return _pos < o._pos; }
            iterator( DefinedC *c, uint8_t *b, uint8_t *tb, int p )
                : _parent( c ), _base( b ), _type_base( tb ), _pos( p )
            {}
        };

        Internal _base;
        Pool & _sh_defined;
        Pool & _sh_type;
        DataExceptions & _exceptions;
        int _from;
        int _to;

        DefinedC(Pool &def, Pool &type, DataExceptions &exc, Internal base, int from, int to)
            : _base(base), _sh_defined(def), _sh_type(type), _exceptions(exc), _from(from), _to(to)
        {}
        iterator begin() { return iterator( this, _sh_defined.template machinePointer< uint8_t >( _base ),
                _sh_type.template machinePointer< uint8_t >( _base ), _from ); }
        iterator end() { return iterator( this, _sh_defined.template machinePointer< uint8_t >( _base ),
                _sh_type.template machinePointer< uint8_t >( _base ), _to ); }
        proxy operator[]( int i )
        {
            return proxy( this, _sh_defined.template machinePointer< uint8_t >( _base ),
                    _sh_type.template machinePointer< uint8_t >( _base ), _from + i );
        }
    };

    struct PointerC
    {
        using t_iterator = typename TypeC::iterator;
        struct proxy
        {
            PointerC *_parent;
            t_iterator _i;
            proxy( PointerC * p, t_iterator i ) : _parent( p ), _i( i )
            {
                ASSERT( *i & ShadowType::Pointer || *(i + 4) == ShadowType::Pointer );
            }
            proxy *operator->() { return this; }
            int offset() const { return _i._pos - _parent->types.begin()._pos; }
            int size() const
            {
                if ( *_i == ShadowType::Pointer )
                    return 4;
                if ( ( *_i & ShadowType::Pointer ) == 0 && *(_i + 4) == ShadowType::Pointer )
                    return 8;
                if ( *_i == ShadowType::PointerException )
                    return 1;
                NOT_IMPLEMENTED();
            }
            bool operator==( const proxy &o ) const
            {
                return offset() == o.offset() && size() == o.size();
            }
            uint32_t fragment() const
            {
                ASSERT_EQ( *_i, ShadowType::PointerException );
                return _parent->_exceptions.at( _parent->obj, bitlevel::downalign( _i._pos, 4 ) )
                    .objid[ _i._pos % 4 ];
            }
        };

        struct iterator : std::iterator< std::forward_iterator_tag, proxy >
        {
            PointerC *_parent;
            t_iterator _self;
            void seek()
            {
                if ( _self == _parent->types.end() )
                    return;

                if ( *_self == ShadowType::PointerException )
                {
                    PointerException exc = _parent->_exceptions.at( _parent->obj,
                            bitlevel::downalign( _self._pos, 4 ) );

                    do {
                        if ( exc.objid[ _self._pos % 4 ] )
                            return;
                        ++_self;
                    } while ( _self._pos % 4 );
                }

                while ( _self < _parent->types.end() &&
                        ( *_self & ShadowType::Pointer ) == 0 &&
                        ( ! ( _self + 4 < _parent->types.end() ) ||
                          *(_self + 4) != ShadowType::Pointer ) )
                    _self = _self + 4;

                if ( ! ( _self < _parent->types.end() ) )
                {
                    _self = _parent->types.end();
                    return;
                }

                if ( *_self == ShadowType::PointerException )
                    seek();
            }
            iterator &operator++()
            {
                _self = _self + (*this)->size();
                seek();
                return *this;
            }
            proxy operator*() const { return proxy( _parent, _self ); }
            proxy operator->() const { return proxy( _parent, _self ); }
            bool operator!=( iterator o ) const { return _parent != o._parent || _self != o._self; }
            bool operator==( iterator o ) const { return _parent == o._parent && _self == o._self; }
            iterator( PointerC *p, t_iterator s )
                : _parent( p ), _self( s )
            {}
        };

        TypeC types;
        Internal obj;
        PointerExceptions & _exceptions;
        iterator begin() { auto b = iterator( this, types.begin() ); b.seek(); return b; }
        iterator end() { return iterator( this, types.end() ); }
        proxy atoffset( int i ) { return *iterator( types.begin() + i ); }
        PointerC( Pool &p, PointerExceptions &pexcs, Internal i, int f, int t )
            : types( p, i, f, t ), obj( i ), _exceptions( pexcs )
        {}
    };

    void make( Internal p, int size )
    {
        /* types: 2 bits per word (= 1/2 bit per byte), defined: 1 bit per byte */
        _type.materialise( p, ( size / 16 ) + ( size % 16 ? 1 : 0 ) );
        _defined.materialise( p, ( size / 8 )  + ( size % 8  ? 1 : 0 ));
        _shared.materialise( p, 1 );
    }

    void free( Internal p )
    {
        _def_exceptions->free( p );
        _ptr_exceptions->free( p );
    }

    auto type( Loc l, int sz )
    {
        return TypeC( _type, l.object, l.offset, l.offset + sz );
    }
    auto defined( Loc l, int sz )
    {
        return DefinedC( _defined, _type, *_def_exceptions, l.object, l.offset, l.offset + sz );
    }
    auto pointers( Loc l, int sz )
    {
        return PointerC( _type, *_ptr_exceptions, l.object, l.offset, l.offset + sz );
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
    int compare( OtherSH & a_sh, typename OtherSH::Internal a, Internal b, int sz )
    {
        auto _ty_a = a_sh._type.template machinePointer< uint8_t >( a ),
             _ty_b = _type.template machinePointer< uint8_t >( b );

        int cmp;

        if ( ( cmp = ::memcmp( _ty_a, _ty_b, sz / 16 ) ) )
            return cmp;

        for ( int off = sz - sz % 16; off < sz; off += 4 ) /* the tail */
            if ( ( cmp = TypeProxy( _ty_a, off ) - TypeProxy( _ty_b, off ) ) )
                return cmp;

        auto _def_a = a_sh._defined.template machinePointer< uint8_t >( a ),
             _def_b = _defined.template machinePointer< uint8_t >( b );

        if ( ( cmp = ::memcmp( _def_a, _def_b, sz / 8 ) ) )
            return cmp;

        for ( int off = sz - sz % 8; off < sz; off ++ )
            if ( ( cmp = int( BitProxy( _def_a, off ) ) - int( BitProxy( _def_b, off ) ) ) )
                return cmp;

        int off = 0;
        for ( ; off < bitlevel::downalign( sz, 4 ); off += 4 )
            if ( TypeProxy( _ty_a, off ) & ShadowType::DataException
                    && ( cmp = a_sh._def_exceptions->at( a, off )
                        - _def_exceptions->at( b, off ) ) )
                return cmp;

        if ( off < sz && TypeProxy( _ty_a, off ) & ShadowType::DataException )
            for ( ; off < sz; ++off )
                if ( ( cmp = a_sh._def_exceptions->defined( a, off )
                            - _def_exceptions->defined( b, off ) ) )
                    return cmp;

        return 0;
    }

    template< typename V >
    void write( Loc l, V value )
    {
        const int sz = sizeof( typename V::Raw );

        if ( sz >= 4 )
            ASSERT_EQ( l.offset % 4, 0 );
        else if ( sz == 2 )
            ASSERT_LT( l.offset % 4, 3 );
        else
            ASSERT_EQ( sz, 1 );

        auto obj = l.object;

        auto _ty = _type.template machinePointer< uint8_t >( obj );
        auto _def = _defined.template machinePointer< uint8_t >( obj );

        union
        {
            typename V::Raw _def_mask;
            uint8_t _def_bytes[ sz ];
        };

        _def_mask = value.defbits();

        int off = 0;

        if ( sz >= 4 )
            for ( ; off < bitlevel::downalign( sz, 4 ); off += 4 )
                _write_def( _def_bytes + off, _def, _ty, obj, l.offset + off );

        if ( sz % 4 )
        {
            uint8_t tail_def[4];
            auto tail_off = bitlevel::downalign( l.offset + off, 4 );

            _read_def( tail_def, _def, _ty, obj, tail_off );

            std::copy( _def_bytes + off, _def_bytes + sz,
                    tail_def + ( ( l.offset + off ) % 4 ) );

            _write_def( tail_def, _def, _ty, obj, tail_off );
        }

        if ( sz == PointerBytes )
            if ( value.pointer() && l.offset % 4 == 0 )
                TypeProxy( _ty, l.offset + 4 ) = ShadowType::Pointer;
    }

    template< typename V >
    void read( Loc l, V &value )
    {
        constexpr int sz = sizeof( typename V::Raw );

        if ( sz >= 4 )
            ASSERT_EQ( l.offset % 4, 0 );
        else if ( sz == 2 )
            ASSERT_LT( l.offset % 4, 3 );
        else
            ASSERT_EQ( sz, 1 );

        auto obj = l.object;

        auto _ty = _type.template machinePointer< uint8_t >( obj );
        auto _def = _defined.template machinePointer< uint8_t >( obj );

        union
        {
            typename V::Raw _def_mask;
            uint8_t _def_bytes[ sz ];
        };

        int off = 0;

        if ( sz >= 4 )
            for ( ; off < bitlevel::downalign( sz, 4 ); off += 4 )
                _read_def( _def_bytes + off, _def, _ty, obj, l.offset + off );

        if ( sz % 4 )
        {
            bool is_exc = TypeProxy( _ty, l.offset ).is_exception();
            for ( ; off < sz; ++off )
                _def_bytes[ off ] = is_exc ? _def_exceptions->defined( obj, l.offset + off )
                                           : BitProxy( _def, l.offset + off ).get() * 0xff;
        }

        value.defbits( _def_mask );

        if ( sz == PointerBytes )
            value.pointer( TypeProxy( _ty, l.offset + 4 ) == ShadowType::Pointer );
    }

    template< typename FromSh, typename FromHeapReader, typename ToHeapReader >
    void copy( FromSh &from_sh, typename FromSh::Loc from, Loc to, int sz,
               FromHeapReader fhr, ToHeapReader thr )
    {
        if ( sz == 0 )
            return;

        ASSERT_LT( 0, sz );

        auto _ty_from = from_sh._type.template machinePointer< uint8_t >( from.object );
        auto _def_from = from_sh._defined.template machinePointer< uint8_t >( from.object );

        auto _ty_to = _type.template machinePointer< uint8_t >( to.object );
        auto _def_to = _defined.template machinePointer< uint8_t >( to.object );

        int off = 0;

        if ( sz >= 4 && from.offset % 4 == to.offset % 4 )
        {
            ASSERT_EQ( from.offset % 4, 0 );

            for ( ; off < bitlevel::downalign( sz, 4 ); off += 4 )
            {
                auto st_from = TypeProxy( _ty_from, from.offset + off ).get();
                auto st_to = TypeProxy( _ty_to, to.offset + off ).get();

                uint8_t shadow_word_from = BitProxy( _def_from, from.offset + off ).word();
                uint8_t shadow_shift_from = ( from.offset + off ) % 8;
                uint8_t shadow_word_to = BitProxy( _def_to, to.offset + off ).word();
                uint8_t shadow_shift_to = ( to.offset + off ) % 8;

                shadow_word_to &= ( 0x0f << shadow_shift_to );
                shadow_word_from <<= shadow_shift_from;
                shadow_word_from &= 0xf0;
                shadow_word_to |= ( shadow_word_from >> shadow_shift_to );
                BitProxy( _def_to, to.offset + off ).word() = shadow_word_to;

                if ( st_from & ShadowType::DataException )
                    _def_exceptions->set( to.object, to.offset + off,
                            from_sh._def_exceptions->at( from.object, from.offset + off ) );
                else if ( st_to & ShadowType::DataException )
                    _def_exceptions->invalidate( to.object, to.offset + off );

                if ( st_from == ShadowType::PointerException )
                    _ptr_exceptions->set( to.object, to.offset + off,
                            from_sh._ptr_exceptions->at( from.object, from.offset + off ) );
                else if ( st_to == ShadowType::PointerException )
                    _ptr_exceptions->invalidate( to.object, to.offset + off );

                TypeProxy( _ty_to, to.offset + off ).set( st_from );
            }
        }

        if ( off < sz ) // unaligned and tail of aligned copy
        {
            uint8_t current_def_from[ 4 ];
            uint8_t current_def_to[ 4 ];

            int off_from = from.offset + off,
                off_to = to.offset + off;
            bool def_written = false;

            PointerException current_ptr_from,
                             current_ptr_to;

            if ( off_from % 4 )
            {
                int aligned = bitlevel::downalign( off_from, 4 );
                from_sh._read_def( current_def_from, _def_from, _ty_from, from.object, aligned );
                current_ptr_from = from_sh._read_ptr( _ty_from, from.object, aligned, fhr );
            }
            if ( off_to % 4 )
            {
                int aligned = bitlevel::downalign( off_to, 4 );
                _read_def( current_def_to, _def_to, _ty_to, to.object, aligned );
                current_ptr_to = _read_ptr( _ty_to, to.object, aligned, thr );
            }

            while ( off < sz )
            {
                if ( off_from % 4 == 0 )
                {
                    from_sh._read_def( current_def_from, _def_from, _ty_from, from.object, off_from );
                    current_ptr_from = from_sh._read_ptr( _ty_from, from.object, off_from, fhr );
                }
                if ( off_to % 4 == 0 )
                {
                    if ( sz - off < 4 )
                    {
                        _read_def( current_def_to, _def_to, _ty_to, to.object, off_to );
                        current_ptr_to = _read_ptr( _ty_to, to.object, off_to, thr );
                    }
                    def_written = false;
                }

                current_def_to[ off_to % 4 ] = current_def_from[ off_from % 4 ];
                current_ptr_to.objid[ off_to % 4 ] = current_ptr_from.objid[ off_from % 4 ];
                current_ptr_to.index[ off_to % 4 ] = current_ptr_from.index[ off_from % 4 ];

                ++off;
                ++off_from;
                ++off_to;

                if ( off_to % 4 == 0 )
                {
                    auto st = _write_ptr( current_ptr_to, to.object, off_to - 4 );
                    _write_def( current_def_to, _def_to, _ty_to, to.object, off_to - 4, st );
                    def_written = true;
                }
            }

            if ( ! def_written )
            {
                int aligned = bitlevel::downalign( off_to, 4 );
                auto st = _write_ptr( current_ptr_to, to.object, aligned );
                _write_def( current_def_to, _def_to, _ty_to, to.object, aligned, st );
            }
        }
    }

    template< typename HeapReader >
    void copy( Loc from, Loc to, int sz, HeapReader hr ) {
        return copy( *this, from, to, sz, hr, hr );
    }

    void _read_def( uint8_t *dst, uint8_t *_def, uint8_t *_ty, Internal obj, int off )
    {
        ASSERT_EQ( off % 4, 0 );

        auto st = TypeProxy( _ty, off ).get();
        if ( st & ShadowType::DataException )
            _def_exceptions->get( obj, off, dst );
        else if ( st == ShadowType::Pointer )
        {
            for ( int i = 0; i < 4; ++i )
                dst[ i ] = 0xff;
        }
        else
        {
            uint8_t shadow_word = BitProxy( _def, off ).word();

            shadow_word >>= 4 - off % 8;
            shadow_word &= 0x0f;

            ASSERT_LT( shadow_word, 0x10 );

            alignas( 4 ) const unsigned char mask_table[] = {
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0xff,
                0x00, 0x00, 0xff, 0x00,
                0x00, 0x00, 0xff, 0xff,
                0x00, 0xff, 0x00, 0x00,
                0x00, 0xff, 0x00, 0xff,
                0x00, 0xff, 0xff, 0x00,
                0x00, 0xff, 0xff, 0xff,
                0xff, 0x00, 0x00, 0x00,
                0xff, 0x00, 0x00, 0xff,
                0xff, 0x00, 0xff, 0x00,
                0xff, 0x00, 0xff, 0xff,
                0xff, 0xff, 0x00, 0x00,
                0xff, 0xff, 0x00, 0xff,
                0xff, 0xff, 0xff, 0x00,
                0xff, 0xff, 0xff, 0xff,
            };

            auto mask = mask_table + 4 * shadow_word;
            std::copy( mask, mask + 4, dst );
        }
    }

    void _write_def( uint8_t *src, uint8_t *_def, uint8_t *_ty, Internal obj, int off,
            ShadowType::T st = ShadowType::Data )
    {
        ASSERT_EQ( off % 4, 0 );

        auto old_type = TypeProxy( _ty, off ).get();

        uint8_t shadow_word = BitProxy( _def, off ).word();
        uint8_t shadow_mask = BitProxy( _def, off ).mask();
        ASSERT_EQ( shadow_mask & 0x77, 0x00 );
        ASSERT( shadow_mask );

        for ( int i = 0; i < 4; ++i )
        {
            shadow_word &= ~shadow_mask;
            shadow_word |= ( src[ i ] == 0xff ? shadow_mask : 0x00 );
            shadow_mask >>= 1;
        }
        BitProxy( _def, off ).word() = shadow_word;

        if ( ! DataException::bitmask_is_trivial( src ) )
            st = ShadowType::T( st | ShadowType::DataException );
        TypeProxy( _ty, off ) = st;

        if ( st & ShadowType::DataException )
            _def_exceptions->set( obj, off, src );
        else if ( old_type & ShadowType::DataException )
            _def_exceptions->invalidate( obj, off );

        if ( old_type == ShadowType::PointerException && st != ShadowType::PointerException )
            _ptr_exceptions->invalidate( obj, off );
    }

    template< typename HeapReader >
    PointerException _read_ptr(uint8_t *_ty, Internal obj, int off, HeapReader hr )
    {
        ASSERT_EQ( off % 4, 0 );

        auto st = TypeProxy( _ty, off );
        if ( ! ( st & ShadowType::Pointer ) )
            return PointerException::null();
        if ( st == ShadowType::PointerException )
            return _ptr_exceptions->at( obj, off );

        PointerException e;
        uint32_t objid = hr( obj, off );
        std::fill( e.objid, e.objid + 4, objid );
        const uint8_t redundant_sequence[] { 3, 2, 1, 0 };
        std::copy( redundant_sequence, redundant_sequence + 4, e.index );
        return e;
    }

    ShadowType::T _write_ptr( const PointerException &exc, Internal obj, int off)
    {
        ASSERT_EQ( off % 4, 0 );

        if ( ! exc.valid() )
            return ShadowType::Data;
        if ( exc.redundant() )
            return ShadowType::Pointer;
        else
        {
            _ptr_exceptions->set( obj, off, exc );
            return ShadowType::PointerException;
        }
    }

    void dump( std::string what, Loc l, int sz )
    {
        auto _ty = _type.template machinePointer< uint8_t >( l.object );
        auto _def = _defined.template machinePointer< uint8_t >( l.object );

        std::vector< PointerException > pexcs;

        std::cerr << what << ", obj = " << l.object << ", off = " << l.offset << ", type: ";
        for ( int i = 0; i < sz; ++i )
        {
            auto t = TypeProxy( _ty, l.offset + i ).get();
            std::cerr << t;
            if ( (l.offset + i ) % 4 == 0 && t == ShadowType::PointerException )
                pexcs.push_back( _ptr_exceptions->at( l.object, l.offset + i ) );
        }
        std::cerr << ", def:" << std::hex << std::fixed;
        for ( int i = 0; i < sz; ++i )
        {
            uint8_t def = BitProxy( _def, l.offset + i ).get() * 0xff;
            if ( def == 0x00 && TypeProxy( _ty, l.offset + i ).is_exception() )
                def = _def_exceptions->defined( l.object, l.offset + i );
            std::cerr << ' ' << +def;
        }
        for ( auto & pexc : pexcs )
            std::cerr << pexc << std::endl;
        std::cerr << std::dec << std::endl;
    }
};

template< template < typename > class SlaveShadow, typename MasterPool,
          uint8_t TaintBit = 0, uint8_t PerBytes = 1 >
struct PooledTaintShadow : public SlaveShadow< MasterPool >
{
    static_assert( PerBytes > 0 && PerBytes < 8, "unsupported width of PooledTaintShadow" );
    static constexpr uint8_t PerBits = 8 * PerBytes;
    static constexpr uint8_t TaintMask = ( 1 << TaintBit );

    using NextShadow = SlaveShadow< MasterPool >;
    using Pool = mem::SlavePool< MasterPool >;
    using Internal = typename Pool::Pointer;
    using Loc = InternalLoc< Internal >;

    Pool _taints;

    PooledTaintShadow( const MasterPool &mp )
        : NextShadow( mp )
        , _taints( mp ) {}

    void make( Internal p, int size )
    {
        _taints.materialise( p, ( size / PerBits )  + ( size % PerBits  ? 1 : 0 ));
        NextShadow::make( p, size );
    }

    using NextShadow::free;

    using TaintC = BitContainer< _BitProxy< PerBits >, Pool >;

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
        int cmp;
        auto a_ts = a_sh.taints( a, sz );
        auto b_t = taints( b, sz ).begin();
        for ( auto a_t : a_ts )
            if ( ( cmp = int( a_t.get() ) - b_t.get() ) )
                return cmp;

        return NextShadow::compare( a_sh, a, b, sz );
    }

    template< typename V >
    void write( Loc l, V value )
    {
        const int sz = sizeof( typename V::Raw );
        for ( auto t : taints( l, sz ) )
            t.set( value.taints() & TaintMask );

        NextShadow::write( l, value );
    }

    template< typename V >
    void read( Loc l, V &value )
    {
        constexpr int sz = sizeof( typename V::Raw );
        for ( auto t : taints( l, sz ) )
            if ( t.get() )
                value.taints( value.taints() | TaintMask );
            else
                value.taints( value.taints() & ~TaintMask );

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
        std::copy( from_ts.begin(), from_ts.end(), taints( to, sz ).begin() );

        NextShadow::copy( from_sh, from, to, sz, fhr, thr );
    }

    template< typename HeapReader >
    void copy( Loc from, Loc to, int sz, HeapReader hr ) {
        copy( *this, from, to, sz, hr, hr );
    }

};

}

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
    auto taints( Ptr p, int sz ) { return shadows.taints( shloc( p, 0 ), sz ); }
    Loc shloc( Ptr p, int off ) { return Loc( p, off ); }

    Ptr make( int sz )
    {
        auto r = pool.allocate( sizeof( Ptr ) );
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

struct PooledShadow
{
    using PointerV = vm::value::Pointer;
    using H = NonHeap< vm::PooledShadow >;
    H heap;
    H::Ptr obj;

    PooledShadow() { obj = heap.make( 100 ); }

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
        vm::value::Int< 16 > i1( 32, 0xFFFF, false ), i2;
        heap.write( obj, 0, i1 );
        heap.read( obj, 0, i2 );
        ASSERT( i2.defined() );
    }

    TEST( copy_int )
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
        heap.write( obj, 0, p1 );
        int count = 0;
        for ( auto x : heap.pointers( obj, 100 ) )
            static_cast< void >( x ), count ++;
        ASSERT_EQ( count, 1 );
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

    TEST( copy_partially_initialized )
    {
        vm::value::Int< 16 > i1( 32, 0x0AFF, false ), i2;
        vm::value::Int< 64 > l1( 99, 0xDEADBEEF'0FF0FFFF, false ), l2;
        heap.write( obj, 0, i1 );
        heap.copy( obj, 0, obj, 2, 2 );
        heap.read( obj, 2, i2 );
        ASSERT_EQ( i2.defbits(), 0x0AFF );

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
        ASSERT( heap.shadows._ptr_exceptions->empty() );
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

        ASSERT( heap.shadows._ptr_exceptions->empty() );
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

struct PooledTaintShadow
{
    template< uint8_t S >
    using Int = vm::value::Int< S >;
    template< typename MasterPool >
    using Sh = vm::PooledTaintShadow< vm::PooledShadow, MasterPool, 0, 1 >;
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

        i1.taints( 0x1 );
        heap.write( obj, 2, i1 );
        heap.read( obj, 2, i2 );
        ASSERT_EQ( i2.taints(), 0x01 );
    }

    TEST( ignore_other_taints )
    {
        Int< 16 > i1( 42, 0xFFFF, false ),
                  i2;
        i1.taints( 0x7 );
        heap.write( obj, 2, i1 );
        heap.read( obj, 2, i2 );
        ASSERT_EQ( i2.taints(), 0x01 );
    }

    TEST( read_heterogeneous )
    {
        Int< 16 > i1( 42, 0xFFFF, false );
        Int< 32 > i2;
        heap.write( obj, 0, i1 );
        i1.taints( 0x1 );
        heap.write( obj, 2, i1 );
        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.taints(), 0x01 );
    }

    TEST( copy )
    {
        Int< 16 > i1( 42, 0xFFFF, false ),
                  i2;
        heap.write( obj, 0, i1 );
        i1.taints( 0x1 );
        heap.write( obj, 2, i1 );
        heap.copy( obj, 0, obj, 4, 4 );
        heap.read( obj, 0, i2 );
        ASSERT_EQ( i2.taints(), 0x00 );
        heap.read( obj, 2, i2 );
        ASSERT_EQ( i2.taints(), 0x01 );
    }

    TEST( iterators )
    {
        Int< 16 > i1( 42, 0xFFFF, false );
        heap.write( obj, 0, i1 );
        i1.taints( 0x1 );
        heap.write( obj, 2, i1 );
        int i = 0;
        for ( auto t : heap.taints( obj, 6 ) )
        {
            uint8_t expected = ( i % 4 < 2) ? 0x00 : 0x01;
            ASSERT_EQ( t, expected );
            ++i;
        }
    }

};

}

}
