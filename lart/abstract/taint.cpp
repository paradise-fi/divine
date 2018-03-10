// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/taint.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Argument.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/DerivedTypes.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/metadata.h>
#include <lart/abstract/util.h>

#include <lart/support/util.h>

#include <algorithm>

namespace lart {
namespace abstract {

using namespace llvm;

namespace {

std::string taint_type_name( Type *rty, const Types &args ) { 
    std::string res = "." + llvm_name( rty );
    for ( auto a : args )
        res += "." + llvm_name( a );
    return res;
}

Function * get_taint_fn( Instruction *i ) {
    auto m = getModule( i );

    std::string name = "vm.test.taint";

    auto ret = i->getType();

    Types args;
    // TODO add abstract intrinsic to args
    args.push_back( i->getType() ); // fallback type
    for ( auto t : types_of( i->operands() ) )
        args.push_back( t );

    auto fty = FunctionType::get( ret, args, false );

    name += taint_type_name( ret, types_of( i->operands() ) );

    auto fn = m->getOrInsertFunction( name, fty );
    return cast< Function >( fn );
}

bool is_taintable( Value *i ) {
    return is_one_of< BinaryOperator, CmpInst, TruncInst, SExtInst, ZExtInst >( i );
}

void use_tainted_value( Instruction *i, Instruction *orig, Instruction *tainted ) {
    auto op = i->getOperandList();
    while ( op ) {
        if ( op->get() == orig ) {
            op->set( tainted );
            return;
        }
        op = op->getNext();
    }
    UNREACHABLE( "Instruction does not use tainted value." );
}

} // anonymous namespace

void Tainting::run( Module &m ) {
    for ( const auto & mdv : abstract_metadata( m ) )
    	taint( cast< Instruction >( mdv.value() ) );
}

void Tainting::taint( Instruction *i ) {
    if ( !is_taintable( i ) )
        return;

    IRBuilder<> irb( i );
    auto fn = get_taint_fn( i );

    Values args;
    // TODO add abstract intrinsic to args
    args.push_back( i ); // fallback value
    for ( auto & op : i->operands() )
        args.emplace_back( op.get() );

    auto call = irb.CreateCall( fn, args );
    call->removeFromParent();
    call->insertAfter( i );

    for ( const auto & u : i->users() )
        if ( u != call )
            use_tainted_value( cast< Instruction >( u ), i, call );

    tainted.insert( i );
}

} // namespace abstract
} // namespace lart
