// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/util.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/lib/IR/LLVMContextImpl.h>
DIVINE_UNRELAX_WARNINGS

#include <unordered_map>
#include <brick-llvm>
#include <brick-string>

namespace lart {
namespace abstract {

using namespace llvm;

std::string llvm_name( Type *type ) {
    std::string buffer;
    raw_string_ostream rso( buffer );
    type->print( rso );
    return rso.str();
}

Domain get_domain( Type *type ) {
    auto st = cast< StructType >( type );
    auto name = st->getName().split('.').second.split('.').first;
    return Domain( name.str() );
}

bool is_intr( CallInst *intr, std::string name ) {
    assert( intr->getCalledFunction()->getName().startswith( "__vm_test_taint." ) );
    return intr->getCalledFunction()->getName().count( name );
}

bool is_lift( CallInst *intr ) { return is_intr( intr, ".lift" ); }
bool is_lower( CallInst *intr ) { return is_intr( intr, ".lower" ); }
bool is_assume( CallInst *intr ) { return is_intr( intr, ".assume" ); }
bool is_thaw( CallInst *intr ) { return is_intr( intr, ".thaw" ); }
bool is_freeze( CallInst *intr ) { return is_intr( intr, ".freeze" ); }
bool is_tobool( CallInst *intr ) { return is_intr( intr, ".to_i1" ); }
bool is_cast( CallInst *intr ) {
    return is_intr( intr, ".zext" ) ||
           is_intr( intr, ".sext" ) ||
           is_intr( intr, ".trunc" ) ||
           is_intr( intr, ".fpext" ) ||
           is_intr( intr, ".fptrunc" ) ||
           is_intr( intr, ".bitcast" );
}

Values taints( Module &m ) {
    Values res;
    for ( auto &fn : m )
        if ( fn.getName().startswith( "__vm_test_taint" ) )
            for ( auto u : fn.users() )
                res.push_back( u );
    return res;
}

Function* get_function( Argument *a ) {
    return a->getParent();
}

Function* get_function( Instruction *i ) {
    return i->getParent()->getParent();
}

Function* get_function( Value *v ) {
    if ( auto arg = dyn_cast< Argument >( v ) )
        return get_function( arg );
    return get_function( cast< Instruction >( v ) );
}

Function* get_or_insert_function( Module *m, FunctionType *fty, StringRef name ) {
    auto fn = cast< Function >( m->getOrInsertFunction( name, fty ) );
    fn->addFnAttr( llvm::Attribute::NoUnwind );
    return fn;
}

Module* get_module( llvm::Value *v ) {
    return get_function( v )->getParent();
}

Type* abstract_type( Type *orig, Domain dom ) {
    std::string name;
    if ( dom == Domain::Tristate() )
        name = "lart." + dom.name();
    else
		name = "lart." + dom.name() + "." + llvm_name( orig );

    if ( auto aty = orig->getContext().pImpl->NamedStructTypes.lookup( name ) )
        return aty;
    return StructType::create( { orig }, name );
}

Instruction* find_placeholder( Value *val, std::string name ) {
    if ( isa< Constant >( val ) )
        return nullptr;

    for ( auto u : val->users() )
        if ( auto call = dyn_cast< CallInst >( u ) ) {
            auto fn = call->getCalledFunction();
            if ( fn && fn->hasName() && fn->getName().startswith( name ) )
                return call;
        }
    return nullptr;
}

Instruction* get_placeholder_impl( Value *val, const std::string &name ) {
    if ( auto ph = find_placeholder( val, name ) ) {
        return ph;
    } else
        UNREACHABLE( "Value does not have", name, "val =", val );
}

Instruction* get_placeholder( Value *val ) {
    return get_placeholder_impl( val, "lart.placeholder" );
}

Instruction* get_unstash_placeholder( Value *val ) {
    return get_placeholder_impl( val, "lart.unstash.placeholder" );
}

Instruction* get_placeholder_in_domain( Value *val, Domain dom ) {
    auto name = "lart." + dom.name() + ".placeholder";
    return get_placeholder_impl( val, name );
}

bool has_placeholder( llvm::Value *val, const std::string &name ) {
    return find_placeholder( val, name );
}

bool has_placeholder( Value *val ) {
    return has_placeholder( val, "lart.placeholder" );
}

bool has_placeholder_in_domain( Value *val, Domain dom ) {
    auto name = "lart." + dom.name() + ".placeholder";
    return find_placeholder( val, name );
}

bool is_placeholder( Instruction* inst ) {
    if ( auto call = dyn_cast< CallInst >( inst ) ) {
        auto fn = call->getCalledFunction();
        if ( fn && fn->hasName() ) {
            auto name = fn->getName();
            return name.count( "lart." ) && name.count( ".placeholder" );
        }
    }

    return false;
}

std::vector< Instruction* > placeholders( Module &m ) {
    return query::query( m ).flatten().flatten()
        .map( query::refToPtr )
        .filter( query::llvmdyncast< CallInst > )
        .filter( query::notnull )
        .filter( is_placeholder )
        .freeze();
}


bool is_base_type( Type *type ) {
    // TODO determine from domain metadata what is base type
    return type->isIntegerTy() || type->isFloatingPointTy();
}

// Tries to find precise set of possible called functions.
// Returns true if it succeeded.
bool potentialy_called_functions( Value * called, Functions & fns ) {
    bool surrender = true;
    llvmcase( called,
        [&] ( Function * fn ) {
            fns.push_back( fn );
        },
        [&] ( PHINode * phi ) {
            for ( auto & iv : phi->incoming_values() ) {
                if ( ( surrender = potentialy_called_functions( iv.get(), fns ) ) )
                    break;
            }
        },
        [&] ( LoadInst * ) { surrender = true; }, // TODO
        [&] ( Argument * ) { surrender = true; },
        [&] ( Value * val ) {
            UNREACHABLE( "Unknown parent instruction:", val );
        }
    );

    return !surrender;
}

std::vector< Function* > get_potentialy_called_functions( llvm::CallInst* call ) {
    auto val = call->getCalledValue();
    if ( auto fn = dyn_cast< Function >( val ) )
        return { fn };
    else if ( auto ce = dyn_cast< ConstantExpr >( val ) )
        return { cast< Function >( ce->stripPointerCasts() ) };

    auto m = get_module( call );
    auto type = call->getCalledValue()->getType();

    if ( !isa< Argument >( call->getCalledValue() ) ) {
        Functions fns;
        if ( potentialy_called_functions( call->getCalledValue(), fns) ) {
            return fns;
        }
    }

    // brute force all possible functions with correct signature
    return query::query( m->functions() )
        .filter( [type] ( auto & fn ) { return fn.getType() == type; } )
        .filter( [] ( auto & fn ) { return fn.hasAddressTaken(); } )
        .map( query::refToPtr )
        .freeze();
}

llvm::Function * get_some_called_function( llvm::CallInst * call ) {
    return get_potentialy_called_functions( call ).front();
}

} // namespace abstract
} // namespace lart
