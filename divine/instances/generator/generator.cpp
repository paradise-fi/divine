// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <divine/instances/select-impl.h>

#include <array>
#include <cstdint>
#include <cmath>
#include <iterator>
#include <fstream>
#include <iostream>

namespace divine {
namespace instantiate {

static const std::vector< std::string > defaultHeaders = {
        "divine/instances/definitions.h",
        "divine/instances/create.h",
        "divine/instances/auto/extern.h"
    };

struct Symbol {
    using str = std::string;

    std::array< Atom, Instantiate::length > atoms;

    template< typename List >
    Symbol( List ) {
        static_assert( List::length == Instantiate::length, "Invalid symbol list" );
        saveAtoms< 0 >( List() );
    }

    template< size_t I, typename List >
    auto saveAtoms( List )
        -> typename std::enable_if< ( List::length > 0 ) >::type
    {
        std::get< I >( atoms ) = Atom( typename List::Head() );
        saveAtoms< I + 1 >( typename List::Tail() );
    }

    template< size_t I, typename List >
    auto saveAtoms( List )
        -> typename std::enable_if< List::length == 0 >::type
    { }

    auto headers() const -> std::set< std::string > {
        std::set< std::string > head;
        auto ins = std::inserter( head, head.begin() );
        mapAtoms( [ &ins ]( const Atom &atom, size_t ) {
                std::copy( atom.headers.begin(), atom.headers.end(), ins );
            } );
        return head;
    }

    auto typeList() const -> std::string {
        std::stringstream ss;
        ss << "divine::TypeList< ";
        mapAtoms( [ &ss ]( const Atom &atom, size_t i ) {
                ss << atom.name;
                if ( i < Instantiate::length - 1 )
                    ss << ", ";
            } );
        ss << " >";
        return ss.str();
    }

    auto specializationDecl() const -> std::string {
        std::stringstream ss;
        specializationDecl( ss );
        return ss.str();
    }

    template< typename OStream >
    void specializationDecl( OStream &os ) const {
        os << "template<>" << std::endl
           << "AlgoR createInstance< " << typeList() << " >( Meta & );" << std::endl;
/*           << "{" << std::endl
           << "    static AlgoR create( Meta & );" << std::endl
           << "};" << std::endl;*/
    }

    auto externDeclaration() const -> std::string {
        return str( "extern template AlgoR createInstance< " ) + typeList() + str( " >( Meta & );" );
    }

    auto specialization() const -> std::string {
        std::stringstream ss;
        specialization( ss );
        return ss.str();
    }

    template< typename OStream >
    void specialization( OStream &os ) const {
        os << "namespace divine {" << std::endl
           << "namespace instantiate {" << std::endl
           << "template<>" << std::endl
           << "struct Setup< " << typeList() << " >" << std::endl
           << "{" << std::endl

           << "  private:" << std::endl
           << "    using _Hasher = ::divine::algorithm::Hasher;" << std::endl;
        mapAtoms( [ &os ]( const Atom &a, size_t ) {
                os << a.get;
            } );

        os << "  public:" << std::endl
           << "    using Store = _Store< _TableProvider, _Generator, _Hasher, _Statistics >;" << std::endl
           << "    using Graph = _Transform< _Generator, Store, _Statistics >;" << std::endl
           << "    using Visitor = _Visitor;" << std::endl
           << "    template< typename I >" << std::endl
           << "    using Topology = typename _Topology< Transition< Graph, Store > >" << std::endl
           << "                       ::template T< I >;" << std::endl
           << "    using Statistics = _Statistics;" << std::endl
           << "    friend AlgoR createInstance< " << typeList() << " >( Meta & );" << std::endl
           << "};" << std::endl
           << std::endl
           << "template<>" << std::endl
           << "AlgoR createInstance< " << typeList() << " >( Meta &meta )" << std::endl
           << "{" << std::endl
           << "    using Setup = Setup< " << typeList() << " >;" << std::endl
           << "    return AlgoR( new Setup::_Algorithm< Setup >( meta ) );" << std::endl
           << "}" << std::endl
           << std::endl
           << "template AlgoR createInstance< " << typeList() << " >( Meta & );" << std::endl
           << "}" << std::endl
           << "}" << std::endl;
    }

    template< typename Fn >
    void mapAtoms( Fn fn ) const {
        _mapAtoms< 0 >( fn );
    }

    template< size_t I, typename Fn >
    auto _mapAtoms( Fn fn ) const
        -> typename std::enable_if< ( I < Instantiate::length ) >::type
    {
        fn( std::get< I >( atoms ), I );
        _mapAtoms< I + 1 >( fn );
    }

    template< size_t I, typename Fn >
    auto _mapAtoms( Fn ) const
        -> typename std::enable_if< I == Instantiate::length >::type
    { }
};

bool operator<( const Atom &a, const Atom &b ) {
    return a.name < b.name;
}

bool operator<( const Symbol &a, const Symbol &b ) {
    return a.atoms < b.atoms;
}

struct SymbolListSelector {
    using ReturnType = wibble::Unit;

    struct Data {
        std::set< Symbol > symbols;
    };

    std::shared_ptr< Data > _data;

    SymbolListSelector() : _data( new Data() ) { }

    std::vector< Symbol > symbols() {
        return std::vector< Symbol >(
                _data->symbols.begin(), _data->symbols.end() );
    }

    std::set< Symbol > symbolSet() const {
        return _data->symbols;
    }

    std::set< Symbol > symbolSet() {
        return _data->symbols;
    }

    void addSymbol( Symbol sym ) {
        _data->symbols.insert( sym );
    }

    template< typename T, typename TrueFn, typename FalseFn >
    ReturnType ifSelect( T, TrueFn trueFn, FalseFn falseFn ) {
        trueFn();
        return falseFn();
    }

    template< typename T >
    ReturnType instantiationError( T ) { return ReturnType(); }

    template< typename Selected >
    ReturnType create() {
        if ( checkSelected( Selected(), Traits::compiled() ) )
            addSymbol( Symbol( Selected() ) );
        return wibble::Unit();
    }

    template< typename Selected >
    auto checkSelected( Selected, const Traits &tr ) -> typename
        std::enable_if< ( Selected::length > 0 ), bool >::type
    {
        return Selected::Head::trait( tr )
            && checkSelected( typename Selected::Tail(), tr );
    }

    template< typename Selected >
    auto checkSelected( Selected, const Traits & ) -> typename
        std::enable_if< Selected::length == 0, bool >::type
    {
        return true;
    }

    auto externDeclarations() const -> std::string {
        std::stringstream ss;
        externDeclarations( ss );
        return ss.str();
    }

    template< typename OStream >
    void externDeclarations( OStream &os ) const {
        os << "#include <divine/instances/create.h>" << std::endl
           << "namespace divine {" << std::endl
           << "namespace instantiate {" << std::endl;
        for ( const auto &sym : symbolSet() ) {
            sym.specializationDecl( os );
            os << sym.externDeclaration() << std::endl;
        }
        os << "}" << std::endl
           << "}" << std::endl;
    }
};

void printInclude( const std::string &h, std::ofstream &file ) {
    file << "#include <" << h << ">" << std::endl;
}

void definitions( std::set< Symbol > &sym, int i ) {
    std::set< std::string > headers;
    auto inserter = std::inserter( headers, headers.begin() );
    for ( const auto &symbol : sym ) {
        auto sh = symbol.headers();
        std::copy( sh.begin(), sh.end(), inserter );
    }

    std::stringstream ss;
    ss << "instance-" << i << ".cpp";
    std::ofstream file( ss.str() );

    if ( sym.size() ) {
        for ( const auto &h : defaultHeaders )
            printInclude( h, file );
        for ( const auto &h : headers )
            printInclude( h, file );
        file << std::endl;

        for ( const auto &symbol : sym )
            symbol.specialization( file );
    }
}

}
}

int main( int argc, char** argv ) {
    using namespace divine;
    using namespace divine::instantiate;

    SymbolListSelector sls;
    runSelector( sls, Instantiate(), TypeList<>() );

    assert_eq( argc, 3 );

    int files = std::stoi( argv[ 1 ] );
    int min = std::stoi( argv[ 2 ] );
    assert_leq( 1, files );
    assert_leq( 1, min );

    std::ofstream externh( "extern.h" );
    sls.externDeclarations( externh );
    externh.close();

    int perFile = std::max( min, int( std::ceil( double( sls.symbolSet().size() ) / files ) ) );
    std::set< Symbol > sym;
    int used = 0;
    auto symbols = sls.symbolSet();
    auto it = symbols.begin();
    auto end = symbols.end();
    for ( int i = 1; i <= files; ++i ) {
        for ( int cnt = 0 ; cnt < perFile && it != end; ++it, ++cnt, ++used )
            sym.insert( *it );
        definitions( sym, i );
        sym.clear();
    }
    assert_eq( used, int( symbols.size() ) );
    static_cast< void >( used );
    std::cerr << "Generated " << symbols.size() << " instances." << std::endl;
    return 0;
}
