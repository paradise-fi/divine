// -*- C++ -*- (c) 2015 Jiří Weiser
//             (c) 2014 Vladimír Štill
//  StrongEnumFlags is ported from bricks/brick-types.h
#include "fs-utils.h"

#ifndef _FS_STORAGE_H_
#define _FS_STORAGE_H_

namespace divine {
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

} // namespace storage
} // namespace fs
} // namespace divine

#endif
