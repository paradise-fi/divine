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
struct InObject
{
    InternalPtr object;
    int offset, size;
    InObject( InternalPtr p, int o, int s ) : object( p ), offset( o ), size( s ) {}
};

struct InVoid {};
struct InValue {};

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

template< typename IP >
using PointerLocation = brick::types::Union< InObject< IP >, InVoid, InValue >;

/* Type of each 4-byte word */
enum ShadowType
{
    Data = 0,         /// Generic data or offset of a pointer
    Pointer,          /// Object ID (4 most significant bytes) of a pointer
    DataException,    /// As 'Data', but with partially initialized bytes
    PointerException  /// Word containing parts of pointers, may contain bytes
                      /// of data (and these may be partially initialized).
};

enum ExceptionType
{
    PointerExc = 0,
    DataExc = 1
};

inline std::ostream &operator<<( std::ostream &o, ShadowType t )
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

inline std::ostream &operator<<( std::ostream &o, ExceptionType t )
{
    switch ( t )
    {
        case ExceptionType::DataExc: return o << "data";
        case ExceptionType::PointerExc: return o << "pointer";
        default: return o << "?";
    }
}

union ShadowException
{
    struct
    {
        uint32_t obj;              // Object ID of the original pointer
        ExceptionType type    : 1; // Pointer Exception
        uint8_t valid         : 1;
        uint8_t byte_index    : 2; // Position of byte in the orig. pointer
    };

    struct
    {
        uint8_t bitmask[4];        // Definedness of the word
        ExceptionType /*type*/: 1; // Data Exception
        uint8_t /*valid*/     : 1;
    };

    bool bitmask_is_trivial() const {
        for ( int i = 0; i < 4; ++i )
            if ( bitmask[ i ] != 0x00 && bitmask[ i ] != 0xff )
                return false;
        return true;
    }
};

inline std::ostream &operator<<( std::ostream &o, const ShadowException &e )
{
    if ( !e.valid )
    {
        o << "INVALID ";
    }

    o << e.type << " exc.: ";
    if ( e.type == ExceptionType::DataExc )
    {
        for ( int i = 0; i < 4; ++i )
            o << std::hex << std::setw( 2 ) << std::internal
                << std::setfill( '0' ) << int( e.bitmask[ i ] ) << std::dec;
    } else {
        o << "UNIMPLEMENTED";
    }

    return o;
}

template< typename MasterPool >
struct PooledShadow
{
    using InObj = InObject< typename MasterPool::Pointer >;
    using Pool = mem::SlavePool< MasterPool >;
    using Internal = typename Pool::Pointer;

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
        : _type( mp ), _defined( mp ), _shared( mp ), _exceptions( new Exceptions )
    {}

    struct Loc : public brick::types::Ord
    {
        Internal object;
        int offset;
        Loc( Internal o, int off = 0 )
            : object( o ), offset( off )
        {}
        Loc operator-( int i ) const { Loc r = *this; r.offset -= i; return r; }
        Loc operator+( int i ) const { Loc r = *this; r.offset += i; return r; }
        bool operator==( const Loc & o) const
        {
            return object == o.object && offset == o.offset;
        }
        bool operator<=(const Loc & o) const
        {
            return object < o.object || (object == o.object && offset <= o.offset);
        }
        bool operator<(const Loc & o) const
        {
            return object < o.object || (object == o.object && offset < o.offset);
        }
    };

    struct Exceptions
    {
        using ExcMap = std::multimap< Loc, ShadowException >;
        using Lock = std::lock_guard< std::mutex >;

        Exceptions &operator=( const Exceptions & o ) = delete;

        void dump() const
        {
            std::cout << "exceptions: {\n";
            for ( auto &e : _map )
            {
                std::cout << "  {" << e.first.object._raw << " + "
                   << e.first.offset << ": " << e.second << "}\n";
            }
            std::cout << "}\n";
        }

        /** Which bits of 'pos'th byte in pool object 'obj' are initialized */
        uint8_t defined( Internal obj, int pos )
        {
            Lock lk( _mtx );

            int wpos = ( pos / 4 ) * 4;
            auto range = _map.equal_range( Loc( obj, wpos ) );
            while ( range.first != range.second )
            {
                if ( range.first->second.valid && range.first->second.type == ExceptionType::DataExc )
                {
                    return range.first->second.bitmask[ pos % 4 ];
                }
                ++range.first;
            }
            return 0x00;
        }

        /** Change definedness mask of 'pos'th byte in pool object 'obj' to 'def'
         * @return true, if there is an exception in definedness */
        bool definedness_changed( Internal obj, int pos, uint8_t def,
                uint8_t shword, bool exc_should_exist )
        {
            Lock lk( _mtx );

            int wpos = ( pos / 4 ) * 4;
            auto range = _map.equal_range( Loc( obj, wpos ) );

            while ( range.first != range.second )
            {
                if ( range.first->second.type == ExceptionType::DataExc )
                    break;
                ++range.first;
            }

            bool exc_exists = range.first != range.second;
            bool def_trivial = def == 0x00 || def == 0xff;

            ASSERT( ( exc_exists && range.first->second.valid ) == exc_should_exist );

            if ( def_trivial && (! exc_exists || ! range.first->second.valid) )
                return false;

            if ( ! exc_exists )
            {
                // Create new data exception
                ShadowException e;
                e.type = ExceptionType::DataExc;
                e.valid = false;
                range.first = _map.insert( range.second, std::make_pair( Loc( obj, wpos ), e ));
            }

            auto & exc = range.first->second;

            if ( ! exc.valid )
            {
                // Load current state
                set_word_definedness( exc.bitmask, shword, wpos );
                exc.valid = true;
            }

            exc.bitmask[ pos % 4 ] = def;

            // Check whether the exception is needed
            if ( def_trivial && exc_exists && exc.bitmask_is_trivial() )
                exc.valid = false;

            return exc.valid;
        }

        void free( Internal obj )
        {
            Lock lk( _mtx );

            auto lb = _map.lower_bound( Loc( obj, 0 ) );
            auto ub = _map.upper_bound( Loc( obj, (1 << _VM_PB_Off) - 1 ) );
            while (lb != ub)
            {
                lb->second.valid = false;
                ++lb;
            }
        }

    private:
        void set_word_definedness( uint8_t *mask, uint8_t shadow_content, int pos )
        {
            if ( pos % 8 < 4 )
                shadow_content >>= 4;
            for ( int i = 0; i < 4; ++i )
            {
                bool shbit = shadow_content & 0x01;
                mask[ 3 - i ] = shbit ? 0xff : 0x00;
                shadow_content >>= 1;
            }
        }

        ExcMap _map;
        mutable std::mutex _mtx;

    };

    std::shared_ptr< Exceptions > _exceptions;

    struct TypeProxy
    {
        uint8_t *_base; int _pos;
        int shift() const { return 2 * ( ( _pos % 16 ) / 4 ); }
        uint8_t mask() const { return uint8_t( 0b11 ) << shift(); }
        uint8_t &word() const { return *( _base + _pos / 16 ); }
        TypeProxy &operator=( const TypeProxy &o ) { return *this = ShadowType( o ); }
        TypeProxy &operator=( ShadowType st )
        {
            set(st);
            return *this;
        }
        ShadowType get() const
        {
            return ShadowType( ( word() & mask() ) >> shift() );
        }
        void set( ShadowType st )
        {
            word() &= ~mask();
            word() |= uint8_t( st ) << shift();
        }
        bool is_exception() const
        {
            return get() == ShadowType::PointerException || get() == ShadowType::DataException;
        }
        void exceptionise_data()
        {
            auto tp = get();
            if ( tp == ShadowType::Data )
                set( ShadowType::DataException );
            else if ( tp == ShadowType::Pointer )
                set( ShadowType::PointerException );
        }
        void unexceptionise_data()
        {
            auto tp = get();
            if ( tp == ShadowType::DataException )
                set( ShadowType::Data );
        }
        operator ShadowType() const { return get(); }
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
            Exceptions &exceptions() const { return _parent->_exceptions; }
            uint8_t mask() const { return uint8_t( 0x80 ) >> ( _pos % 8 ); }
            uint8_t &word() const { return *( _base + ( _pos / 8 ) ); };
            proxy *operator->() { return this; }
            proxy &operator=( const proxy &o ) { return *this = uint8_t( o ); }
            proxy &operator=( uint8_t b )
            {
                auto tp = TypeProxy(_type_base, _pos);
                bool is_exc = tp.is_exception();
                if ( b == 0xff && ! ( word() & mask() ) )
                {
                    word() |= mask();
                    if ( is_exc )
                    {
                        if ( ! exceptions().definedness_changed( _parent->_base, _pos, b, word(), is_exc ) )
                        {
                            tp.unexceptionise_data();
                            // TODO: unexceptionise pointer exceptions
                        }
                    }
                }
                else if ( b != 0xff )
                {
                    word() &= ~mask();
                    if ( b != 0x00 || is_exc )
                    {
                        if ( exceptions().definedness_changed(_parent->_base, _pos, b, word(), is_exc) )
                        {
                            tp.exceptionise_data();
                        }
                        else
                        {
                            tp.unexceptionise_data();
                        }
                    }
                }
                return *this;
            }
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
        Exceptions & _exceptions;
        int _from;
        int _to;

        DefinedC(Pool &def, Pool &type, Exceptions &exc, Internal base, int from, int to)
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
                ASSERT( *(i + 4) == ShadowType::Pointer || *(i + 4) == ShadowType::PointerException );
            }
            proxy *operator->() { return this; }
            int offset() const { return _i._pos - _parent->types.begin()._pos; }
            int size() const
            {
                if ( *_i == ShadowType::Pointer )
                    return 4;
                if ( *(_i + 4) == ShadowType::Pointer )
                    return 8;
                NOT_IMPLEMENTED();
            }
            bool operator==( const proxy &o ) const
            {
                return offset() == o.offset() && size() == o.size();
            }
        };

        struct iterator : std::iterator< std::forward_iterator_tag, proxy >
        {
            PointerC *_parent;
            t_iterator _self;
            void seek()
            {
                while ( _self + 4 < _parent->types.end() &&
                        *(_self + 4) != ShadowType::Pointer &&
                        *(_self + 4) != ShadowType::PointerException ) _self = _self + 4;
                if ( ! ( _self + 4 < _parent->types.end() ) )
                    _self = _parent->types.end();
            }
            iterator &operator++() { _self = _self + 4; seek(); return *this; }
            proxy operator*() const { return proxy( _parent, _self ); }
            proxy operator->() const { return proxy( _parent, _self ); }
            bool operator!=( iterator o ) const { return _parent != o._parent || _self != o._self; }
            bool operator==( iterator o ) const { return _parent == o._parent && _self == o._self; }
            iterator( PointerC *p, t_iterator s )
                : _parent( p ), _self( s )
            {}
        };

        TypeC types;
        iterator begin() { auto b = iterator( this, types.begin() ); b.seek(); return b; }
        iterator end() { return iterator( this, types.end() ); }
        proxy atoffset( int i ) { return *iterator( types.begin() + i ); }
        PointerC( Pool &p, Internal i, int f, int t )
            : types( p, i, f, t )
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
        _exceptions->free( p );
    }

    auto type( Loc l, int sz )
    {
        return TypeC( _type, l.object, l.offset, l.offset + sz );
    }
    auto defined( Loc l, int sz )
    {
        return DefinedC( _defined, _type, *_exceptions, l.object, l.offset, l.offset + sz );
    }
    auto pointers( Loc l, int sz )
    {
        return PointerC( _type, l.object, l.offset, l.offset + sz );
    }
    bool &shared( Internal p )
    {
        return *_shared.template machinePointer< bool >( p );
    }

    template< typename V >
    void write( Loc l, V value )
    {
        const int size = sizeof( typename V::Raw );
        auto t = type( l, size );
        if ( value.pointer() && l.offset % 4 == 0 )
        {
            t[ 0 ] = ShadowType::Data;
            t[ 4 ] = ShadowType::Pointer;
        }
        else
        {
            for ( int i = 0; i < size; i += 4 )
                if ( !t[ i ].is_exception() )
                    t[ i ] = ShadowType::Data;
        }

        union {
            typename V::Raw _def;
            uint8_t _def_bytes[ size ];
        };

        _def = value.defbits();
        std::copy( _def_bytes, _def_bytes + size, defined( l, size ).begin() );
    }

    template< typename V >
    void read( Loc l, V &value )
    {
        const int size = sizeof( typename V::Raw );

        union {
            typename V::Raw _def;
            uint8_t _def_bytes[ size ];
        };

        auto def = defined( l, size );
        std::copy( def.begin(), def.end(), _def_bytes );
        value.defbits( _def );

        if ( l.offset % 4 == 0 )
        {
            auto t = type( l, size );
            value.pointer( size == PointerBytes &&
                           t[ 0 ] == ShadowType::Data &&
                           t[ 4 ] == ShadowType::Pointer );
        }
    }

    template< typename FromSh >
    void copy( FromSh &from_sh, typename FromSh::Loc from, Loc to, int sz )
    {
        auto from_type = from_sh.type( from, sz );
        auto to_type = type( to, sz );
        auto from_type_it = from_type.begin();
        auto to_type_it = to_type.begin();
        while ( from_type_it < from_type.end() )
        {
            if ( ! to_type_it->is_exception() && ! from_type_it->is_exception() )
                *to_type_it = *from_type_it;
            to_type_it += 4;
            from_type_it += 4;
        }

        auto from_def = from_sh.defined( from, sz );
        auto to_def = defined( to, sz );
        std::copy( from_def.begin(), from_def.end(), to_def.begin() );
    }

    void copy( Loc from, Loc to, int sz ) { return copy( *this, from, to, sz ); }

    void dump( std::string what, Loc l, int sz )
    {
        std::cerr << what << ", obj = " << l.object << ", off = " << l.offset << ": ";
        for ( auto t : type( l, sz ) )
            std::cerr << t;
        std::cerr << " ... ";
        for ( auto d : defined( l, sz ) )
            std::cerr << +d << " ";
        std::cerr << std::endl;
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
    Loc shloc( Ptr p, int off ) { return Loc( p, off ); }

    Ptr make( int sz )
    {
        auto r = pool.allocate( sizeof( Ptr ) );
        shadows.make( r, sz );
        return r;
    }

    template< typename T >
    void write( Ptr p, int off, T t ) { shadows.write( shloc( p, off ), t ); }

    template< typename T >
    void read( Ptr p, int off, T &t ) { shadows.read( shloc( p, off ), t ); }

    void copy( Ptr pf, int of, Ptr pt, int ot, int sz )
    {
        shadows.copy( shloc( pf, of ), shloc( pt, ot ), sz );
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

    TEST( read_ptr_unaligned )
    {
        PointerV p1( vm::nullPointer() ), p2;
        heap.write( obj, 0, p1 );
        heap.write( obj, 4, p1 );
        heap.read< PointerV >( obj, 1, p2 );
        ASSERT( p2.defined() );
        ASSERT( ! p2.pointer() );
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
        vm::value::Int< 16 > i1( 32, 0x0AFF, false ), i2, i3;
        ASSERT( i1.defbits() == 0x0AFF );
        heap.write( obj, 0, i1 );
        heap.read( obj, 0, i2 );
        ASSERT( i2.defbits() == 0x0AFF );

        heap.write( obj, 3, i1 );
        heap.read( obj, 3, i3 );
        ASSERT( i3.defbits() == 0x0AFF );

        heap.read( obj, 0, i2 );
        ASSERT( i2.defbits() == 0x0AFF );
    }

    TEST( read_truly_partially_initialized )
    {
        vm::value::Int< 16 > i1( 32, 0x0AFE, false ), i2, i3;
        ASSERT( i1.defbits() == 0x0AFE );
        heap.write( obj, 0, i1 );
        heap.read( obj, 0, i2 );
        ASSERT( i2.defbits() == 0x0AFE );

        heap.write( obj, 3, i1 );
        heap.read( obj, 3, i3 );
        ASSERT( i3.defbits() == 0x0AFE );

        heap.read( obj, 0, i2 );
        ASSERT( i2.defbits() == 0x0AFE );
    }

    TEST( copy_partially_initialized )
    {
        vm::value::Int< 16 > i1( 32, 0x0AFF, false ), i2;
        heap.write( obj, 0, i1 );
        heap.copy( obj, 0, obj, 3, 2 );
        heap.read( obj, 3, i2 );
        ASSERT( i2.defbits() == 0x0AFF );
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
