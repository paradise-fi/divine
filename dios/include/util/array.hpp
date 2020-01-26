// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>
//             (c) 2018 Vladimír Štill <xstill@fi.muni.cz>

#ifndef __DIOS_ARRAY_HPP__
#define __DIOS_ARRAY_HPP__

#include <sys/divm.h>
#include <iterator>
#include <cstring>
#include <brick-order>
#include <initializer_list>
#include <memory>
#include <algorithm>

namespace __dios {

struct construct_shared_t {};

template < typename T, int PT = _VM_PT_Heap >
struct Array : brq::derive_ord, brq::derive_eq
{
    using size_type = int;
    using value_type = T;
    using iterator = T *;
    using const_iterator = const T *;
    using reverse_iterator = std::reverse_iterator< iterator >;
    using const_reverse_iterator = std::reverse_iterator< const_iterator >;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = T const&;

    /* Allow construction of multiple array instances that point to the same
     * memory. Objects constructed in this manner must never be destroyed but
     * must be disowned instead. */

    static constexpr construct_shared_t construct_shared;

    static constexpr bool nothrow_dtor = std::is_nothrow_destructible_v< T >;
    static constexpr bool nothrow_copy = std::is_nothrow_copy_constructible_v< T >;
    static constexpr bool nothrow_move = std::is_nothrow_move_constructible_v< T >;
    static constexpr bool is_trivial   = std::is_trivial_v< T >;

    Array() noexcept = default;
    ~Array() noexcept( nothrow_dtor ) { clear(); }

    template< int PTI >
    Array( const Array< T, PTI > &other, construct_shared_t ) noexcept
        : _data( other._data )
    {}

    Array( void *ptr, construct_shared_t ) noexcept
        : _data( static_cast< _Item * >( ptr ) )
    {}

    Array( const Array& other ) noexcept ( nothrow_copy )
        : Array( other.size(), other.begin(), other.end() )
    { }

    Array( Array&& other ) noexcept { swap( other ); };

    Array( std::initializer_list< T > ilist ) noexcept ( nothrow_copy ) :
        Array( ilist.size(), ilist.begin(), ilist.end() )
    { }

    Array( size_type size, const T &val = T() ) noexcept( nothrow_copy )
    {
        _resize( size );
        uninit_fill( begin(), end(), val );
    }

    template< typename It >
    Array( size_type size, It b, It e ) noexcept ( nothrow_copy )
    {
        append( size, b, e );
    }

    Array& operator=( Array other ) noexcept ( nothrow_copy )
    {
        swap( other );
        return *this;
    };

    void *disown() { void *rv = begin(); _data = nullptr; return rv; }

    template< typename It >
    void assign( size_type size, It b, It e ) noexcept ( nothrow_copy )
    {
        _clear();
        _resize( size );
        uninit_copy( b, e, begin() );
    }

    void swap( Array& other ) noexcept
    {
        using std::swap;
        swap( _data, other._data );
    }

    struct _Item
    {
        T _items[ 0 ];
        T *get() noexcept { return _items; }
    };

    iterator begin() noexcept { return _data ?_data->get() : nullptr; }
    const_iterator begin() const noexcept { return _data ?_data->get() : nullptr; }
    const_iterator cbegin() const noexcept { return _data ?_data->get() : nullptr; }

    iterator end() noexcept { return _data ? _data->get() + size() : nullptr; }
    const_iterator end() const noexcept { return _data ? _data->get() + size() : nullptr; }
    const_iterator cend() const noexcept { return _data ? _data->get() + size() : nullptr; }

    reverse_iterator rbegin() noexcept { return reverse_iterator( end() ); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator( end() ); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator( end() ); }

    reverse_iterator rend() noexcept { return reverse_iterator( begin() ); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator( begin() ); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator( begin() ); }

    T& back() noexcept { return *( end() - 1 ); }
    const T& back() const noexcept { return *( end() - 1 ); }
    T& front() noexcept { return *begin(); }
    const T& front() const noexcept { return *begin(); }

    bool empty() const noexcept { return !_data; }
    size_type size() const noexcept { return empty() ? 0 : __vm_obj_size( _data ) / sizeof( T ); }

    void clear() noexcept ( nothrow_dtor )
    {
        if ( empty() )
            return;
        _clear();
        __vm_obj_free( _data );
        _data = nullptr;
    }

    void push_back( const T& t ) noexcept ( nothrow_copy )
    {
        _resize( size() + 1 );
        new ( &back() ) T( t );
    }

    template< typename It >
    void append( size_type count, It b, It e ) noexcept ( is_trivial )
    {
        size_type oldsz = size();
        _resize( oldsz + count );
        uninit_copy( b, e, begin() + oldsz );
    }

    template < typename... Args >
    T& emplace_back( Args&&... args )
    {
        _resize( size() + 1 );
        new ( &back() ) T( std::forward< Args >( args )... );
        return back();
    }

    void pop_back() noexcept ( nothrow_dtor )
    {
        back().~T();
        _resize( size() - 1 );
    }

    iterator erase( iterator it ) noexcept ( nothrow_dtor )
    {
        return erase( it, std::next( it ) );
    }

    iterator erase( iterator first, iterator last ) noexcept ( nothrow_dtor )
    {
        if ( empty() )
            return;
        int origSize = size();
        int shift = std::distance( first, last );
        for ( iterator it = first; it != last; it++ )
            it->~T();
        memmove( first, last, ( end() - last ) * sizeof( T ) );
        _resize( origSize - shift );
        return last;
    }

    iterator insert( iterator where, const T &val ) noexcept ( nothrow_copy && nothrow_move )
    {
        _resize( size() + 1 );
        for ( iterator i = end() - 1; i > where; --i )
            new ( i ) T( std::move( *( i - 1 ) ) );
        new ( where ) T( val );
        return where;
    }


    T& operator[]( size_type idx ) noexcept { return _data->get()[ idx ]; }
    const T& operator[]( size_type idx ) const noexcept { return _data->get()[ idx ]; }

    void uninit_fill( iterator b, iterator e, const T &val ) noexcept ( nothrow_copy )
    {
        if constexpr ( nothrow_copy )
            for ( auto i = b; i != e; ++i )
                new ( i ) T( val );
        else
            std::uninitialized_fill( b, e, val );
    }

    template< typename from_t >
    void uninit_copy( from_t b, from_t e, iterator to ) noexcept ( nothrow_copy )
    {
        if constexpr ( nothrow_copy )
            for ( auto i = b; i != e; ++i )
                new ( to++ ) T( *i );
        else
            std::uninitialized_copy( b, e, to );
    }

    void resize( size_type n, const T &val = T() ) noexcept( nothrow_copy )
    {
        size_type old = size();
        _resize( n );

        if ( n > old )
            uninit_fill( begin() + old, end(), val );
    }

    void _resize( size_type n ) noexcept
    {
        if ( n == 0 )
        {
            if ( _data )
                __vm_obj_free( _data );
            _data = nullptr;
        }
        else if ( empty() )
            _data = static_cast< _Item * >( __vm_obj_make( n * sizeof( T ), PT ) );
        else
            __vm_obj_resize( _data, n * sizeof( T ) );
    }

    __local_skipcfl void _clear() noexcept ( nothrow_dtor )
    {
        auto s = size();
        for ( auto i = begin(); i != end(); ++i )
            i->~T();
    }

    bool operator==( const Array &o ) const noexcept
    {
        return size() == o.size() && std::equal( begin(), end(), o.begin() );
    }

    bool operator<( const Array &o ) const noexcept
    {
        return std::lexicographical_compare( begin(), end(), o.begin(), o.end() );
    }

    _Item *_data = nullptr;
};

} // namespace __dios

# endif // __DIOS_ARRAY_HPP__
