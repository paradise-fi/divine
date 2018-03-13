// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/taint.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Argument.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/DerivedTypes.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/metadata.h>
#include <lart/abstract/intrinsics.h>

#include <lart/support/util.h>

#include <algorithm>

namespace lart {
namespace abstract {

using namespace llvm;

namespace {

std::string taint_suffix( const Types &args ) {
    std::string res;
    for ( auto a : args )
        res += "." + llvm_name( a );
    return res;
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

Function* intrinsic( Instruction *i ) {
    auto d = MDValue( i ).domain();
    return get_intrinsic( i, d );
}

Instruction* branch_intrinsic( Instruction *i, std::string name ) {
    auto i8 = Type::getInt8Ty( i->getContext() );
    assert( i->getType() == i8 );
    auto fn = get_intrinsic( getModule( i ), name, i8, { i8 } );
    return create_taint( i, { fn, i, i } );
}

Instruction* to_tristate( Instruction *i, Domain dom ) {
    return branch_intrinsic( i, "lart.gen." + DomainTable[ dom ] + ".bool_to_tristate" );
}

Instruction* lower_tristate( Instruction *i ) {
    return branch_intrinsic( i, "lart.gen.tristate.lower" );
}

} // anonymous namespace

Function* get_taint_fn( Module *m, Type *ret, Types args ) {
    auto taint_fn = args.front();
    args.erase( args.begin() );
    auto name = "__vm_test_taint" + taint_suffix( args );
    args.insert( args.begin(), taint_fn );

    auto fty = FunctionType::get( ret, args, false );
    auto fn = m->getOrInsertFunction( name, fty );
    return cast< Function >( fn );
}

Instruction* create_taint( Instruction *i, const Values &args ) {
    IRBuilder<> irb( i );

    auto rty = i->getType();

    auto fn = get_taint_fn( getModule( i ), rty, types_of( args ) );

    auto call = irb.CreateCall( fn, args );
    call->removeFromParent();
    call->insertAfter( i );
    return call;
}

bool is_taintable( Value *i ) {
    return is_one_of< BinaryOperator, CmpInst, TruncInst, SExtInst, ZExtInst >( i );
}

void Tainting::run( Module &m ) {
    for ( const auto & mdv : abstract_metadata( m ) )
    	taint( cast< Instruction >( mdv.value() ) );
}

void Tainting::taint( Instruction *i ) {
    if ( !is_taintable( i ) )
        return;

    Values args;
    args.push_back( intrinsic( i ) );
    args.push_back( i ); // fallback value
    for ( auto & op : i->operands() )
        args.emplace_back( op.get() );

    auto call = create_taint( i, args );

    for ( const auto & u : i->users() )
        if ( u != call )
            use_tainted_value( cast< Instruction >( u ), i, call );

    tainted.insert( i );
}

void TaintBranching::run( Module &m ) {
    for ( auto t : taints( m ) )
        for ( auto u : t->users() )
            if ( auto br = dyn_cast< BranchInst >( u ) )
                expand( t, br );
}

void TaintBranching::expand( Value *t, BranchInst *br ) {
    IRBuilder<> irb( br );
    auto &ctx = br->getContext();

    auto orig = cast< User >( t )->getOperand( 1 ); // fallback value
    auto dom = MDValue( orig ).domain();

    auto i8 = irb.CreateZExt( t, Type::getInt8Ty( ctx ) );
    auto trs = to_tristate( cast< Instruction >( i8 ), dom );
    auto low = lower_tristate( trs );
    auto i1 = irb.CreateTrunc( low, Type::getInt1Ty( ctx ) );

    br->setCondition( i1 );
}

} // namespace abstract
} // namespace lart
