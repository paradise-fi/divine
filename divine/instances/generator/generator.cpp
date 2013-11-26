// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <divine/instances/definitions.h>
#include <wibble/fixarray.h>
#include <wibble/sys/fs.h>

#include <array>
#include <cstdint>
#include <cmath>
#include <iterator>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace divine {
namespace instantiate {

using wibble::FixArray;

static const std::vector< std::string > defaultHeaders = {
        "divine/instances/auto/extern.h",
        "divine/instances/definitions.h"
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
                if ( (component.type == Type::Algorithm || component.type == Type::Generator
                            || component.type == Type::Transform )
                        && headers.find( component ) == headers.end() )
                    error( { show( component ) + " does not define header" } );
                if ( (component.type == Type::Algorithm || component.type == Type::Generator)
                        && postSelect.find( component ) == postSelect.end() )
                    error( { show( component ) + " does not define postSelect" } );
            }
        }
        assert( good );
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

        std::vector< ID > branches;
        std::vector< FixArray< Key > > finals;

        while ( !stack.empty() ) {
            ID now = std::move( stack.front() );
            stack.pop_front();

            const auto &trace = now.second;
            if ( !graph.insert( trace ).second )
                continue;

            if ( now.first < int( Instantiation::length ) ) {
                branches.emplace_back( now );
                const auto &succs = instantiation[ now.first ];
                for ( auto s : succs ) {
                    auto newtrace = appendArray( trace, s );
                    if ( _valid( newtrace ) )
                        stack.emplace_back( now.first + 1, std::move( newtrace ) );
                }
            } else
                finals.emplace_back( std::move( trace ) );

        }

        emitSelect( branches, finals );
        emitExtern( finals );
        return emitInstances( finals );
    }

    void emitSelect( std::vector< std::pair< int, FixArray< Key > > > branches,
            std::vector< FixArray< Key > > finals )
    {
        emitFileHeader();

        for ( auto trace : finals )
            emitFinalCode( trace );

        for ( auto it = branches.rbegin(); it != branches.rend(); ++it ) {
            auto now = *it;
            const auto &trace = now.second;
            const auto &succs = instantiation[ now.first ];

            emitSelectFnBegin( trace );
            for ( auto s : succs ) {
                auto newtrace = appendArray( trace, s );
                if ( _valid( newtrace ) )
                    emitJumpCode( trace, s );
            }
            emitDefault( trace, succs );
            emitSelectFnEnd();
        }

        emitFileEnd();
    }

    void emitFileHeader() {
        _select << "#include <divine/instances/select.h>" << std::endl
                << "#include <divine/instances/auto/extern.h>" << std::endl
                << "#include <divine/instances/definitions.h>" << std::endl
                << std::endl
                << "namespace divine {" << std::endl
                << "using RetT = std::unique_ptr< ::divine::algorithm::Algorithm >;" << std::endl
                << std::endl;
    }

    void emitFileEnd() {
        _select << std::endl << "}" << std::endl;
    }

    void emitSelectFnEnd() {
        _select << std::endl << "    assert_unreachable( \"end select\" );" << std::endl
                << "}" << std::endl;
    }

    void emitSelectFnBegin( const FixArray< Key > &trace ) {
        std::string label = _label( trace );
        if ( label.size() != 0 )
            _select << std::endl
                    << "RetT " << label << "( Meta &meta ) {" << std::endl;
        else
            _select << "RetT select( Meta &meta ) {" << std::endl;
    }

    void emitJumpCode( const FixArray< Key > &trace, Key next ) {
        std::string label = _label( trace );
        std::string snext = _show( next );
        _select << "    if ( instantiate::_select( meta, " << snext << " ) ) {" << std::endl
                << "        if ( instantiate::_traits( " << snext << " ) ) {" << std::endl;
        if ( options[ next.type ].has( SelectOption::WarnOther ) )
            _select << "            instantiate::_warnOtherAvailable( meta, " << snext << " );" << std::endl;
        _select << "            instantiate::_postSelect( meta, " << snext << " );" << std::endl
                << "            return " << _label( trace, next ) << "( meta );" << std::endl
                << "        } else {" << std::endl
                << "            instantiate::_deactivation( meta, " << snext << " );" << std::endl;
        if ( options[ next.type ].has( SelectOption::WarnUnavailable ) )
            _select << "            instantiate::_warningMissing( " << snext << " );" << std::endl;
        if ( options[ next.type ].has( SelectOption::ErrUnavailable ) )
            _select << "            return instantiate::_errorMissing( " << _ctor( trace, next ) << " );" << std::endl;
        _select << "        }" << std::endl
                << "    }" << std::endl;
    }

    template< typename Succs >
    void emitDefault( const FixArray< Key > &trace, Succs succs ) {
        if ( options[ succs[0].type ].has( SelectOption::LastDefault ) ) {
            for ( auto it = succs.rbegin(); it != succs.rend(); ++it ) {
                if ( _valid( wibble::appendArray( trace, *it ) ) )
                    _select << "    if ( instantiate::_traits( " << _show( *it ) << " ) )" << std::endl
                            << "        return " << _label( trace, *it ) << "( meta );" << std::endl;
            }
        }
        _select << "    return instantiate::_noDefault( " << _ctor( trace ) << " );" << std::endl;
    }
    void emitFinalCode( FixArray< Key > trace ) {
        _select << std::endl
               << "RetT " << _label( trace ) << "( Meta &meta ) {" << std::endl
               << "    instantiate::_init( meta, " << _ctor( trace ) << " );" << std::endl
               << "    return instantiate::create" << _label( trace ) << "( meta );" << std::endl
               << "}" << std::endl;
    }

    void emitExtern( std::vector< FixArray< Key > > finals ) {
        _extern << "#include <divine/utility/meta.h>" << std::endl
                << "#include <divine/utility/die.h>" << std::endl
                << "#include <divine/algorithm/common.h>" << std::endl
                << "#include <memory>" << std::endl
                << "namespace divine {" << std::endl
                 << "namespace instantiate {" << std::endl << std::endl;

        for ( auto symbol : finals )
            _extern << "std::unique_ptr< ::divine::algorithm::Algorithm > create"
                    << _label( symbol ) << "( Meta & );" << std::endl;

        _extern << "}" << std::endl << "}" << std::endl;
    }

    int emitInstances( std::vector< FixArray< Key > > finals ) {
        int size = 0;
        for ( auto symbol : finals )
            if ( _allTraits( symbol ) )
                ++size;

        int perFile;
        if ( _perFile * _instances.size() >= size )
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
        for ( int i = 0; i < _instances.size(); ++i ) {
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
            assert_unreachable( "Algorithm missing in selection." );

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
                 << "    using Store = _Store< _TableProvider, _Generator, _Hasher, _Statistics >;" << std::endl
                 << "    using Graph = _Transform< _Generator, Store, _Statistics >;" << std::endl
                 << "    using Visitor = _Visitor;" << std::endl
                 << "    template< typename I >" << std::endl
                 << "    using Topology = typename _Topology< Transition< Graph, Store > >" << std::endl
                 << "                       ::template T< I >;" << std::endl
                 << "    using Statistics = _Statistics;" << std::endl
                 << "};" << std::endl;

            file << "std::unique_ptr< ::divine::algorithm::Algorithm > create"
                 << _label( symbol ) << "( Meta &meta ) {" << std::endl
                 << "    return std::unique_ptr< ::divine::algorithm::Algorithm >( new "
                 <<          algorithm[ alg ] << "< Setup" << _label( symbol ) << ">( meta ) );" << std::endl
                 << "}" << std::endl << std::endl;
            return true;
        } else {
            file << "std::unique_ptr< ::divine::algorithm::Algorithm > create"
                 << _label( symbol ) << "( Meta & ) {" << std::endl
                 << "    die( \"Missing instance: " << _ctor( symbol ) << ".\" );" << std::endl
                 << "    return nullptr;" << std::endl
                 << "}" << std::endl << std::endl;
            return false;
        }
    }

    bool _allTraits( FixArray< Key > symbol ) {
        return std::accumulate( symbol.begin(), symbol.end(), true,
                []( bool val, Key key ) -> bool {
                    auto tr = traits.find( key );
                    return val && (tr == traits.end() || tr->second( Traits::available() ));
                } );
    }

    bool _evalSuppBy( const SupportedBy &suppBy, const std::vector< Key > &vec ) {
        if ( suppBy.is< Not >() )
            return !_evalSuppBy( *suppBy.get< Not >().val, vec );
        if ( suppBy.is< And >() ) {
            for ( auto s : suppBy.get< And >().val )
                if ( !_evalSuppBy( s, vec ) )
                    return false;
            return true;
        }
        if ( suppBy.is< Or >() ) {
            for ( auto s : suppBy.get< Or >().val )
                if ( _evalSuppBy( s, vec ) )
                        return true;
            return false;
        }
        assert( suppBy.is< Key >() );
        for ( auto v : vec )
            if ( v == suppBy.get< Key >() )
                return true;
        return false;
    }

    bool _valid( FixArray< Key > trace ) {
        std::vector< Key > vec;
        for ( int i = 0; i < trace.size(); ++i ) {
            auto supp = supportedBy.find( trace[ i ] );
            if ( supp != supportedBy.end() ) {
                if ( !_evalSuppBy( supp->second, vec ) )
                    return false;
            }
            vec.push_back( trace[ i ] );
        }
        return true;
    }

    int writeFile( std::string name, std::stringstream &data, uint64_t s1 = 0, uint64_t s2 = 0 ) {
        std::string sdata = data.str();

        std::ifstream ifile{ name };
        if ( !ifile.good() || wibble::sys::fs::readFile( ifile ) != sdata ) {
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
        cnt += writeFile( selName, _select, 1 );
        cnt += writeFile( extName, _extern, 2 );
        for ( int i = 0; i < _instances.size(); ++i )
            cnt += writeFile( instancesPrefix + std::to_string( i + 1 ) + ".cpp",
                        _instances[ i ], 3, i );
        return cnt;
    }


};

}
}

int main( int argc, char** argv ) {
    assert_eq( argc, 3 );

    int files = std::stoi( argv[ 1 ] );
    int min = std::stoi( argv[ 2 ] );
    assert_leq( 1, files );
    assert_leq( 1, min );

    divine::instantiate::InstGenerator gen( min, files );
    int size = gen.buildGraph();
    int regen = gen.writeFiles( "select.cpp", "extern.h", "instances-" );

    std::cerr << "Generated " << size << " instances, " << regen << " new files." << std::endl;
    return 0;
}
