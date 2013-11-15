// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <type_traits>
#include <tuple>

// runtime readable meta-information about each component
struct Atom {
    std::string name;
    std::string get;
    std::vector< std::string > headers;
    std::string component;
    std::string friendlyName;

    Atom() = default;

    template< typename S >
    Atom( S ) : get( S::symbol ) {
        init( S(), wibble::Preferred() );
    }

    template< typename R, typename... X >
    static R declcheck( X... ) { assert_die(); }

    template< typename S >
    auto init( S, wibble::Preferred ) -> decltype( declcheck< void >( S::header ) ) {
        std::string head( S::header );
        for ( size_t from = 0, to;
                to = head.find( ':', from ), from != std::string::npos;
                from = to == std::string::npos ? to : to + 1 )
            headers.push_back( head.substr( from, to ) );
        initName( S() );
    }

    template< typename S >
    auto init( S, wibble::NotPreferred ) -> void {
        initName( S() );
    }

    template< typename S >
    void initName( S ) {
        auto n = symbolName( S() );
        name = std::get< 0 >( n ) + "::" + std::get< 1 >( n );
        component = std::get< 0 >( n );
        friendlyName = std::get< 1 >( n );
        if ( friendlyName.size() >= 2 && !std::isupper( friendlyName[ 1 ] ) )
            friendlyName[ 0 ] = std::tolower( friendlyName[ 0 ] );
    }

#define SYMBOL_NAME( TRAIT, NS ) template< typename S > \
    auto symbolName( S ) -> \
        decltype( declcheck< std::tuple< std::string, std::string > >( typename S::TRAIT() ) ) \
    { \
        return std::tuple< std::string, std::string >( std::string( NS ), std::string( S::key ) ); \
    }

    SYMBOL_NAME( IsAlgorithm,  "algorithm" );
    SYMBOL_NAME( IsGenerator,  "generator" );
    SYMBOL_NAME( IsTransform,  "transform" );
    SYMBOL_NAME( IsVisitor,    "visitor" );
    SYMBOL_NAME( IsStore,      "store" );
    SYMBOL_NAME( IsTopology,   "topology" );
    SYMBOL_NAME( IsStatistics, "statistics" );

    friend bool operator<( const Atom &a, const Atom &b ) {
        return std::make_tuple( a.name, a.component ) < std::make_tuple( b.name, b.component );
    }
};

/*****************************************************************************/
