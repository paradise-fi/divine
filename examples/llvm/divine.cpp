// -*- C++ -*- (c) 2014 Vladimír Štill <xstill@fi.muni.cz>

/* divine-cflags: -std=c++11 -DPOSIX -DNVALGRIND -DBRICKS_CACHELINE=8 */
/* noinfo */

/* The model of DIVINE, to be verified by DIVINE
   You will have to compile it with some of DIVINE's libraries, most easily
   from root source dir like this:
   divine compile -l examples/llvm/divine.cpp divine/utility/meta.cpp \
          wibble/test.cpp wibble/string.cpp divine/toolkit/mpi.cpp \
          wibble/exception.cpp --cflags='-I.. -I../bricks'
 */

#include <divine/utility/meta.h>
#include <divine/algorithm/reachability.h>
#include <divine/generator/dummy.h>
#include <divine/graph/por.h>
#include <divine/graph/store.h>
#include <divine/utility/statistics.h>

// from stackoverflow (http://stackoverflow.com/a/760353/1620753):
#include <streambuf>
#include <ostream>

template <class cT, class traits = std::char_traits<cT> >
class basic_nullbuf: public std::basic_streambuf<cT, traits> {
    typename traits::int_type overflow(typename traits::int_type c)
    {
        return traits::not_eof(c); // indicate success
    }
};

template <class cT, class traits = std::char_traits<cT> >
class basic_onullstream: public std::basic_ostream<cT, traits> {
  public:
    basic_onullstream():
        std::basic_ios<cT, traits>(&m_sbuf),
        std::basic_ostream<cT, traits>(&m_sbuf)
    {
        this->init(&m_sbuf);
    }

  private:
    basic_nullbuf<cT, traits> m_sbuf;
};

typedef basic_onullstream<char> onullstream;
typedef basic_onullstream<wchar_t> wonullstream;

#ifdef __divine__
// override new to non-failing version
#include <divine.h>
#include <new>

void* operator new  ( std::size_t count ) { return __divine_malloc( count ); }
#endif

namespace divine {

// override output
struct DummyOutput : Output {
    onullstream str;
    virtual std::ostream &statistics() { return str; }
    virtual std::ostream &progress() { return str; }
    virtual std::ostream &debug() { return str; }
};
std::unique_ptr< Output > Output::_output = std::unique_ptr< Output >( new DummyOutput() );

// this is missing but we don't want to compile statistics.cpp
NoStatistics NoStatistics::_global;

template< typename Graph, typename Store >
using Transition = std::tuple< typename Store::Vertex, typename Graph::Node, typename Graph::Label >;

struct Setup {
  private:
    using _Hasher = ::divine::algorithm::Hasher;
    using _Generator = ::divine::generator::Dummy;
    template< typename Graph, typename Store, typename Stat >
    using _Transform = ::divine::graph::NonPORGraph< Graph, Store >;
    using _Visitor = ::divine::visitor::Shared;
    using _TableProvider = ::divine::visitor::SharedProvider;
    template < typename Provider, typename Generator, typename Hasher, typename Stat >
    using _Store = ::divine::visitor::DefaultStore< Provider, Generator, Hasher, Stat >;
    template< typename Transition >
    struct _Topology {
        template< typename I >
        using T = typename ::divine::Topology< Transition >::template Local< I >;
    };
    using _Statistics = ::divine::NoStatistics;
  public:
    using Store = _Store< _TableProvider, _Generator, _Hasher, _Statistics >;
    using Graph = _Transform< _Generator, Store, _Statistics >;
    using Visitor = _Visitor;
    template< typename I >
    using Topology = typename _Topology< Transition< Graph, Store > >
                       ::template T< I >;
    using Statistics = _Statistics;
};

}

int main() {
    using namespace divine;
    Meta m;
    m.execution.initialTable = 2;
    algorithm::Reachability< Setup > reach{ m };
    reach.run();
    return 0;
}
