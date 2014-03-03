// -*- C++ -*- (c) 2013 Vladimír Štill

#if __cplusplus >= 201103L

#include <utility>
#include <type_traits>

#ifndef WIBBLE_RAII_H
#define WIBBLE_RAII_H

namespace wibble {
namespace raii {

template< typename Instance,
    typename Delete = void(*)( typename std::remove_reference< Instance >::type & ) >
struct AutoDelete {
    Instance value;
    Delete deleter;

    AutoDelete() : runDeleter( false ) { }
    AutoDelete( Instance &&instance, Delete deleter ) :
        AutoDelete( true, std::forward< Instance >( instance ), deleter )
    { }
    AutoDelete( bool runDeleter, Instance &&instance, Delete deleter ) :
        value( std::forward< Instance >( instance ) ),
        deleter( deleter ), runDeleter( runDeleter )
    { }

    // allow only move
    AutoDelete( const AutoDelete & ) = delete;
    AutoDelete( AutoDelete &&o ) = default;
    AutoDelete &operator=( AutoDelete other ) {
        std::swap( other.value, value );
        std::swap( other.deleter, deleter );
        std::swap( other.runDeleter, runDeleter );
    }

    ~AutoDelete() {
        if ( runDeleter )
            deleter( value );
    }

  private:
    bool runDeleter;
};

template< typename Creator, typename Delete >
auto autoDeleter( Creator creator, Delete deleter )
    -> AutoDelete< typename std::result_of< Creator() >::type, Delete >
{
    using Instance = typename std::result_of< Creator() >::type;
    return AutoDelete< Instance, Delete >( true, creator(), deleter );
}

template< typename Instance, typename Delete >
auto refDeleteIf( bool cond, Instance &ref, Delete deleter )
    -> AutoDelete< Instance &, Delete >
{
    return AutoDelete< Instance &, Delete >( cond, ref, deleter );
}

template< typename Instance, typename Delete >
auto refDeleter( Instance &ref, Delete deleter )
    -> AutoDelete< Instance &, Delete >
{
    return refDeleteIf( true, ref, deleter );
}

template< typename Fn >
struct Defer {
    Defer( Fn fn ) : fn( fn ) { }
    void run() {
        if ( !_deleted ) {
            fn();
            _deleted = true;
        }
    }
    bool deleted() const { return _deleted; }
    ~Defer() { run(); }
  private:
    Fn fn;
    bool _deleted;
};

template< typename Fn >
Defer< Fn > defer( Fn fn ) { return Defer< Fn >( fn ); }

}
}

#endif // WIBBLE_RAII_H

#endif
