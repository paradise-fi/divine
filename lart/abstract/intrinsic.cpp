// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/intrinsic.h>

#include <sstream>

namespace lart {
namespace abstract {

namespace intrinsic {

namespace {
    std::vector< std::string > parse( const llvm::Function * fn ) {
        if ( !fn || !fn->hasName() )
            return {};
        auto name = fn->getName();
        std::istringstream ss(name);

        std::string part;
        std::vector< std::string > parts;
        while( std::getline( ss, part, '.' ) ) {
            parts.push_back( part );
        }

        return parts;
    }

    std::vector< llvm::Type * > typesOf( const std::vector< llvm::Value * > & vs ) {
        std::vector< llvm::Type * > ts;
        std::transform( vs.begin(), vs.end(), std::back_inserter( ts ),
            [] ( const auto & v ) { return v->getType(); } );
        return ts;
    }
}

// intrinsic format: lart.<domain>.<name>.<type1>.<type2>
MaybeDomain domain( const llvm::Function * fn ) {
    auto parts = parse( fn );
    assert( parts.size() > 2 );
    return Domain::value( parts[1] );
}

MaybeDomain domain( const llvm::CallInst * call ) {
    return domain( call->getCalledFunction() );
}


const std::string name( const llvm::Function * fn ) {
    auto parts = parse( fn );
    return parts.size() > 2 ? parts[2] : "";
}

const std::string ty1( const llvm::Function * fn ) {
    auto parts = parse( fn );
    return parts.size() > 3 ? parts[3] : "";
}

const std::string ty2( const llvm::Function * fn ) {
    auto parts = parse( fn );
    return parts.size() > 4 ? parts[4] : "";
}

const std::string name( const llvm::CallInst * call ) {
    return name( call->getCalledFunction() );
}

const std::string ty1( const llvm::CallInst * call ) {
    return ty1( call->getCalledFunction() );
}

const std::string ty2( const llvm::CallInst * call ) {
    return ty2( call->getCalledFunction() );
}

const std::string tag( const AbstractValue & av ) {
    return tag( llvm::cast< llvm::Instruction >( av.value() ), av.domain() );
}

const std::string tag( const llvm::Instruction * i, Domain::Value dom  ) {
    return std::string("lart.") + Domain::name( dom ) +
           "." + i->getOpcodeName( i->getOpcode() );
}

llvm::Function * get( llvm::Module * m,
                      llvm::Type * rty,
                      const std::string & tag,
                      llvm::ArrayRef < llvm::Type * > types )
{
	auto fty = llvm::FunctionType::get( rty, types, false );
    return llvm::cast< llvm::Function >( m->getOrInsertFunction( tag, fty ) );
}

llvm::CallInst * build( llvm::Module * m,
                        llvm::IRBuilder<> & irb,
                        llvm::Type * rty,
                        const std::string & tag,
                        std::vector< llvm::Value * > args )
{
    auto call = get( m, rty, tag, typesOf( args ) );
    return irb.CreateCall( call, args );
}

//helpers
bool is( const llvm::Function * fn ) {
    auto parts = parse( fn );
    return parts.size() > 2 && parts[0] == "lart";
}

bool is( const llvm::CallInst * call ) {
    return intrinsic::is( call->getCalledFunction() );
}

bool isAssume( const llvm::CallInst * call ) {
    return intrinsic::name( call ) == "assume";
}

bool isLift( const llvm::CallInst * call ) {
    return intrinsic::name( call ) == "lift";
}

bool isLower( const llvm::CallInst * call ) {
    return intrinsic::name( call ) == "lower";
}

} // namespace intrinsic

} // namespace abstract
} // namespace lart
