#include <lart/abstract/substitution.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>
#include <lart/support/query.h>
#include <lart/support/util.h>
#include <lart/abstract/stash.h>
#include <lart/abstract/taint.h>
#include <lart/abstract/metadata.h>

#include <iostream>

namespace lart {
namespace abstract {

using namespace llvm;

namespace {

Type * strip_pointers( Type *t ) {
    return t->isPointerTy() ? t->getPointerElementType() : t;
}

bool is_abstract( Type *t ) {
    t = strip_pointers( t );
    if ( auto s = dyn_cast< StructType >( t ) )
        if ( s->hasName() )
            return s->getName().startswith( "lart." );
    return false;
}

auto abstract_insts( Module &m ) {
    return query::query( m )
        .filter( [] ( auto &fn ) { return fn.getMetadata( "lart.abstract.roots" ); } )
        .flatten()
        .flatten()
        .map( query::refToPtr )
        .filter( [] ( auto i ) {
            if ( is_abstract( i->getType() ) )
                return true;
            return query::query( types_of( i->operands() ) )
                  .any( [] ( auto t ) { return is_abstract( t ); } );
        } )
        .freeze();
}

Values in_domain_args( CallInst *intr, std::map< Value*, Value* > &smap ) {
    return query::query( intr->arg_operands() )
        .map( [&] ( auto &arg ) { return arg.get(); } )
        .map( [&] ( auto arg ) { return smap.count( arg ) ? smap[ arg ] : arg; } )
        .freeze();
}

Domain get_domain( Type *type ) {
    auto st = cast< StructType >( type );
    auto name = st->getName().split('.').second.split('.').first;
    return DomainTable[ name.str() ];
}

void make_duals_in_domain( Instruction *a, Instruction *b, Domain dom ) {
    auto &ctx = a->getContext();
    auto tag = "lart.dual." + DomainTable[ dom ];
    a->setMetadata( tag, MDTuple::get( ctx, { ValueAsMetadata::get( b ) } ) );
    b->setMetadata( tag, MDTuple::get( ctx, { ValueAsMetadata::get( a ) } ) );
}

bool has_dual_in_domain( Instruction *inst, Domain dom ) {
    return inst->getMetadata( "lart.dual." + DomainTable[ dom ] );
}

Instruction* get_dual_in_domain( Instruction *inst, Domain dom ) {
    auto &dual = inst->getMetadata( "lart.dual." + DomainTable[ dom ] )->getOperand( 0 );
    auto md = cast< ValueAsMetadata >( dual.get() );
    return cast< Instruction >( md->getValue() );
}

void change_dual_in_domain_with( Instruction *a, Instruction *b, Domain dom ) {
    auto dual = get_dual_in_domain( a, dom );
    dual->replaceAllUsesWith( b );
    make_duals_in_domain( a, b, dom );
}

Function* dup_function( Module *m, Type *in, Type *out ) {
	auto fty = llvm::FunctionType::get( out, { in }, false );
    std::string name = "lart.placeholder.";
    if ( auto s = dyn_cast< StructType >( out ) )
        name += s->getName();
    else
        name += llvm_name( out );

    name += ".";

    if ( auto s = dyn_cast< StructType >( in ) )
        name += s->getName();
    else
        name += llvm_name( in );
   	return get_or_insert_function( m, fty, name );
}

Domain get_domain_of_intr( Instruction *inst ) {
    assert( inst->getType()->isStructTy() );
    if ( auto call = dyn_cast< CallInst >( inst ) ) {
        if ( call->getCalledFunction()->getName().startswith( "__lart_cast" ) ) {
            auto dual = get_dual( call );
            return MDValue( dual ).domain();
        }

        if ( call->getCalledFunction()->getName().startswith( "lart.placeholder" ) ) {
            ASSERT( isa<Argument >( get_dual( call ) ) );
            return get_domain( call->getType() );
        }

        // taint
        auto intr = cast< Function >( call->getOperand( 0 ) );
        if ( intr->getName().count( ".assume" ) )
            return MDValue( call ).domain();
        else
            return MDValue( get_dual( call ) ).domain();
    } else {
        auto phi = cast< PHINode >( inst );
        auto dual = get_dual( phi );
        return MDValue( dual ).domain();
    }
}

bool is_taint_of_type( Function *fn, const std::string &name ) {
    return fn->getName().count( "lart." ) &&
           fn->getName().count( name );
}

Function* taint_function( CallInst *taint ) {
    return cast< Function >( taint->getOperand( 0 ) );
}

Type* return_type_of_intr( CallInst *call ) {
    if ( !call->getType()->isStructTy() )
        return call->getType();
    auto dom = get_domain_of_intr( call );

    if ( is_taint_of_type( taint_function( call ), "assume" ) ) {
        auto def = cast< Instruction >( call->getOperand( 1 ) );
        return get_dual_in_domain( def, dom )->getType();
    }
    return get_dual_in_domain( call, dom )->getType();
}

BasicBlock* make_bb( Function *fn, std::string name ) {
	auto &ctx = fn->getContext();
	return BasicBlock::Create( ctx, name, fn );
}

size_t taint_args_size( CallInst *taint ) {
    return taint_function( taint )->arg_size() / 4;
}

struct BaseLifter {
    BaseLifter( DomainsHolder& domains, CallInst *taint )
        : taint( taint ), domains( domains )
    {}

    virtual void syntetize() = 0;

    Domain domain() const {
        auto name = cast< Function >( taint->getOperand( 0 ) )->getName()
                   .split('.').second.split('.').first.str();
        return DomainTable[ name ];
    }

    Function * function() const { return taint_function( taint ); }
protected:
    CallInst *taint;
    DomainsHolder &domains;
};

struct Lifter : BaseLifter {
    using BaseLifter::BaseLifter;

    struct Argument {
        Value *taint;
        Value *value;
    };

    struct ArgPair {
        Argument concrete;
        Argument abstract;
    };

    using ArgPairs = std::vector< ArgPair >;
    ArgPairs args() const {
        ArgPairs res;
        auto fn = function();
        auto it = fn->arg_begin();
        assert( fn->arg_size() % 4 == 0 );
        while ( it != fn->arg_end() ) {
            ArgPair pair;
            pair.concrete = { it, ++it }; // flag, value
            pair.abstract = { ++it, ++it };
            res.emplace_back( pair );
            it++;
        }
        return res;
    }

    size_t args_size() const { return taint_args_size( taint ); }

    struct ArgBlock {
        BasicBlock *entry;
        BasicBlock *exit;
        Value *value;
    };

    ArgBlock generate_arg_block( ArgPair arg, size_t idx ) {
        auto fn = function();
        std::string pref = "arg." + std::to_string( idx );
        // entry block
        auto ebb = make_bb( fn, pref + ".entry" );
        // lifting block
        auto lbb = make_bb( fn, pref + ".lifter" );
        IRBuilder<> irb( lbb );

        auto l = domains.get( domain() )->lift( arg.concrete.value );
        auto lift = cast< Instruction >( l );
        for ( auto &op : lift->operands() )
            if ( auto i = dyn_cast< Instruction >( op ) )
                if( !i->getParent() )
                    irb.Insert( i );
        irb.Insert( lift );

        // merging block
        auto mbb = make_bb( fn, pref + ".merge" );
        irb.SetInsertPoint( mbb );

        auto type = arg.abstract.value->getType();
        auto phi = irb.CreatePHI( type, 2 );
        phi->addIncoming( arg.abstract.value, ebb );
        phi->addIncoming( lift, lbb );

        irb.SetInsertPoint( ebb );
        irb.CreateCondBr( arg.concrete.taint, mbb, lbb );
        irb.SetInsertPoint( lbb );
        irb.CreateBr( mbb );
        return { ebb, mbb, phi };
    }
};

struct RepLifter : BaseLifter {
    using BaseLifter::BaseLifter;

    void syntetize() final {
        ASSERT( function()->empty() );
        IRBuilder<> irb( make_bb( function(), "entry" ) );

        auto begin = function()->arg_begin();
        Value *addr = std::next( begin, 3 );
        // TODO move to symbolic domain
        if ( !addr->getType()->getPointerElementType()->isIntegerTy( 8 ) )
            addr = irb.CreateBitCast( addr, Type::getInt8PtrTy( addr->getContext() ) );

        Values args = { addr };
        auto rep = domains.get( domain() )->process( taint, args );
        irb.Insert( cast< Instruction >( rep ) );
        irb.CreateRet( rep );
    }
};

struct UnrepLifter : BaseLifter {
    using BaseLifter::BaseLifter;

    void syntetize() final {
        ASSERT( function()->empty() );
      	IRBuilder<> irb( make_bb( function(), "entry" ) );

        auto begin = function()->arg_begin();
        auto formula = std::next( begin, 3 );
        auto addr = std::next( begin, 5 );

        auto bcst = irb.CreateBitCast( addr, Type::getInt8PtrTy( addr->getContext() ) );

        Values args = { formula, bcst };
        auto rep = domains.get( domain() )->process( taint, args );
        irb.Insert( cast< Instruction >( rep ) );

        // TODO insert to symbolic domain
        auto taint_zero = get_module( taint )->getFunction( "__vm_taint_i64" );

        Value *ret = irb.CreateCall( taint_zero );
        if ( !function()->getReturnType()->isIntegerTy( 64 ) )
            ret = irb.CreateTrunc( ret, function()->getReturnType() );
        irb.CreateRet( ret );
    }
};

struct ToBoolLifter : Lifter {
    using Lifter::Lifter;

    void syntetize() final {
        IRBuilder<> irb( make_bb( function(), "entry" ) );
        auto arg = *args().begin();

        Values args = { arg.abstract.value };
        auto tristate = domains.get( domain() )->process( taint, args );
        irb.Insert( cast< Instruction >( tristate ) );

        Values lower = { tristate };
        auto ret = domains.get( Domain::Tristate )->process( taint, lower );
        irb.Insert( cast< Instruction >( ret ) );
        irb.CreateRet( ret );
    }
};

struct AssumeLifter : Lifter {
    using Lifter::Lifter;

    void syntetize() final {
        auto args_begin = function()->arg_begin();
        auto concrete = std::next( args_begin, 1 );
        auto abstract = std::next( args_begin, 3 );
        auto assumed = std::next( args_begin, 5 );

        IRBuilder<> irb( make_bb( function(), "entry" ) );
        Values args = { concrete, abstract, assumed };
        auto call = domains.get( domain() )->process( taint, args );
        irb.Insert( cast< Instruction >( call ) );
        irb.CreateRet( call );
    }
};


struct UnaryLifter : Lifter {
    using Lifter::Lifter;

    void syntetize() final {
        IRBuilder<> irb( make_bb( function(), "entry" ) );
        Values vals = { args().begin()->abstract.value };
        auto rep = domains.get( domain() )->process( taint, vals );
        irb.Insert( cast< Instruction >( rep ) );
        irb.CreateRet( rep );
    }
};

struct BinaryLifter : Lifter {
    using Lifter::Lifter;

    void syntetize() final {
        auto fn = function();
        auto exit = make_bb( fn, "exit" );

        std::vector< ArgBlock > blocks;
        for ( const auto &arg_pair : args() ) {
            auto block = generate_arg_block( arg_pair, blocks.size() );

            if ( !blocks.empty() ) {
                auto prev = blocks.back();
                prev.exit->getTerminator()->setSuccessor( 0, block.entry );
            }

            exit->moveAfter( block.exit );
            IRBuilder<> irb( block.exit );
            irb.CreateBr( exit );

            blocks.emplace_back( block );
        }

        IRBuilder<> irb( exit );

        auto abstract_args = query::query( blocks )
            .map( [] ( auto & block ) { return block.value; } )
            .freeze();
        auto call = domains.get( domain() )->process( taint, abstract_args );
        irb.Insert( cast< Instruction >( call ) );
        irb.CreateRet( call );
    }
};


} // anonymous namespace

void DomainsHolder::add_domain( std::shared_ptr< Common > dom ) {
    domains[ dom->domain() ] = dom;
}

void Substitution::run( Module &m ) {
    domains.init( &m );
    auto insts = abstract_insts( m );
    for ( auto &i : insts ) {
        if ( auto call = dyn_cast< CallInst >( i ) ) {
                auto fn = call->getCalledFunction();
                if ( !fn->getName().startswith( "lart.placeholder" ) )
                    process( call );
        } else {
            auto phi = cast< PHINode >( i );
            auto dual = get_dual( phi );
            auto dom = MDValue( dual ).domain();
            auto dphi = cast< PHINode >( get_dual_in_domain( phi, dom ) );
            for ( size_t op = 0; op < phi->getNumIncomingValues(); ++op ) {
                auto in = phi->getIncomingValue( op );
                Value* val = UndefValue::get( dphi->getType() );
                if ( auto iin = dyn_cast< Instruction >( in ) )
                    if ( has_dual_in_domain( iin, dom ) )
                        val = get_dual_in_domain( iin, dom );
                dphi->addIncoming( val, phi->getIncomingBlock( op ) );
            }
        }
    }

    for ( auto i : insts ) {
        if ( i->getParent() ) {
            i->replaceAllUsesWith( UndefValue::get( i->getType() ) );
            i->eraseFromParent();
        }
    }
}

Type * Substitution::abstract_type( Instruction *i ) {
    auto dom = MDValue( i ).domain();
    return domains.type( i->getType(), dom );
}

inline bool is_cast_call( CallInst *call ) {
    return call->getCalledFunction()->getName().startswith( "__lart_cast" );
}

inline bool is_taint_call( CallInst *call ) {
    return call->getCalledFunction()->getName().startswith( "__vm_test_taint" );
}

void Substitution::process( CallInst *call ) {
    if ( is_cast_call( call ) )
        process_cast( call );
    else if ( is_taint_call( call ) )
        process_taint( call );
    else
        UNREACHABLE( "Unknown abstract instruction" );
}

void Substitution::process_cast( CallInst *call ) {
    IRBuilder<> irb( call );
    // TODO move to sym domain
    if ( call->getType()->isIntegerTy() ) { // to i64 cast
        auto op = call->getOperand( 0 );
        if ( auto inst = dyn_cast< Instruction >( op ) ) {
            auto dom = get_domain( inst->getType() );
            auto dual = get_dual_in_domain( inst, dom );
            auto cst = cast< Instruction >( irb.CreatePtrToInt( dual, call->getType() ) );
            call->replaceAllUsesWith( cst );
        } else {
            ASSERT( isa< UndefValue >( op ) );
            call->replaceAllUsesWith( UndefValue::get( call->getType() ) );
        }
    } else { // to abstract cast
        auto dom = MDValue( get_dual( call ) ).domain();
        auto dual = get_dual_in_domain( call, dom ); // placeholder
        auto rty = dual->getType();
        auto cst = cast< Instruction >( irb.CreateIntToPtr( call->getOperand( 0 ), rty ) );
        change_dual_in_domain_with( call, cst, dom );
    }
}

void Substitution::process_taint( CallInst *call ) {
    IRBuilder<> irb( call );

    auto rty = return_type_of_intr( call );

    auto intr = cast< Function >( call->getOperand( 0 ) );
    auto name = "lart." + intr->getName().drop_front( std::string( "lart.gen." ).size() ).str();

    auto apply = [] ( auto values, auto fn ) {
        return query::query( values ).map( fn ).freeze();
    };

    auto to_domain = [&] ( auto type ) {
        return domains.type( type, get_domain( type ) );
    };

    auto types_to_domain = [&] ( auto types ) {
        return apply( types, [&] ( auto ty ) {
            return ty->isStructTy() ? to_domain( ty ) : ty;
        } );
    };

    auto arg_types = apply( intr->args(), [] ( auto &a ) { return a.getType(); } );
    auto fty = FunctionType::get( rty, types_to_domain( arg_types ), false );
    auto intrinsic = get_or_insert_function( get_module( call ), fty, name );

    auto taint_types = apply( call->arg_operands(), [] ( const auto &o ) {
        return o->getType();
    } );

    taint_types = types_to_domain( taint_types );
    taint_types[ 0 ] = intrinsic->getType();

    auto taint_fty = FunctionType::get( rty, taint_types, false );
    auto m = get_module( call );
    auto tname = "__vm_test_taint." + name;
    auto taint = get_or_insert_function( m, taint_fty, tname );

    Values args = { intrinsic };
    for ( size_t i = 1; i < taint_fty->getNumParams(); ++i ) {
        auto op = call->getArgOperand( i );
        if ( isa< UndefValue >( op ) ) {
            args.push_back( UndefValue::get( taint_fty->getParamType( i ) ) );
        } else if ( op->getType()->isStructTy() ) {
            auto inst = cast< Instruction >( op );
            auto dom = get_domain_of_intr( inst );
            args.push_back( get_dual_in_domain( inst, dom ) );
        } else if ( op->getType()->isIntegerTy() ) {
            args.push_back( op );
        } else if ( op->getType()->isPointerTy() ) {
            args.push_back( op );
        } else {
            UNREACHABLE( "Unknown argument type" );
        }
    }

    auto subs = irb.CreateCall( taint, args );

    if ( call->getType()->isStructTy() ) {
        auto dom = get_domain_of_intr( call );
        if ( has_dual_in_domain( call, dom ) )
            change_dual_in_domain_with( call, subs, dom );
        else
            make_duals_in_domain( call, subs, dom );
    }

    if ( call->getType()->isIntegerTy() )
        call->replaceAllUsesWith( subs );
}

struct ArgWrapper {
    ArgWrapper( Function *fn, DomainsHolder &doms )
        : fn( fn ), domains( doms )
    {}

    void unwrap() {
        IRBuilder<> irb( fn->getEntryBlock().begin() );

        auto ty = type();

        auto unfn = unstash_function( fn->getParent() );
        auto unstash = irb.CreateCall( unfn );
        auto wrapped = irb.CreateIntToPtr( unstash, ty->getPointerTo() );

        auto loaded = irb.CreateLoad( wrapped );

        for ( unsigned int i = 0; i < ty->getNumElements(); ++i ) {
            auto arg = std::next( fn->arg_begin(), i );
            if ( arg->getType()->isIntegerTy() ) {
                auto ev = cast< Instruction >( irb.CreateExtractValue( loaded, { i } ) );
                auto ph = cast< Instruction >( placeholder( arg ) );
                make_duals_in_domain( ph, ev, get_domain( ph->getType() ) );
            }
        }
    }

    void wrap( CallInst *call ) {
        IRBuilder<> irb( call );

        auto ty = type();
        auto wrapped = irb.CreateAlloca( ty );

        auto args = query::query( call->arg_operands() )
            .map( [] ( auto &op ) -> Value* {
                auto arg = dyn_cast< Instruction >( op );
                if ( arg && has_dual( arg ) ) {
                    auto dom = MDValue( arg ).domain();
                    return get_dual_in_domain( cast< Instruction >( get_dual( arg ) ), dom );
                } else {
                    return nullptr;
                }
            } )
            .freeze();

        Value *val = UndefValue::get( ty );
        for ( unsigned int i = 0; i < ty->getNumElements(); ++i ) {
            if ( args[ i ] ) {
                val = irb.CreateInsertValue( val, args[ i ], { i } );
            } else {
                auto undef = UndefValue::get( ty->getElementType( i ) );
                val = irb.CreateInsertValue( val, undef, { i } );
            }
        }

        irb.CreateStore( val, wrapped );

        auto i64 = IntegerType::get( call->getContext(), 64 );
        auto i = irb.CreatePtrToInt( wrapped, i64 );
        auto sfn = stash_function( get_module( call ) );
        irb.CreateCall( sfn, { i } );
    }

    StructType* type() {
        auto tys = types();
        auto &ctx = tys.at( 0 )->getContext();
        return StructType::get( ctx, tys );
    }
private:
    Values placeholders() const {
        return query::query( fn->args() )
            .map( [&] ( auto &arg ) {
                return arg.getType()->isIntegerTy() ? placeholder( &arg ) : &arg;
            } )
            .freeze();
    }

    Types types() const {
        return query::query( placeholders() )
            .map( [&] ( auto &val ) {
                if ( isa< Argument >( val ) ) {
                    return val->getType();
                } else {
                    auto inst = cast< CallInst >( val );
                    auto dom = MDValue( inst ).domain();
                    return domains.type( inst->getOperand( 0 )->getType(), dom );
                }
            } )
            .freeze();
    }

    Function *fn;
    DomainsHolder &domains;
};

Functions abstract_functions( Module &m ) {
    return query::query( m )
        .map( query::refToPtr )
        .filter( [] ( auto &fn ) {
            return fn->getMetadata( "lart.abstract.roots" );
        } )
        .filter( [] ( auto fn ) {
            return fn->getFunctionType()->getNumParams() != 0;
        } )
        .freeze();
}

void SubstitutionDuplicator::run( Module &m ) {
    domains.init( &m );

    auto insts = query::query( m )
        .filter( [] ( auto &fn ) { return fn.getMetadata( "lart.abstract.roots" ); } )
        .flatten()
        .flatten()
        .map( query::refToPtr )
        .filter( [] ( auto i ) { return is_abstract( i->getType() ); } )
        .filter( [] ( auto i ) { return has_dual( i ) && !isa< Argument >( get_dual( i ) ); } );

    for ( auto i : insts )
        process( i );

    for ( auto fn : abstract_functions( m ) )
        ArgWrapper( fn, domains ).unwrap();

    for ( auto fn : abstract_functions( m ) )
        for ( auto u : fn->users() )
            if ( auto call = dyn_cast< CallInst >( u ) )
                ArgWrapper( call->getCalledFunction(), domains ).wrap( call );
}

void SubstitutionDuplicator::process( Instruction *i ) {
    auto m = get_module( i );
    auto ty = i->getType();
    auto dom = get_domain( ty );
    auto out = domains.type( ty, dom );
	auto fn = dup_function( m, ty, out );

	IRBuilder<> irb( i );

    auto dup = [&] () -> Instruction* {
        if ( auto phi = dyn_cast< PHINode >( i ) )
            return irb.CreatePHI( out, phi->getNumIncomingValues() );
        return irb.CreateCall( fn, { i } );
    } ();

	dup->removeFromParent();
	dup->insertAfter( i );

    make_duals_in_domain( i, dup, dom );
}

void UnrepStores::run( Module &m ) {
    domains.init( &m );

    auto stores = query::query( m )
        .filter( [] ( auto &fn ) { return fn.getMetadata( "lart.abstract.roots" ); } )
        .flatten().flatten()
        .map( query::refToPtr )
        .filter( query::llvmdyncast< StoreInst > )
        .filter( query::notnull )
        .filter( [] ( auto store ) { return store->getMetadata( "lart.domains" ); } )
        .filter( [] ( auto store ) { return store->getOperand( 0 )->getType()->isIntegerTy(); } )
        .filter( [] ( auto store ) { return !isa< Constant >( store->getOperand( 0 ) ); } )
        .freeze();

    for ( auto s : stores )
        process( cast< StoreInst >( s ) );

    for ( auto s : stores )
	s->eraseFromParent();
}

void UnrepStores::process( StoreInst *store ) {
    auto dom = MDValue( store ).domain();
    auto m = get_module( store );

    auto val = store->getValueOperand();
    auto ptr = store->getPointerOperand();

    auto ty = store->getValueOperand()->getType();
    auto aty = domains.type( ty, dom );

    auto unrep = [&] () {
        auto flag = Type::getInt1Ty( store->getContext() );
        auto fty = FunctionType::get( ty, { flag, ty, flag, aty, flag, ptr->getType() }, false );
        auto name = "lart." + DomainTable[ dom ] + ".unrep." + llvm_name( ty );
        return get_or_insert_function( m, fty, name );
    } ();

    auto abstract = [&] () -> Value* {
        if ( auto inst = dyn_cast< Instruction >( val ) ) {
            if ( has_dual( inst ) ) {
                auto dual = cast< Instruction >( get_dual( inst ) );
                return get_dual_in_domain( dual, dom );
            } else {
                return UndefValue::get( aty );
            }
        }

        if ( auto arg = dyn_cast< Argument >( val ) ) {
            auto ph = cast< Instruction >( placeholder( arg ) );
            return get_dual_in_domain( ph, dom );
        }

        val->dump();
        UNREACHABLE( "Can not obtaint dual in domain." );
    } ();

    Values args = { unrep, val, val, abstract, ptr };

    auto fty = FunctionType::get( ty, types_of( args ), false );
    auto tname = "__vm_test_taint." + unrep->getName().str();
    auto tfn = get_or_insert_function( m, fty, tname );

    IRBuilder<> irb( store );
    auto taint = irb.CreateCall( tfn, args );
    irb.CreateStore( taint, ptr );
}


void Synthesize::run( Module &m ) {
    domains.init( &m );
    for ( auto t : taints( m ) )
        process( cast< CallInst >( t ) );
}

void Synthesize::process( CallInst *taint ) {
    auto fn = cast< Function >( taint->getOperand( 0 ) );
    if ( fn->empty() ) {
        if ( is_taint_of_type( fn, ".rep" ) ) {
            RepLifter( domains, taint ).syntetize();
	} else if ( is_taint_of_type( fn, ".unrep" ) ) {
            UnrepLifter( domains, taint ).syntetize();
        } else if ( is_taint_of_type( fn, ".to_i1" ) ) {
            ToBoolLifter( domains, taint ).syntetize();
        } else if ( is_taint_of_type( fn, ".assume" ) ) {
            AssumeLifter( domains, taint ).syntetize();
        } else if ( taint_args_size( taint ) == 1 ) {
            UnaryLifter( domains, taint ).syntetize();
        } else if ( taint_args_size( taint ) == 2 ) {
            BinaryLifter( domains, taint ).syntetize();
        } else {
            taint->dump();
            UNREACHABLE( "Unknown taint function." );
        }
    }
}

} // namespace abstract
} // namespace lart
