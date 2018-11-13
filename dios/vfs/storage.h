// -*- C++ -*- (c) 2015 Jiří Weiser
//             (c) 2014 Vladimír Štill
//  StrongEnumFlags is ported from bricks/brick-types.h
#include <dios/vfs/utils.h>

#ifndef _FS_STORAGE_H_
#define _FS_STORAGE_H_

namespace __dios {
namespace fs {
namespace storage {

template< typename Self >
struct StrongEnumFlags {
    static_assert( utils::is_enum_class< Self >::value, "Not an enum class." );
    using This = StrongEnumFlags< Self >;
    using UnderlyingType = typename std::underlying_type< Self >::type;

    constexpr StrongEnumFlags() noexcept : store( 0 ) { }
    constexpr StrongEnumFlags( Self flag ) noexcept :
        store( static_cast< UnderlyingType >( flag ) )
    { }
    explicit constexpr StrongEnumFlags( UnderlyingType st ) noexcept : store( st ) { }

    constexpr explicit operator UnderlyingType() const noexcept {
        return store;
    }

    This &operator|=( This o ) noexcept {
        store |= o.store;
        return *this;
    }

    This &operator&=( This o ) noexcept {
        store &= o.store;
        return *this;
    }

    This &operator^=( This o ) noexcept {
        store ^= o.store;
        return *this;
    }

    friend constexpr This operator|( This a, This b ) noexcept {
        return This( a.store | b.store );
    }

    friend constexpr This operator&( This a, This b ) noexcept {
        return This( a.store & b.store );
    }

    friend constexpr This operator^( This a, This b ) noexcept {
        return This( a.store ^ b.store );
    }

    friend constexpr bool operator==( This a, This b ) noexcept {
        return a.store == b.store;
    }

    friend constexpr bool operator!=( This a, This b ) noexcept {
        return a.store != b.store;
    }

    constexpr bool has( Self x ) const noexcept {
        return ((*this) & x) == x;
    }

    This clear( Self x ) noexcept {
        store &= ~UnderlyingType( x );
        return *this;
    }

    explicit constexpr operator bool() const noexcept {
        return store;
    }

  private:
    UnderlyingType store;
};

// don't catch integral types and classical enum!
template< typename Self, typename = typename
          std::enable_if< utils::is_enum_class< Self >::value >::type >
constexpr StrongEnumFlags< Self > operator|( Self a, Self b ) noexcept {
    using Ret = StrongEnumFlags< Self >;
    return Ret( a ) | Ret( b );
}

template< typename Self, typename = typename
          std::enable_if< utils::is_enum_class< Self >::value >::type >
constexpr StrongEnumFlags< Self > operator&( Self a, Self b ) noexcept {
    using Ret = StrongEnumFlags< Self >;
    return Ret( a ) & Ret( b );
}

struct Stream {

    Stream( size_t capacity ) :
        _data( capacity ),
        _head( 0 ),
        _occupied( 0 )
    {}

    size_t size() const {
        return _occupied;
    }
    bool empty() const {
        return _occupied == 0;
    }
    size_t capacity() const {
        return _data.size();
    }

    size_t push( const char *data, size_t length ) {
        if ( _occupied + length > capacity() )
            length = capacity() - _occupied;

        if ( !length )
            return 0;

        size_t usedLength = std::min( length, capacity() - ( _head + _occupied ) % capacity() );
        std::copy( data, data + usedLength, end() );
        _occupied += usedLength;

        if ( usedLength < length ) {
            std::copy( data + usedLength, data + length, end() );
            _occupied += length - usedLength;
        }
        return length;
    }

    size_t pop( char *data, size_t length ) {
        if ( _occupied < length )
            length = _occupied;

        if ( !length )
            return 0;

        size_t usedLength = std::min( length, capacity() - _head );
        std::copy( begin(), begin() + usedLength, data );
        _head = ( _head + usedLength ) % capacity();

        if ( usedLength < length ) {
            std::copy( begin(), begin() + length - usedLength, data + usedLength );
            _head = ( _head + length - usedLength ) % capacity();
        }
        _occupied -= length;
        return length;
    }

    size_t peek( char *data, size_t length ) {
        size_t head = _head;
        size_t occupied = _occupied;
        size_t r = pop( data, length );
        _head = head;
        _occupied = occupied;
        return r;
    }

    bool resize( size_t newCapacity ) {
        if ( newCapacity < size() )
            return false;

        Array< char > newData( newCapacity );

        _occupied = pop( &newData.front(), _occupied );
        _head = 0;
        _data.swap( newData );
        return true;
    }

private:
    using Iterator = Array< char >::iterator;

    Iterator begin() {
        return _data.begin() + _head;
    }
    Iterator end() {
        return _data.begin() + ( _head + _occupied ) % capacity();
    }

    Array< char > _data;
    size_t _head;
    size_t _occupied;
};

} // namespace storage
} // namespace fs
} // namespace __dios

#endif
