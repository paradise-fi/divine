// -*- C++ -*- (c) 2016 Vladimír Štill <xstill@fi.muni.cz>
#pragma once

#include <stdexcept>
#include <utility>
#include <brick-types>
DIVINE_RELAX_WARNINGS
#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/StringRef.h>
DIVINE_UNRELAX_WARNINGS

namespace lart {

struct UnexpectedValue : std::runtime_error {
    using std::runtime_error::runtime_error;
};

template< typename... >
using VoidT = void;

namespace detail {
    template< typename V >
    static auto dump( llvm::raw_string_ostream &str, V &v, brick::types::Preferred )
        -> decltype( v.print( str ) )
    { v.print( str ); str << "\n"; }

    template< typename V >
    static auto dump( llvm::raw_string_ostream &str, V *v, brick::types::Preferred )
        -> decltype( v->print( str ) )
    {
        if ( v != nullptr ) {
            v->print( str );
            str << "\n";
        } else {
            str << "nullptr\n";
        }
    }

    template< typename T >
    static void dump( llvm::raw_string_ostream &str, const T &v, brick::types::NotPreferred )
    { str << "  " << v << "\n"; }

    template< typename Range,
              typename = std::enable_if_t< !std::is_convertible< Range, llvm::StringRef >::value > >
    static auto dump( llvm::raw_string_ostream &str, Range &vs, brick::types::NotPreferred )
        -> VoidT< decltype( vs.begin() ), decltype( vs.end() ) >
    {
        for ( auto v : vs )
            dump( str, v, brick::types::Preferred() );
    }

    template< typename... Vals >
    static std::string mkmsg( std::string msg, Vals &&... toDump ) {
        std::string buffer;
        llvm::raw_string_ostream str( buffer );
        str << msg;
        str << "\nrelevant information:\n";
        ( dump( str, std::forward< Vals >( toDump ), brick::types::Preferred() ), ... );
        return str.str();
    }
}

struct UnexpectedLlvmIr : UnexpectedValue {
    using UnexpectedValue::UnexpectedValue;

    template< typename... Vals >
    UnexpectedLlvmIr( std::string msg, Vals &&... toDump ) :
        UnexpectedValue( detail::mkmsg( std::move( msg ), std::forward< Vals >( toDump )... ) )
    { }
};

#define ENSURE_LLVM( X, ... ) \
    if ( !( X ) ) { \
        throw ::lart::UnexpectedLlvmIr( \
                std::string( "Failed ENSURE: " #X " at " __FILE__ ":" ) + std::to_string( __LINE__ ), \
                ##__VA_ARGS__ ); \
    } else ((void)0)

#define WARN_LLVM( X, ... ) \
    if ( !( X ) ) { \
        std::cerr << ::lart::detail::mkmsg( std::string( "LART WARNING: " #X " at " __FILE__ ":" ) \
                      + std::to_string( __LINE__ ),  ##__VA_ARGS__ ); \
    } else ((void)0)

} // namespace llvm
