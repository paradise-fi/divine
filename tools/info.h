// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>
#include <divine/utility/meta.h>
#include <type_traits>
#include <vector>

#ifndef DIVINE_ALGORITHM_INFO
#define DIVINE_ALGORITHM_INFO

namespace divine {

struct Prop {
    Prop() = default;
    Prop( std::string name, std::string desc ) : name( name ), desc( desc ) { }
    std::string name;
    std::string desc;
};

struct InfoBase {
    virtual void propertyInfo( graph::PropertySet prop, Meta &m ) = 0;
    virtual generator::ReductionSet filterReductions( generator::ReductionSet ) = 0;
    virtual std::vector< Prop > getProperties() = 0;
    virtual ~InfoBase() {};
};

template< typename Setup >
struct Info : virtual algorithm::Algorithm, algorithm::AlgorithmUtils< Setup, brick::types::Unit >,
              virtual InfoBase, Sequential
{
    typename Setup::Graph *g;

    void run() {
        std::cout << "Available properties:" << std::endl;
        for ( auto p : getProperties() )
            std::cout << " * " << p.name << ": " << p.desc << std::endl;
        std::cout << std::endl;

        auto cap = compactCapabilities();
        if ( std::get< 0 >( cap ) )
            std::cout << std::get< 1 >( cap ) << std::endl << std::endl;

        std::cout << "State flags: ";
        bool fst = true;
        g->enumerateFlags( [&]( std::string name, int, graph::flags::Type t ) {
                std::cout << (fst ? "" : ", ") << graph::flags::flagName( name, t );
                fst = false;
            } );
        std::cout << std::endl;
    }

    int id() { return 0; }

    virtual std::vector< Prop > getProperties() override {
        std::vector< Prop > props;
        g->properties( [&] ( std::string name, std::string descr, graph::PropertyType ) {
                props.emplace_back( name, descr );
            } );
        return props;
    }

    virtual void propertyInfo( graph::PropertySet s, Meta &m ) override {
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

    virtual generator::ReductionSet filterReductions( generator::ReductionSet rs ) override {
        return g->useReductions( rs );
    }

    template< typename Gen >
    auto _capa( brick::types::Preferred ) ->
        decltype( typename Gen::IsExplicit(), std::tuple< bool, std::string >() )
    {
        return std::make_tuple( true, g->base().explicitInfo() );
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
