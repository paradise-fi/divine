// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/intrinsic.h>

#include <sstream>

namespace lart {
namespace abstract {

namespace {
    std::vector< std::string > parse( const std::string & name ) {
        std::istringstream ss(name);
        std::string part;
        std::vector< std::string > parts;
        while( std::getline( ss, part, '.' ) ) {
            parts.push_back( part );
        }
        return parts;
    }

    std::vector< std::string > parse( const llvm::Function * fn ) {
        if ( !fn || !fn->hasName() )
            return {};
        return parse( fn->getName() );
    }

    std::vector< llvm::Type * > typesOf( const std::vector< llvm::Value * > & vs ) {
        std::vector< llvm::Type * > ts;
        std::transform( vs.begin(), vs.end(), std::back_inserter( ts ),
            [] ( const auto & v ) { return v->getType(); } );
        return ts;
    }
}

Intrinsic::Intrinsic( llvm::Module * m,
                      llvm::Type * rty,
                      const std::string & tag,
                      const ArgTypes & args )
{
    assert( parse( tag ).size() > 2 );
    auto fty = llvm::FunctionType::get( rty, args, false );
    intr = llvm::cast< llvm::Function >( m->getOrInsertFunction( tag, fty ) );
}

DomainPtr Intrinsic::domain() const {
    auto dom = nameElement( 1 );
    if ( dom != "struct" )
        return ::lart::abstract::domain( dom );
    UNREACHABLE( "Intrinsic domain fro structs not yet implemented." );
}

std::string Intrinsic::name() const {
    return nameElement( 2 );
}

bool Intrinsic::is() const {
    auto parts = parse( intr );
    return parts.size() > 2 && parts[0] == "lart";
}

Intrinsic::Type Intrinsic::type() const {
    auto n = name();
    if ( n == "lift" )
        return Intrinsic::Type::Lift;
    if ( n == "lower" )
        return Intrinsic::Type::Lower;
    if ( n == "assume" )
        return Intrinsic::Type::Assume;
    return Intrinsic::Type::LLVM;
}

std::string Intrinsic::nameElement( size_t idx ) const {
    auto pars = parse( intr );
    return pars.size() > idx ? pars[ idx ] : "";
}

} // namespace abstract
} // namespace lart
