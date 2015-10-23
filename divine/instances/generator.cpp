// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>
//                 2013 Petr Ročkai <me@mornfall.net>

#include <brick-fs.h>

#include <array>
#include <cstdint>
#include <cmath>
#include <iterator>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

#include <divine/instances/definitions.h>

namespace divine {
namespace instantiate {

template< typename T >
using FixArray = std::vector< T >;

static const std::vector< std::string > defaultHeaders = {
        "divine/instances/definitions.h",
        "divine/graph/store.h",
    };

struct InstGenerator {
    bool good;

    std::stringstream _select;
    std::stringstream _extern;
    FixArray< std::stringstream > _instances;

    int _perFile;

    InstGenerator( int perFile, int files ) :
        _select(), _extern(), _instances( files ), _perFile( perFile )
    {
        _checkConsistency();
    }

    void error( std::initializer_list< std::string > msg ) {
        good = false;
        std::cerr << "Error: ";
        bool first = true;
        for ( auto m : msg ) {
            std::cerr << (first ? "" : "      ") << m << std::endl;
            first = false;
        }
    };

    void _checkConsistency() {
        good = true;

        for ( const auto &v : instantiation ) {
            if ( v.size() <= 0 )
                error( { "Some component does not have any choices." } );
            if ( options.find( v[0].type ) == options.end() )
                error( { "Component " + std::get< 0 >( showGen( v[0] ) ) + " does not define instantiation options" } );
            for ( auto component : v ) {
                if ( component.type != v[0].type )
                    error( { "Component " + std::get< 0 >( showGen( component ) ) + " is misplacet between other components",
                             "of type " + std::get< 0 >( showGen( v[0] ) ) + "." } );
                if ( select.find( component ) == select.end() )
                    error( { "Component " + show( component ) + " does not define select" } );
                if ( (component.type == Type::Algorithm || component.type == Type::Generator )
                        && headers.find( component ) == headers.end() )
                    error( { show( component ) + " does not define header" } );
                if ( (component.type == Type::Algorithm || component.type == Type::Generator)
                        && postSelect.find( component ) == postSelect.end() )
                    error( { show( component ) + " does not define postSelect" } );
            }
        }
        ASSERT( good );
    }

    std::vector< std::string > _headers( Key k ) {
        auto it = headers.find( k );
        return it == headers.end() ? it->second : std::vector< std::string >();
    }

    std::string _label( Key k ) {
        std::string a, b;
        std::tie( a, b ) = showGen( k );
        return "_" + a + "_" + b + "_";
    }

    std::string _label( const FixArray< Key > &trace ) {
        std::string label = "";
        for ( auto k : trace )
            label += _label( k );
        return label;
    }

    std::string _label( const FixArray< Key > &trace, Key last ) {
        return _label( trace ) + _label( last );
    }

    std::string _show( Key k ) {
        return "instantiate::" + show( k );
    }

    std::string _show( const FixArray< Key > &trace ) {
        std::string label = "";
        for ( auto k : trace )
            label += (label.size() ? ", " : "") + _show( k );
        return "{ " + label + " }";
    }

    std::string _show( const FixArray< Key > &trace, Key last ) {
        return _show( appendArray( trace, last ) );
    }

    std::string _ctor( const FixArray< Key > &trace, Key last ) {
        std::string ctor = "{ ";
        for ( auto k : trace )
            ctor += _show( k ) + ", ";
        return ctor + _show( last ) + " }";
    }

    std::string _ctor( FixArray< Key > trace ) {
        if ( trace.size() == 0 )
            return "{ }";

        Key last = trace.back();
        trace.resize( trace.size() - 1 );
        return _ctor( trace, last );
    }

    std::string _algorithm( Algorithm alg ) {
        return algorithm[ alg ];
    }

    std::set< FixArray< Key > > graph;

    int buildGraph() {

        using ID = std::pair< int, FixArray< Key > >;
        std::deque< ID > stack;
        stack.emplace_back( 0, FixArray< Key >() );

        std::vector< FixArray< Key > > finals;

        while ( !stack.empty() ) {
            ID now = std::move( stack.front() );
            stack.pop_front();

            const auto &trace = now.second;
            if ( !graph.insert( trace ).second )
                continue;

            if ( now.first < int( Instantiation::length ) ) {
                const auto &succs = instantiation[ now.first ];
                for ( auto s : succs ) {
                    auto newtrace = appendArray( trace, s );
                    if ( _valid( newtrace ) )
                        stack.emplace_back( now.first + 1, std::move( newtrace ) );
                }
            } else
                finals.emplace_back( std::move( trace ) );

        }

        std::sort( finals.begin(), finals.end(), []( Trace a, Trace b ) {
                for ( auto xa : a )
                    if ( xa.type == Type::Generator )
                        for ( auto xb : b )
                            if ( xb.type == Type::Generator )
                                return xa < xb;
                return false;
            } );

        emitSelect( finals );
        emitExtern( finals );
        return emitInstances( finals );
    }

    std::string _gen( Key k ) {
        return std::get< 1 >( showGen( k ) );
    }

    std::string _gen( Trace trace ) {
        for ( auto t : trace )
            if ( t.type == Type::Generator )
                return _gen( t );
    }

    void emitSelect( std::vector< FixArray< Key > > finals )
    {
        _select << "#include <divine/instances/auto/extern.h>" << std::endl
                << "#include <divine/instances/definitions.h>" << std::endl
                << std::endl;

        _select << "namespace divine {" << std::endl;


        _select << "namespace instantiate {" << std::endl
                << std::endl;

        std::string current;

        for ( auto trace : finals ) {
            if ( !_allTraits(trace) )
                continue;

            if ( current != _gen( trace ) ) {
                if ( current != "" )
                    _select << "};" << std::endl;
                _select << "template<> const instantiate::CMap< Trace, AlgorithmPtr (*)( Meta &, ::divine::generator::"
                        << _gen( trace ) << " * ) > "
                        << "instantiate::JumpTable< ::divine::generator::" << _gen( trace ) << " * >::data = {" << std::endl;
                current = _gen( trace );
            }

            _select << std::endl << "{ Trace" << _show( trace ) << ", instantiate::create2"
                    << _label( trace ) << " }," << std::endl;
        }

        _select << std::endl << "};" << std::endl;

        _select << "template<> const instantiate::CMap< Trace, AlgorithmPtr (*)( Meta &, Unit ) >"
                << " instantiate::JumpTable< Unit >::data = {" << std::endl;

        for ( auto trace : finals )
            if ( _allTraits(trace) )
                _select << std::endl << "{ Trace" << _show( trace ) << ", instantiate::create1"
                        << _label( trace ) << " }," << std::endl;

        _select << std::endl << "};" << std::endl;

        _select << std::endl << "} }" << std::endl;
    }

    void emitExtern( std::vector< FixArray< Key > > finals ) {
        _extern << "#include <divine/utility/meta.h>" << std::endl
                << "#include <divine/utility/die.h>" << std::endl
                << "#include <divine/algorithm/common.h>" << std::endl
                << "#include <memory>" << std::endl;

        std::string current;
        std::set< std::string > hdrs;

        for ( auto trace : finals ) {
            if ( _gen( trace ) == current )
                continue;

            for ( auto sym : trace ) {
                if ( sym.type == Type::Generator ) {
                    auto it = headers.find( sym );
                    if ( it != headers.end() )
                        std::copy( it->second.begin(), it->second.end(), std::inserter( hdrs, hdrs.begin() ) );
                }
            }
        }

        for ( auto header : hdrs )
            _extern << "#include <" << header << ">" << std::endl;

        _extern << "namespace divine {" << std::endl
                << "namespace instantiate {" << std::endl;

        for ( auto symbol : finals )
            if ( _allTraits( symbol ) ) {
                _extern << "std::unique_ptr< ::divine::algorithm::Algorithm > create2"
                        << _label( symbol ) << "( Meta &, ::divine::generator::" << _gen( symbol ) << " * );" << std::endl;
                _extern << "std::unique_ptr< ::divine::algorithm::Algorithm > create1"
                        << _label( symbol ) << "( Meta &, Unit );" << std::endl;
            }

        _extern << "}" << std::endl << "}" << std::endl;
    }

    int emitInstances( std::vector< FixArray< Key > > finals ) {
        int size = 0;
        for ( auto symbol : finals )
            if ( _allTraits( symbol ) )
                ++size;

        int perFile;
        if ( _perFile * int( _instances.size() ) >= size )
            perFile = _perFile;
        else
            perFile = std::ceil( double( size ) / _instances.size() );

        int inFile = 0;
        int numFiles = 0;

        FixArray< std::vector< FixArray< Key > > > files( _instances.size() );
        for ( auto symbol : finals ) {
            if ( _allTraits( symbol ) )
                ++inFile;
            files[ numFiles ].push_back( symbol );
            if ( inFile == perFile ) {
                inFile = 0;
                ++numFiles;
            }
        }
        for ( int i = 0; i < int( _instances.size() ); ++i ) {
            auto &file = _instances[ i ];
            if ( files[ i ].size() > 0 ) {
            emitHeaders( file, files[ i ] );
                for ( auto sym : files[ i ] )
                    emitCreate( file, sym );
                emitInstancesEnd( file );
            }
        }
        return size;
    }

    void emitHeaders( std::ostream &file, std::vector< FixArray< Key > > instances ) {
        std::set< std::string > hdrs{ defaultHeaders.begin(), defaultHeaders.end() };
        for ( auto inst : instances ) {
            for ( auto sym : inst ) {
                auto it = headers.find( sym );
                if ( it != headers.end() )
                    std::copy( it->second.begin(), it->second.end(), std::inserter( hdrs, hdrs.begin() ) );
            }
        }
        for ( auto header : hdrs )
            file << "#include <" << header << ">" << std::endl;
        file << "namespace divine {" << std::endl
             << "namespace instantiate {" << std::endl;
    }

    void emitInstancesEnd( std::ostream &file ) {
        file << "}" << std::endl << "}" << std::endl;
    }

    bool emitCreate( std::ostream &file, FixArray< Key > symbol ) {
        Algorithm alg = Algorithm::Begin;
        for ( auto c : symbol )
            if ( c.type == Type::Algorithm )
                alg = Algorithm( c.key );
        if ( alg == Algorithm::Begin )
            ASSERT_UNREACHABLE( "Algorithm missing in selection." );

        if ( _allTraits( symbol ) ) {
            file << "struct Setup" << _label( symbol ) << " {" << std::endl
                 << "  private:" << std::endl
                 << "    using _Hasher = ::divine::algorithm::Hasher;" << std::endl;
            for ( auto c : symbol ) {
                if ( c.type == Type::Algorithm )
                    continue;
                for ( auto sym : symbols[ c ] )
                    file << sym << std::endl;
            }

            file << "  public:" << std::endl
                 << "    using TableProvider = ::divine::visitor::SharedProvider;" << std::endl
                 << "    using Store = _Store< TableProvider, _Generator, _Hasher >;" << std::endl
                 << "    using Generator = _Generator;" << std::endl
                 << "    using Graph = ::divine::graph::NonPORGraph< _Generator, Store >;" << std::endl
                 << "    using Visitor = ::divine::visitor::Shared;" << std::endl
                 << "    template< typename I >" << std::endl
                 << "    using Topology = typename ::divine::Topology< Transition< Graph, Store > >"
                 << std::endl
                 << "                       ::template Local< I >;" << std::endl
                 << "};" << std::endl;

            file << "std::unique_ptr< ::divine::algorithm::Algorithm > create1"
                 << _label( symbol ) << "( Meta &meta, Unit ) {" << std::endl
                 << "    return std::unique_ptr< ::divine::algorithm::Algorithm >( new "
                 <<          algorithm[ alg ] << "< Setup" << _label( symbol ) << ">( meta ) );" << std::endl
                 << "}" << std::endl << std::endl;

            file << "std::unique_ptr< ::divine::algorithm::Algorithm > create2"
                 << _label( symbol ) << "( Meta &meta, ::divine::generator::" << _gen( symbol ) << " *g ) {" << std::endl
                 << "    return std::unique_ptr< ::divine::algorithm::Algorithm >( new "
                 <<          algorithm[ alg ] << "< Setup" << _label( symbol ) << ">( meta, g ) );" << std::endl
                 << "}" << std::endl << std::endl;

            return true;
        } else
            return false;
    }

    bool _allTraits( FixArray< Key > symbol ) {
        return std::accumulate( symbol.begin(), symbol.end(), true,
                []( bool val, Key key ) -> bool {
                    auto tr = traits.find( key );
                    return val && (tr == traits.end() || tr->second( Traits::available() ));
                } );
    }

    int writeFile( std::string name, std::stringstream &data ) {
        std::string sdata = data.str();

        std::ifstream ifile{ name };
        if ( !ifile.good() || brick::fs::readFile( ifile ) != sdata ) {
            ifile.close();
            std::ofstream ofile{ name };
            ofile << sdata;
            ofile.close();
            return 1;
        }
        return 0;
    }

    int writeFiles( std::string selName, std::string extName, std::string instancesPrefix ) {
        int cnt = 0;

        cnt += writeFile( selName, _select );
        cnt += writeFile( extName, _extern );
        for ( int i = 0; i < int( _instances.size() ); ++i )
            cnt += writeFile( instancesPrefix + std::to_string( i + 1 ) + ".cpp",
                        _instances[ i ] );
        return cnt;
    }


};

}
}

int main( int argc, char** argv ) {
    ASSERT_EQ( argc, 3 );

    int files = std::stoi( argv[ 1 ] );
    int min = std::stoi( argv[ 2 ] );
    ASSERT_LEQ( 1, files );
    ASSERT_LEQ( 1, min );

    divine::instantiate::InstGenerator gen( min, files );
    int size = gen.buildGraph();
    int regen = gen.writeFiles( "select.cpp", "extern.h", "instances-" );

    std::cerr << "Generated " << size << " instances, " << regen << " new files." << std::endl;
    return 0;
}
