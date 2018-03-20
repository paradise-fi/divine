// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/taint.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Argument.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/lib/IR/LLVMContextImpl.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/metadata.h>
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

CallInst *create_call( IRBuilder<> &irb, Function *fn, const Values &args, Domain dom ) {
    auto call = irb.CreateCall( fn, args );

    auto &ctx = fn->getContext();

    MDBuilder mdb( ctx );
    auto dn = mdb.domain_node( dom );
    call->setMetadata( "lart.domains", MDTuple::get( ctx, dn ) );

    return call;
}


inline std::string intrinsic_prefix( Instruction *i, Domain d ) {
    return "lart.gen." + DomainTable[ d ] + "." + i->getOpcodeName();
}

using Predicate = CmpInst::Predicate;
const std::unordered_map< Predicate, std::string > predicate = {
    { Predicate::ICMP_EQ, "eq" },
    { Predicate::ICMP_NE, "ne" },
    { Predicate::ICMP_UGT, "ugt" },
    { Predicate::ICMP_UGE, "uge" },
    { Predicate::ICMP_ULT, "ult" },
    { Predicate::ICMP_ULE, "ule" },
    { Predicate::ICMP_SGT, "sgt" },
    { Predicate::ICMP_SGE, "sge" },
    { Predicate::ICMP_SLT, "slt" },
    { Predicate::ICMP_SLE, "sle" }
};

std::string intrinsic_name( Instruction *i, Domain d ) {
    auto pref = intrinsic_prefix( i, d );

    if ( auto icmp = dyn_cast< ICmpInst >( i ) )
        return pref + "_" + predicate.at( icmp->getPredicate() )
                    + "." + llvm_name( icmp->getOperand( 0 )->getType() );

    if ( isa< BinaryOperator >( i ) )
        return pref + "." + llvm_name( i->getType() );

    if ( auto ci = dyn_cast< CastInst >( i ) )
        return pref + "." + llvm_name( ci->getSrcTy() )
                    + "." + llvm_name( ci->getDestTy() );

    UNREACHABLE( "Unhandled intrinsic." );
}

Function* get_intrinsic( Module *m, std::string name, Type *rty, const Types &args ) {
    auto &ctx = rty->getContext();

    Types arg_types;
    for ( auto &a : args ) {
        arg_types.push_back( Type::getInt1Ty( ctx ) );
        arg_types.push_back( a );
    }

    auto fty = FunctionType::get( rty, arg_types, false );
    return cast< Function >( m->getOrInsertFunction( name, fty ) );
}

Function* get_intrinsic( Instruction *i, Domain d ) {
    return get_intrinsic( get_module( i ), intrinsic_name( i, d ),
                          i->getType(), types_of( i->operands() ) );
}

Function* intrinsic( Instruction *i ) {
    auto d = MDValue( i ).domain();
    return get_intrinsic( i, d );
}

Instruction* branch_intrinsic( Instruction *i, std::string name ) {
    auto i8 = Type::getInt8Ty( i->getContext() );
    assert( i->getType() == i8 );
    auto fn = get_intrinsic( get_module( i ), name, i8, { i8 } );
    return create_taint( i, { fn, i, i } );
}

Instruction* to_tristate( Instruction *i, Domain dom ) {
    auto &ctx = i->getContext();
    MDBuilder mdb( ctx );
    auto dn = mdb.domain_node( Domain::Tristate );
    auto b2t = branch_intrinsic( i, "lart.gen." + DomainTable[ dom ] + ".bool_to_tristate" );
    b2t->setMetadata( "lart.domains", MDTuple::get( ctx, { dn } ) );
    return b2t;
}

Instruction* lower_tristate( Instruction *i ) {
    auto lt = branch_intrinsic( i, "lart.gen.tristate.lower" );
    lt->setMetadata( "lart.domains", nullptr );
    return lt;
}

Type* abstract_type( Type *t, Domain dom ) {
    std::string name;
    if ( dom == Domain::Tristate )
        name = "lart." + DomainTable[ dom ];
    else
		name = "lart." + DomainTable[ dom ] + "." + llvm_name( t );

    if ( auto aty = t->getContext().pImpl->NamedStructTypes.lookup( name ) )
        return aty;
    return StructType::create( { t }, name );

}

std::string lift_name( Type *t, Domain dom ) {
    if ( dom == Domain::Tristate )
        return "lart." + DomainTable[ dom ] + ".lift" ;
    else
		return "lart." + DomainTable[ dom ] + ".lift." + llvm_name( t );
}

Function* lift( Value *val, Domain dom ) {
    auto m = get_module( val );
    auto ty = val->getType();
    auto aty = abstract_type( ty, dom );
    auto name = lift_name( ty, dom );
    auto fty = FunctionType::get( aty, { ty }, false );
    return cast< Function >( m->getOrInsertFunction( name, fty ) );
}

Function* rep( Value *val, Domain dom ) {
    auto m = get_module( val );
	auto ty = val->getType();
	auto aty = abstract_type( ty, dom );
	auto fty = FunctionType::get( aty, { aty }, false );
	auto name = "lart." + DomainTable[ dom ] + ".rep." + llvm_name( ty );
    return cast< Function >( m->getOrInsertFunction( name, fty ) );
}

Function* unrep( Value *val, Domain dom, Type *to ) {
    auto m = get_module( val );
	auto ty = val->getType();
	auto fty = FunctionType::get( ty, { ty }, false );
	auto name = "lart." + DomainTable[ dom ] + ".unrep." + llvm_name( to );
    return cast< Function >( m->getOrInsertFunction( name, fty ) );
}

BasicBlock* make_bb( Function *fn, std::string name ) {
	auto &ctx = fn->getContext();
	return BasicBlock::Create( ctx, name, fn );
}

BasicBlock* entry_bb( Value *tflag, size_t idx ) {
	auto &ctx = tflag->getContext();

    std::string name = "arg." + std::to_string( idx ) + ".entry";
	auto bb = make_bb( get_function( tflag ), name );

    IRBuilder<> irb( bb );
    irb.CreateICmpEQ( tflag, ConstantInt::getTrue( ctx ) );
	return bb;
}

BasicBlock* tainted_bb( Value *arg, size_t idx, Domain dom ) {
    std::string name = "arg." + std::to_string( idx ) + ".tainted";
	auto bb = make_bb( get_function( arg ), name );

    auto aty = abstract_type( arg->getType(), dom );
	auto cs = Constant::getNullValue( aty );

	IRBuilder<> irb( bb );
	auto iv = cast< Instruction >( irb.CreateInsertValue( cs, arg, 0 ) );
	auto call = create_call( irb, rep( arg, dom ), { iv }, dom );
    iv->setMetadata( "lart.domains", call->getMetadata( "lart.domains" ) );

	return bb;
}

BasicBlock* untainted_bb( Value *arg, size_t idx, Domain dom ) {
    std::string name = "arg." + std::to_string( idx ) + ".untainted";
	auto bb = make_bb( get_function( arg ), name );
    IRBuilder<> irb( bb );
    create_call( irb, lift( arg, dom ), { arg }, dom );
	return bb;
}

BasicBlock* merge_bb( Value *arg, size_t idx ) {
    std::string name = "arg." + std::to_string( idx ) + ".merge";
	auto bb = make_bb( get_function( arg ), name );
	return bb;
}

void join_bbs( BasicBlock *ebb, BasicBlock *tbb, BasicBlock *ubb,
			   BasicBlock *mbb, BasicBlock *exbb )
{
	IRBuilder<> irb( ebb );
    irb.CreateCondBr( &ebb->back(), tbb, ubb );

	irb.SetInsertPoint( mbb );
	auto tv = &tbb->back();
	auto uv = &ubb->back();
	assert( tv->getType() == uv->getType() );
	auto phi = irb.CreatePHI( tv->getType(), 2 );
	phi->addIncoming( tv, tbb );
	phi->addIncoming( uv, ubb );
    phi->setMetadata( "lart.domains", tv->getMetadata( "lart.domains" ) );

    exbb->moveAfter( mbb );
	irb.CreateBr( exbb );

    irb.SetInsertPoint( tbb );
    irb.CreateBr( mbb );

	irb.SetInsertPoint( ubb );
    irb.CreateBr( mbb );
}

Function* make_abstract_op( CallInst *taint, Types args ) {
    auto fn = cast< Function >( taint->getOperand( 0 ) );
    auto m = get_module( taint );

	std::string prefix = "lart.gen.";
	auto name = "lart." + fn->getName().drop_front( prefix.size() );

	auto rty = ( taint->getMetadata( "lart.domains" ) )
		     ? abstract_type( taint->getType(), MDValue( taint ).domain() )
		     : taint->getType();

    auto fty = FunctionType::get( rty, args, false );
	return cast< Function >( m->getOrInsertFunction( name.str(), fty ) );
}

void exit_lifter( BasicBlock *exbb, CallInst *taint, Values &args ) {
    auto fn = cast< Function >( taint->getOperand( 0 ) );

    Domain dom;
    if ( taint->getMetadata( "lart.domains" ) )
        dom = MDValue( taint ).domain();
    else
        dom = MDValue( taint->getOperand( 1 ) ).domain();

    IRBuilder<> irb( exbb );
	auto aop = make_abstract_op( taint, types_of( args ) );
	auto call = create_call( irb, aop, args, dom );
	if ( call->getType() != fn->getReturnType() ) {
        auto to = fn->getReturnType();
		auto ur = create_call( irb, unrep( call, dom, to ), { call }, dom );
        auto ev = irb.CreateExtractValue( ur, 0 );
		irb.CreateRet( ev );
	} else {
		irb.CreateRet( call );
	}
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

    auto fn = get_taint_fn( get_module( i ), rty, types_of( args ) );

    auto call = irb.CreateCall( fn, args );
    call->removeFromParent();
    call->insertAfter( i );
    call->setMetadata( "lart.domains", i->getMetadata( "lart.domains" ) );
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

    auto ti = cast< Instruction >( t );
    auto i8 = cast< Instruction >( irb.CreateZExt( ti, Type::getInt8Ty( ctx ) ) );
    i8->setMetadata( "lart.domains", ti->getMetadata( "lart.domains" ) );

    auto trs = to_tristate( cast< Instruction >( i8 ), dom );
    auto low = lower_tristate( trs );
    auto i1 = irb.CreateTrunc( low, Type::getInt1Ty( ctx ) );

    br->setCondition( i1 );
}

bool is_assume_taint( CallInst *taint ) {
    auto fn = taint->getOperand( 0 );
    return fn->getName().count( ".assume" );
}

void LifterSynthesize::run( Module &m ) {
    for ( auto t : taints( m ) ) {
        auto call = cast< CallInst >( t );
        auto fn = cast< Function >( call->getOperand( 0 ) );
        if ( fn->empty() ) {
            if ( is_assume_taint( call ) )
                process_assume( call );
            else
                process( call );
        }
    }
}

void LifterSynthesize::process( CallInst *taint ) {
    auto fn = cast< Function >( taint->getOperand( 0 ) );
    using LifterArg = std::pair< Value*, Value* >; // tainted flag + value

    std::vector< LifterArg > args;
    for ( auto it = fn->arg_begin(); it != fn->arg_end(); std::advance( it, 2 ) )
        args.emplace_back( it, std::next( it ) );

    auto dom = MDValue( taint->getOperand( 1 ) ).domain();

    auto exbb = make_bb( fn, "exit" );

    size_t idx = 0;
    BasicBlock *prev = nullptr;

    Values lifted;
    for ( const auto &arg : args ) {
        auto ebb = entry_bb( arg.first, idx );
        auto tbb = tainted_bb( arg.second, idx, dom );
        auto ubb = untainted_bb( arg.second, idx, dom );
        auto mbb = merge_bb( arg.second, idx );

        join_bbs( ebb, tbb, ubb, mbb, exbb );
        lifted.push_back( mbb->begin() );

        if ( prev ) {
            prev->getTerminator()->setSuccessor( 0, ebb );
        }

        prev = mbb;
        idx++;
    }

    exit_lifter( exbb, taint, lifted );
}

void LifterSynthesize::process_assume( CallInst *taint ) {
    auto fn = cast< Function >( taint->getOperand( 0 ) );
    auto dom = MDValue( taint->getOperand( 1 ) ).domain();

	auto arg = std::next( fn->arg_begin() );
	auto ebb = make_bb( fn, "entry" );

    auto aty = abstract_type( arg->getType(), dom );
	auto cs = Constant::getNullValue( aty );

	IRBuilder<> irb( ebb );
	auto iv = cast< Instruction >( irb.CreateInsertValue( cs, arg, 0 ) );
	auto ar = create_call( irb, rep( arg, dom ), { iv }, dom );
    iv->setMetadata( "lart.domains", ar->getMetadata( "lart.domains" ) );

    Values args = { ar, std::next( fn->arg_begin(), 3 ) };

	auto aop = make_abstract_op( taint, types_of( args ) );
	auto icall = create_call( irb, aop, args, dom );
    auto to = fn->getReturnType();
    auto ur = create_call( irb, unrep( icall, dom, to ), { icall }, dom );
    auto ev = irb.CreateExtractValue( ur, 0 );
    irb.CreateRet( ev );
}

} // namespace abstract
} // namespace lart
