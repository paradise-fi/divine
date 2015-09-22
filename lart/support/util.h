// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>
//
//
#include <type_traits>
#include <llvm/Support/Casting.h>
#include <brick-assert.h>
#include <brick-types.h>

#ifndef LART_SUPPORT_UTIL_H
#define LART_SUPPORT_UTIL_H

namespace lart {

namespace util {

template< typename >
struct ReturnType { };

template< typename R, typename... Args >
struct ReturnType< R (*)( Args... ) > { using T = R; };

template< unsigned i, typename... Empty >
struct _Get { };

template< typename _T, typename... Ts >
struct _Get< 0, _T, Ts... > { using T = _T; };

template< unsigned i, typename _T, typename... Ts >
struct _Get< i, _T, Ts... > { using T = typename _Get< i - 1, Ts... >::T; };

template< unsigned, typename >
struct MethodArg { };

template< unsigned i, typename Obj, typename R, typename... Args >
struct MethodArg< i, R (Obj::*)( Args... ) > {
    using T = typename _Get< i, Args... >::T;
};

template< unsigned i, typename Obj, typename R, typename... Args >
struct MethodArg< i, R (Obj::*)( Args... ) const > {
    using T = typename _Get< i, Args... >::T;
};

}

template< typename What >
bool llvmcase( What * ) { return false; }

template< typename What, typename Lambda, typename... Lambdas >
bool llvmcase( What *w, Lambda lambda, Lambdas &&...lambdas ) {
    if ( auto val = llvm::dyn_cast<
            typename std::remove_pointer<
                typename util::MethodArg< 0, decltype( &Lambda::operator() ) >::T
            >::type >( w ) )
    {
        lambda( val );
        return true;
    }
    return llvmcase( w, std::forward< Lambdas >( lambdas )... );
}

template< typename What, typename... Lambdas >
bool llvmcase( What &w, Lambdas &&...lambdas ) {
    return llvmcase( &w, std::forward< Lambdas >( lambdas )... );
}

}

#endif // LART_SUPPORT_UTIL_H
