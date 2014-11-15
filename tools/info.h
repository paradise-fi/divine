// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>
#include <divine/utility/meta.h>
#include <type_traits>

#ifndef DIVINE_ALGORITHM_INFO
#define DIVINE_ALGORITHM_INFO

namespace divine {

struct InfoBase {
    virtual void propertyInfo( graph::PropertySet prop, Meta &m ) = 0;
    virtual generator::ReductionSet filterReductions( generator::ReductionSet ) = 0;
    virtual ~InfoBase() {};
};

template< typename Setup >
struct Info : virtual algorithm::Algorithm, algorithm::AlgorithmUtils< Setup, brick::types::Unit >,
              virtual InfoBase, Sequential
{
    typename Setup::Graph *g;

    void run() {
        std::cout << "Available properties:" << std::endl;
        g->properties( [&] ( std::string name, std::string descr, graph::PropertyType ) {
                std::cout << " * " << name << ": " << descr << std::endl;
            } );
        auto cap = compactCapabilities();
        if ( std::get< 0 >( cap ) )
            std::cout << "Saved features: " << std::get< 1 >( cap ) << std::endl;
    }

    int id() { return 0; }

    virtual void propertyInfo( graph::PropertySet s, Meta &m ) {
        int count = 0;
        g->properties( [&] ( std::string name, std::string des, graph::PropertyType t ) {
                if ( s.count( name ) ) {
                    m.input.propertyDetails += (count ? " && " : "") + des;
                    ++ count;
                    if ( m.input.propertyType != graph::PT_None &&
                         m.input.propertyType != t )
                        throw std::logic_error(
                            "Combining different property types is currently not supported." );
                    m.input.propertyType = t;
                }
            } );
    }

    virtual generator::ReductionSet filterReductions( generator::ReductionSet rs ) {
        return g->useReductions( rs );
    }

    template< typename T >
    T *ptr_cast( T *ptr ) { return reinterpret_cast< T * >( ptr ); }

    template< typename Gen >
    auto _capa( brick::types::Preferred ) ->
        decltype( typename Gen::IsExplicit(), std::tuple< bool, std::string >() )
    {
        return std::make_tuple( true, to_string( g->base().capabilities() ) );
    }

    template< typename Gen >
    auto _capa( brick::types::NotPreferred ) -> std::tuple< bool, std::string >
    {
        return std::make_tuple( false, std::string() );
    }

    std::tuple< bool, std::string > compactCapabilities() {
        return _capa< typename std::remove_reference<
            decltype( this->graph().base() ) >::type >( brick::types::Preferred() );
    }

    Info( Meta m ) : Algorithm( m ) {
        g = this->initGraph( *this, decltype( this )( nullptr ), false );
    }
};

}

#endif // DIVINE_ALGORITHM_INFO
