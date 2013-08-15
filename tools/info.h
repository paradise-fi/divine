// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>
#include <divine/utility/meta.h>

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

    Info( Meta m ) : Algorithm( m ) {
        g.read( m.input.model, m.input.definitions );
    }
};

}

#endif // DIVINE_ALGORITHM_INFO
