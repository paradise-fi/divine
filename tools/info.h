// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>
#include <divine/utility/meta.h>
#include <type_traits>

#ifndef DIVINE_ALGORITHM_INFO
#define DIVINE_ALGORITHM_INFO

namespace divine {

struct InfoBase {
    virtual void propertyInfo( std::string prop, Meta &m ) = 0;
    virtual generator::ReductionSet filterReductions( generator::ReductionSet ) = 0;
    virtual ~InfoBase() {};
};

template< typename Setup >
struct Info : virtual algorithm::Algorithm, algorithm::AlgorithmUtils< Setup >, virtual InfoBase, Sequential
{
    typename Setup::Graph g;

    void run() {
        typedef std::vector< std::pair< std::string, std::string > > Props;
        std::cout << "Available properties:" << std::endl;
        g.properties( [&] ( std::string name, std::string descr, graph::PropertyType ) {
                std::cout << " * " << name << ": " << descr << std::endl;
            } );
        auto cap = compactCapabilities();
        if ( std::get< 0 >( cap ) )
            std::cout << "Saved features: " << std::get< 1 >( cap ) << std::endl;
    }

    int id() { return 0; }

    virtual void propertyInfo( std::string s, Meta &m ) {
        g.properties( [&] ( std::string name, std::string des, graph::PropertyType t ) {
                if ( s == name ) {
                    m.input.property = des;
                    m.input.propertyType = t;
                }
            } );
    }

    virtual generator::ReductionSet filterReductions( generator::ReductionSet rs ) {
        return g.useReductions( rs );
    }

    template< typename T >
    T *ptr_cast( T *ptr ) { return reinterpret_cast< T * >( ptr ); }

    template< typename Gen >
    auto _capa( wibble::Preferred ) ->
        decltype( typename Gen::IsCompact(), std::tuple< bool, std::string >() )
    {
        return std::make_tuple( true, g.base().capabilities().string() );
    }

    template< typename Gen >
    auto _capa( wibble::NotPreferred ) -> std::tuple< bool, std::string >
    {
        return std::make_tuple( false, std::string() );
    }

    std::tuple< bool, std::string > compactCapabilities() {
        return _capa< typename std::remove_reference<
            decltype( this->graph().base() ) >::type >( wibble::Preferred() );
    }

    Info( Meta m ) : Algorithm( m ) {
        g.read( m.input.model, m.input.definitions );
    }
};

}

#endif // DIVINE_ALGORITHM_INFO
