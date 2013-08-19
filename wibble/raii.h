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

    AutoDelete( Instance &&instance, Delete deleter ) :
        AutoDelete( true, std::forward< Instance >( instance ), deleter )
    { }

    AutoDelete( bool runDeleter, Instance &&instance, Delete deleter ) :
        value( std::forward< Instance >( instance ) ),
        deleter( deleter ), runDeleter( runDeleter )
    { }

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


}
}

#endif // WIBBLE_RAII_H

#endif
