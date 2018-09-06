#include <lart/abstract/substitution.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>
#include <lart/support/query.h>
#include <lart/support/util.h>
#include <lart/abstract/stash.h>
#include <lart/abstract/metadata.h>

#include <iostream>

#include <brick-llvm>

namespace lart {
namespace abstract {

using namespace llvm;

namespace {

bool has_dual_in_domain( Instruction *inst, Domain dom ) {
    return inst->getMetadata( "lart.dual." + dom.name() );
}

Instruction* get_dual_in_domain( Instruction *inst, Domain dom ) {
    auto &dual = inst->getMetadata( "lart.dual." + dom.name() )->getOperand( 0 );
    auto md = cast< ValueAsMetadata >( dual.get() );
    return cast< Instruction >( md->getValue() );
}

Domain get_domain_of_intr( Instruction *inst ) {
    assert( inst->getType()->isStructTy() );
    if ( auto call = dyn_cast< CallInst >( inst ) ) {
        auto fn = get_called_function( call );
        if ( fn->getName().startswith( "__lart_cast" ) ) {
            auto dual = get_dual( call );
            return MDValue( dual ).domain();
        }

        if ( fn->getName().startswith( "lart.placeholder" ) ) {
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

Type* base_type( Type* abstract ) {
    return abstract->getContainedType( 0 );
}

std::string abstract_type_name( Type *ty ) {
    return cast< StructType >( ty )->getName().str();
}

Types args_with_taints( Value* val, const Values &args ) {
    auto i1 = Type::getInt1Ty( val->getContext() );

    Types res;
    for ( auto arg : args ) {
        res.push_back( i1 );
        res.push_back( arg->getType() );
    }

    return res;
}

bool is_taintable( Value *v ) {
    return is_one_of< BinaryOperator, CmpInst, TruncInst, SExtInst, ZExtInst >( v );
}

ConstantInt* bitwidth( Value *v ) {
    auto &ctx = v->getContext();
    auto ty = v->getType();
    auto bw = cast< IntegerType >( ty )->getBitWidth();
    return ConstantInt::get( IntegerType::get( ctx, 32 ), bw );
}

namespace placeholder {

bool is_placeholder_of_name( Instruction *inst, std::string name ) {
    auto fn = get_called_function( cast< CallInst >( inst ) );
    return fn->getName().count( name );
}

bool is_stash( Instruction *inst ) {
    return is_placeholder_of_name( inst, ".stash." );
}

bool is_unstash( Instruction *inst ) {
    return is_placeholder_of_name( inst, ".unstash." );
}

bool is_to_i1( Instruction *inst ) {
    return is_placeholder_of_name( inst, ".to_i1." );
}

bool is_assume( Instruction *inst ) {
    return is_placeholder_of_name( inst, ".assume" );
}

Domain domain( Instruction *inst ) {
    auto ty = inst->getType();
    if ( is_assume( inst ) )
        return get_domain( cast< Instruction >( inst->getOperand( 0 ) )->getOperand( 0 )->getType() );
    if ( ty->isVoidTy() || !inst->getType()->isStructTy() )
        return get_domain( inst->getOperand( 0 )->getType() );
    else
        return get_domain( ty );
}

Type* return_type( Instruction *inst ) {
    auto ty = inst->getType();
    if ( ty->isVoidTy() )
        return ty;

    auto dom = get_domain( ty );
    return domain_metadata( *inst->getModule(), dom ).base_type();
}

Values arguments( Instruction *inst ) {
    if ( is_stash( inst ) ) {
        auto dom = domain( inst );
        auto meta = domain_metadata( *inst->getModule(), dom );
        if ( auto uv = dyn_cast< UndefValue >( inst->getOperand( 0 ) ) )
            return { meta.default_value() };
        auto abstract_placeholder = cast< Instruction >( inst->getOperand( 0 ) );
        return { get_placeholder_in_domain( abstract_placeholder->getOperand( 0 ), dom ) };
    } else if ( is_to_i1( inst ) ) {
        auto abstract_placeholder = cast< Instruction >( inst->getOperand( 0 ) );
        auto dom = domain( abstract_placeholder );
        return { get_placeholder_in_domain( abstract_placeholder->getOperand( 0 ), dom ) };
    } else {
        auto call = cast< CallInst >( inst );
        return { call->arg_operands().begin(), call->arg_operands().end() };
    }
}

std::string name( Instruction *inst, Value *arg, Domain dom ) {
    auto name = "lart." + dom.name() + ".placeholder.";
    if ( is_stash( inst ) )
        name += "stash.";
    else if ( is_unstash( inst ) )
        name += "unstash.";
    else if ( is_to_i1( inst ) )
        name += "to_i1.";
    else if ( is_assume( inst ) ) {
        name += "assume";
    }

    if ( !is_assume( inst ) ) {
        auto ty = inst->getType();
        name += ty->isVoidTy() || !ty->isStructTy()
              ? llvm_name( base_type( arg->getType() ) )
              : llvm_name( base_type( ty ) );
    }
    return name;
}

Function* get( Instruction *inst ) {
    auto rty = inst->getType()->isStructTy() ? return_type( inst ) : inst->getType();
    auto args = arguments( inst );

    auto dom = domain( inst );
    auto phname = name( inst, inst->getOperand( 0 ), dom );
    auto fty = FunctionType::get( rty, types_of( args ), false );
   	return get_or_insert_function( get_module( inst ), fty, phname );
}

} // namespace placeholder

namespace lifter {
    inline std::string prefix( Instruction *inst, Domain dom ) {
        return dom.name() + "." + inst->getOpcodeName();
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

    std::string name( Instruction *inst, Domain dom ) {
        auto pref = prefix( inst, dom );

        if ( auto icmp = dyn_cast< ICmpInst >( inst ) )
            return pref + "_" + predicate.at( icmp->getPredicate() )
                        + "." + llvm_name( icmp->getOperand( 0 )->getType() );

        if ( isa< BinaryOperator >( inst ) )
            return pref + "." + llvm_name( inst->getType() );

        if ( auto ci = dyn_cast< CastInst >( inst ) )
            return pref + "." + llvm_name( ci->getSrcTy() )
                        + "." + llvm_name( ci->getDestTy() );

        UNREACHABLE( "Unhandled lifter." );
    }

} // namespace taint

struct LifterBuilder {

    LifterBuilder( Domain dom )
        : domain( dom )
    {}

    std::string intr_name( CallInst *call ) {
        auto intr = call->getCalledFunction()->getName();
        size_t pref = std::string( "__vm_test_taint." + domain.name() + "." ).length();
        auto tag = intr.substr( pref ).split( '.' ).first;
        return "__" + domain.name() + "_" + tag.str();
    }

    Value* process_thaw( CallInst *call, Values &args ) {
        IRBuilder<> irb( call->getContext() );
        auto name = "__" + domain.name() + "_thaw";
        auto thaw = get_module( call )->getFunction( name );
        return irb.CreateCall( thaw, args );
    }

    Value* process_freeze( CallInst *call, Values &args  ) {
        IRBuilder<> irb( call->getContext() );
        auto name = "__" + domain.name() + "_freeze";
        auto freeze = get_module( call )->getFunction( name );
        return irb.CreateCall( freeze, args );
    }

    Value* process_assume( CallInst *call, Values &args ) {
        IRBuilder<> irb( call->getContext() );
        auto fn = get_module( call )->getFunction( "__" + domain.name() + "_assume" );
        return irb.CreateCall( fn, { args[ 1 ], args[ 1 ], args[ 2 ] } );
    }

    Value* process_tobool( CallInst *call, Values &args ) {
        IRBuilder<> irb( call->getContext() );
        if ( domain == Domain::Tristate() ) {
            auto fn = get_module( call )->getFunction( "__tristate_lower" );
            return irb.CreateCall( fn, args );
        } else {
            IRBuilder<> irb( call->getContext() );
            auto name = "__" + domain.name() + "_bool_to_tristate";
            auto fn = get_module( call )->getFunction( name );
            return irb.CreateCall( fn, args );
        }
    }

    ConstantInt* cast_bitwidth( Function *fn ) {
        auto &ctx = fn->getContext();
        auto bw = fn->getName().rsplit('.').second.drop_front().str();
        return ConstantInt::get( IntegerType::get( ctx, 32 ), std::stoi( bw ) );
    }

    Value* process_cast( CallInst *call, Values &args ) {
        IRBuilder<> irb( call->getContext() );
        auto name = intr_name( call );
        auto op = cast< Function >( call->getOperand( 0 ) );

        auto fn = get_module( call )->getFunction( name );
        return irb.CreateCall( fn, { args[ 0 ], cast_bitwidth( op ) } );
    }

    Value* process_op( CallInst *call, Values &args ) {
        IRBuilder<> irb( call->getContext() );
        auto name = intr_name( call );
        auto fn = get_module( call )->getFunction( name );
        return irb.CreateCall( fn, args );
    }

    Value * process( Value * intr, Values & args ) {
        auto call = cast< CallInst >( intr );

        if ( is_thaw( call ) )
            return process_thaw( call, args );
        if ( is_freeze( call ) )
            return process_freeze( call, args );
        if ( is_cast( call ) )
            return process_cast( call, args );
        if ( is_assume( call ) )
            return process_assume( call, args );
        if ( is_tobool( call ) )
            return process_tobool( call, args );
        return process_op( call, args );
    }
private:
    Domain domain;
};

struct BaseLifter {
    BaseLifter( CallInst *taint )
        : taint( taint )
    {}

    virtual void syntetize() = 0;

    Domain domain() const {
        auto name = cast< Function >( taint->getOperand( 0 ) )->getName()
                   .split('.').second.split('.').first.str();
        return Domain( name );
    }

    Function * function() const { return taint_function( taint ); }
protected:
    CallInst *taint;
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
            pair.concrete = { &*it, &*++it }; // flag, value
            pair.abstract = { &*++it, &*++it };
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

    CallInst * lift( Value * val ) {
        auto &ctx = val->getContext();
        IRBuilder<> irb( ctx );
        auto fn = get_module( val )->getFunction( "__" + domain().name()  + "_lift" );
        auto val64bit = irb.CreateSExt( val, IntegerType::get( ctx, 64 ) );
        auto argc = ConstantInt::get( IntegerType::get( ctx, 32 ), 1 );
        return irb.CreateCall( fn, { bitwidth( val ), argc, val64bit } );
    }

    ArgBlock generate_arg_block( ArgPair arg, size_t idx ) {
        auto fn = function();
        std::string pref = "arg." + std::to_string( idx );
        // entry block
        auto ebb = make_bb( fn, pref + ".entry" );
        // lifting block
        auto lbb = make_bb( fn, pref + ".lifter" );
        IRBuilder<> irb( lbb );

        auto lifted = lift( arg.concrete.value );
        for ( auto &op : lifted->operands() )
            if ( auto i = dyn_cast< Instruction >( op ) )
                if( !i->getParent() )
                    irb.Insert( i );
        irb.Insert( lifted );

        // merging block
        auto mbb = make_bb( fn, pref + ".merge" );
        irb.SetInsertPoint( mbb );

        auto type = arg.abstract.value->getType();
        auto phi = irb.CreatePHI( type, 2 );
        phi->addIncoming( arg.abstract.value, ebb );
        phi->addIncoming( lifted, lbb );

        irb.SetInsertPoint( ebb );
        irb.CreateCondBr( arg.concrete.taint, mbb, lbb );
        irb.SetInsertPoint( lbb );
        irb.CreateBr( mbb );
        return { ebb, mbb, phi };
    }
};

struct ThawLifter : BaseLifter {
    using BaseLifter::BaseLifter;

    void syntetize() final {
        ASSERT( function()->empty() );
        IRBuilder<> irb( make_bb( function(), "entry" ) );

        auto &ctx = taint->getContext();

        auto begin = function()->arg_begin();
        Value *addr = &*std::next( begin, 3 );
        // TODO move to symbolic domain
        auto ty = cast< IntegerType >( addr->getType()->getPointerElementType() );
        if ( !ty->isIntegerTy( 8 ) )
            addr = irb.CreateBitCast( addr, Type::getInt8PtrTy( ctx ) );

        auto i32 = Type::getInt32Ty( ctx );
        auto bitwidth = ConstantInt::get( i32, ty->getBitWidth() );

        Values args = { addr, bitwidth };
        auto thaw = LifterBuilder( domain() ).process( taint, args );
        irb.Insert( cast< Instruction >( thaw ) );
        irb.CreateRet( thaw );
    }
};

struct FreezeLifter : BaseLifter {
    using BaseLifter::BaseLifter;

    void syntetize() final {
        ASSERT( function()->empty() );
      	IRBuilder<> irb( make_bb( function(), "entry" ) );

        auto begin = function()->arg_begin();
        auto formula = std::next( begin, 3 );
        auto addr = std::next( begin, 5 );

        auto bcst = irb.CreateBitCast( &*addr, Type::getInt8PtrTy( addr->getContext() ) );

        Values args = { &*formula, &*bcst };
        auto freeze = LifterBuilder( domain() ).process( taint, args );
        irb.Insert( cast< Instruction >( freeze ) );

        // TODO insert to symbolic domain
        auto taint_zero = get_module( taint )->getFunction( "__rst_taint_i64" );

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
        auto tristate = LifterBuilder( domain() ).process( taint, args );
        irb.Insert( cast< Instruction >( tristate ) );

        Values lower = { tristate };
        auto ret = LifterBuilder( Domain::Tristate() ).process( taint, lower );
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
        Values args = { &*concrete, &*abstract, &*assumed };
        auto call = LifterBuilder( domain() ).process( taint, args );
        irb.Insert( cast< Instruction >( call ) );

        // TODO fix return value for other domains
        irb.CreateRet( UndefValue::get( Type::getInt1Ty( call->getContext() ) ) );
    }
};


struct UnaryLifter : Lifter {
    using Lifter::Lifter;

    void syntetize() final {
        IRBuilder<> irb( make_bb( function(), "entry" ) );
        Values vals = { args().begin()->abstract.value };
        auto rep = LifterBuilder( domain() ).process( taint, vals );
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
        auto call = LifterBuilder( domain() ).process( taint, abstract_args );
        irb.Insert( cast< Instruction >( call ) );
        irb.CreateRet( call );
    }
};

struct GetLifter : Lifter {
    using Lifter::Lifter;

    void syntetize() final {
        auto entry = make_bb( function(), "entry" );

        IRBuilder<> irb( entry );

        auto abstract = std::next( function()->arg_begin(), 3 );
        irb.CreateRet( &*abstract );
    }
};

} // anonymous namespace

// --------------------------- Duplication ---------------------------

void InDomainDuplicate::run( Module &m ) {
    auto phs = placeholders( m );

    auto assumes = query::query( phs ).filter( placeholder::is_assume );
    std::for_each( assumes.begin(), assumes.end(), [&] ( auto ass ) { process( ass ); } );

    auto rest = query::query( phs )
        .filter( query::negate( placeholder::is_assume ) )
        .filter( query::negate( placeholder::is_stash ) );
    std::for_each( rest.begin(), rest.end(), [&] ( auto ph ) { process( ph ); } );

    // process stash placeholders separatelly in this order!
    auto stashes = query::query( phs ).filter( placeholder::is_stash );
    std::for_each( stashes.begin(), stashes.end(), [&] ( auto st ) { process( st ); } );

    // clean abstract placeholders
    for ( auto ph : phs ) {
        ph->replaceAllUsesWith( UndefValue::get( ph->getType() ) );
        ph->eraseFromParent();
    }
}

void InDomainDuplicate::process( Instruction *inst ) {
    IRBuilder<> irb( inst );

    auto ph = placeholder::get( inst );
    auto args = placeholder::arguments( inst );
    auto call = irb.CreateCall( ph, args );

    if ( placeholder::is_to_i1( call ) )
        inst->replaceAllUsesWith( call );
}

// ---------------------------- Tainting ---------------------------

Function* get_taint_fn( Module *m, Type *ret, Types args, std::string name ) {
    auto taint = "__vm_test_taint." + name;
    auto fty = FunctionType::get( ret, args, false );
    return get_or_insert_function( m, fty, taint );
}

template< typename Derived >
struct TaintBase : CRTP< Derived > {
    TaintBase( Instruction *placeholder )
        : placeholder( placeholder )
    {}

    Instruction* generate() {
        IRBuilder<> irb( placeholder );

        Values args;
        args.emplace_back( function() );
        args.emplace_back( default_value() );

        auto sargs = this->self().arguments();
        args.insert( args.end(), sargs.begin(), sargs.end() );

        auto rty = default_value()->getType();

        auto m = get_module( placeholder );
        auto taint = get_taint_fn( m, rty, types_of( args ), this->self().name() );
        return cast< Instruction >( irb.CreateCall( taint, args ) );
    }

    Type* return_type() const {
        return placeholder->getType();
    }

    Value* default_value() const {
        if ( placeholder::is_to_i1( placeholder ) )
            return concrete();
        if ( placeholder::is_assume( placeholder ) )
            return concrete();
        auto meta = domain_metadata( *get_module( placeholder ), domain() );
        return meta.default_value();
    }

    Value* concrete() const {
        if ( placeholder::is_to_i1( placeholder ) )
            return cast< Instruction >( placeholder->getOperand( 0 ) )->getOperand( 0 );
        if ( placeholder::is_assume( placeholder ) )
            return cast< Instruction >(
                       cast< Instruction >(
                           placeholder->getOperand( 0 )
                       )->getOperand( 0 )
                   )->getOperand( 0 );
        return placeholder->getOperand( 0 );
    }

    Domain domain() const {
        return get_domain( concrete() );
    }

    Function *function() {
        auto m = get_module( placeholder );
        auto args = args_with_taints( placeholder, this->self().arguments() );
        auto fty = FunctionType::get( return_type(), args, false );
        auto fname = "lart." + this->self().name();
        return get_or_insert_function( m, fty, fname );
    }

protected:
    Instruction *placeholder;
};

struct Taint : TaintBase< Taint > {
    using TaintBase< Taint >::TaintBase;

    Values arguments() {
        auto dom = domain();
        auto inst = cast< Instruction >( concrete() );

        Values res;
        for ( auto & op : inst->operands() ) {
            res.push_back( op );
            if ( has_placeholder_in_domain( op, dom ) ) {
                res.push_back( get_placeholder_in_domain( op, dom ) );
            } else {
                auto meta = domain_metadata( *get_module( placeholder ), domain() );
                res.push_back( meta.default_value() );
            }
        }

        return res;
    }

    std::string name() const {
        auto inst = cast< Instruction >( concrete() );
        return lifter::name( inst, domain() );
    }
};

struct ThawTaint : TaintBase< ThawTaint > {
    using TaintBase< ThawTaint >::TaintBase;

    LoadInst* loaded() const {
        return cast< LoadInst >( placeholder->getOperand( 0 ) );
    }

    Values arguments() {
        auto load = loaded();
        return { load, load->getPointerOperand() };
    }

    std::string name() const {
        return domain().name() + ".thaw." + llvm_name( loaded()->getType() );
    }
};

struct GetTaint : TaintBase< GetTaint > {
    using TaintBase< GetTaint >::TaintBase;

    Value* stash() {
        ASSERT( placeholder->hasNUses( 1 ) );
        return placeholder->user_back();
    }

    Values arguments() {
        return { concrete(), placeholder };
    }

    std::string name() const {
        return domain().name() + ".get." + llvm_name( concrete()->getType() );
    }
};

struct ToBoolTaint : TaintBase< ToBoolTaint > {
    using TaintBase< ToBoolTaint >::TaintBase;

    Values arguments() {
        auto abstract = cast< Instruction >( placeholder->getOperand( 0 ) );
        auto concrete = abstract->getOperand( 0 );
        return { concrete, abstract };
    }

    std::string name() const {
        return domain().name() + ".to_i1";
    }
};

struct AssumeTaint : TaintBase< AssumeTaint > {
    using TaintBase< AssumeTaint >::TaintBase;

    Values arguments() {
        auto cond = cast< Instruction >( placeholder->getOperand( 0 ) )->getOperand( 0 );
        auto concrete = cast< Instruction >( cond )->getOperand( 0 );
        auto assume = placeholder->getOperand( 1 );
        return { concrete, cond, assume };
    }

    std::string name() const {
        return domain().name() + ".assume";
    }
};

namespace bundle {

bool is_base_type( Value* val ) {
    return val->getType()->isIntegerTy(); // TODO move to domain
}

Values argument_placeholders( CallInst *call ) {
    auto fn = get_called_function( call );
    FunctionMetadata fmd{ fn };

    return query::query( fn->args() )
        .map( query::refToPtr )
        .filter( [&] ( auto arg ) {
            return !is_concrete( arg );
        } )
        .filter( is_base_type )
        .map( [&] ( auto arg ) -> Value* {
            auto idx = arg->getArgNo();
            auto dom = fmd.get_arg_domain( idx );
            return get_placeholder_in_domain( arg, dom );
        } ).freeze();
}


StructType* packed_type( CallInst *call ) {
    auto phs = argument_placeholders( call );
    return StructType::get( call->getContext(), types_of( phs ) );
}

void stash_arguments( CallInst *call ) {
    auto fn = get_called_function( call );
    if ( fn->getMetadata( abstract_tag ) )
        return; // skip internal lart functions

    auto pack_ty = packed_type( call );

    IRBuilder<> irb( call );
    auto pack = irb.CreateAlloca( pack_ty );

    FunctionMetadata fmd{ fn };

    Value *val = UndefValue::get( pack_ty );

    unsigned pack_pos = 0;

    for ( auto &arg : fn->args() ) {
        auto idx = arg.getArgNo();
        auto op = call->getArgOperand( idx );
        auto dom = fmd.get_arg_domain( idx );

        Value *abstract_arg = nullptr;
        if ( is_concrete( dom ) || !is_base_type( op ) ) { // TODO check kind instead of base
            continue; // skip concrete arguments
        } else if ( is_concrete( op ) && !is_concrete( dom ) ) {
            auto meta = domain_metadata( *get_module( call ), dom );
            abstract_arg = meta.default_value();
        } else {
            auto ph = get_placeholder_in_domain( op, dom );
            abstract_arg = GetTaint( ph ).generate();
        }
        val = irb.CreateInsertValue( val, abstract_arg, { pack_pos++ } );
    }

    irb.CreateStore( val, pack );

    auto i64 = IntegerType::get( call->getContext(), 64 );
    auto i = irb.CreatePtrToInt( pack, i64 );
    auto sfn = stash_function( get_module( call ) );
    irb.CreateCall( sfn, { i } );
}

Values unstash_arguments( CallInst *call ) {
    auto fn = get_called_function( call );
    if ( fn->getMetadata( abstract_tag ) )
        return {}; // skip internal lart functions

    IRBuilder<> irb( &*fn->getEntryBlock().begin() );

    auto pack_ty = packed_type( call );

    auto unfn = unstash_function( get_module( call ) );
    auto unstash = irb.CreateCall( unfn );
    auto packed = irb.CreateIntToPtr( unstash, pack_ty->getPointerTo() );
    auto loaded = irb.CreateLoad( packed );

    Values unpacked;
    for ( unsigned int i = 0; i < pack_ty->getNumElements(); ++i )
        unpacked.push_back( irb.CreateExtractValue( loaded, { i } ) );

    return unpacked;
}

void stash_return_value( CallInst *call ) {
    auto fn = get_called_function( call );
    if ( fn->getMetadata( abstract_tag ) )
        return; // skip internal lart functions

    auto rets = query::query( *fn ).flatten()
        .map( query::refToPtr )
        .filter( query::llvmdyncast< ReturnInst > )
        .freeze();

    ASSERT( rets.size() == 1 && "No single return instruction found." );
    auto ret = cast< ReturnInst >( rets[ 0 ] );
    auto val = ret->getReturnValue();

    IRBuilder<> irb( ret );
    auto sfn = stash_function( get_module( call ) );

    auto dom = MDValue( call ).domain();
    auto i64 = IntegerType::get( call->getContext(), 64 );
    ASSERT( has_placeholder( call, "lart." + dom.name() + ".placeholder.unstash" ) );
    // TODO get value from stash placeholder
    if ( !has_placeholder_in_domain( val, dom ) ) {
        irb.CreateCall( sfn, { ConstantInt::get( i64, 0 ) } );
    } else {
        auto get = GetTaint( get_placeholder_in_domain( val, dom ) ).generate();
        auto i = irb.CreatePtrToInt( get, i64 );
        irb.CreateCall( sfn, { i } );
    }
}

Value* unstash_return_value( CallInst *call ) {
    IRBuilder<> irb( call );

    auto unfn = unstash_function( get_module( call ) );
    auto unstash = irb.CreateCall( unfn );

    auto dom = MDValue( call ).domain();
    auto ph = get_placeholder_in_domain( call, dom );

    auto ret = irb.CreateIntToPtr( unstash, ph->getType() );

    call->removeFromParent();
    call->insertBefore( unstash );

    return ret;
}

} // namespace bundle

void Tainting::run( Module &m ) {
    std::unordered_map< Value*, Value* > substitutes;
    auto phs = placeholders( m );

    for ( auto ph : phs )
        if ( !placeholder::is_stash( ph ) && !placeholder::is_unstash( ph ) )
            substitutes[ ph ] = process( ph );

    std::set< Function* > processed;
    run_on_abstract_calls( [&] ( auto call ) {
        auto fn = get_called_function( call );
        if ( call->getNumArgOperands() ) {
            bundle::stash_arguments( call );
            if ( !processed.count( fn ) ) {
                // stash default arguments for non-abstract calls
                for ( auto concrete : fn->users() )
                    if ( auto cc = dyn_cast< CallInst >( concrete ) )
                        if ( !cc->getMetadata( "lart.domains" ) )
                            bundle::stash_arguments( cc );

                auto vals = bundle::unstash_arguments( call );

                unsigned idx = 0;
                for ( auto ph : bundle::argument_placeholders( call ) )
                    substitutes[ ph ] = vals[ idx++ ];
            }
        }

        if ( !call->getType()->isVoidTy() && !call->getType()->isPointerTy() ) {
            if ( !processed.count( fn ) ) {
                bundle::stash_return_value( call );
            }

            auto dom = MDValue( call ).domain();
            auto ph = get_placeholder_in_domain( call, dom );
            substitutes[ ph ] = bundle::unstash_return_value( call );
        }

        processed.insert( fn );
    }, m );

    for ( auto &sub : substitutes )
        sub.first->replaceAllUsesWith( sub.second );

    for ( auto &ph : phs )
        ph->eraseFromParent();
}

Value* create_in_domain_phi( Instruction *placeholder ) {
    auto phi = cast< PHINode >( placeholder->getOperand( 0 ) );
    auto ty =placeholder->getType();

    IRBuilder<> irb( placeholder );
    auto abstract = irb.CreatePHI( ty, phi->getNumIncomingValues() );

    auto dom = MDValue( phi ).domain();

    for ( unsigned int i = 0; i < phi->getNumIncomingValues(); ++i ) {
        auto in = phi->getIncomingValue( i );
        auto bb = phi->getIncomingBlock( i );
        if ( !isa< Constant >( in ) && has_placeholder_in_domain( in, dom ) ) {
            auto val = get_placeholder_in_domain( in , dom );
            abstract->addIncoming( val, bb );
        } else {
            abstract->addIncoming( UndefValue::get( ty ), bb );
        }
    }

    return abstract;
}

Value* Tainting::process( Instruction *placeholder ) {
    auto op = placeholder->getOperand( 0 );
    if ( isa< LoadInst >( op ) )
        return ThawTaint( placeholder ).generate();
    if ( isa< PHINode >( op ) )
        return create_in_domain_phi( placeholder );
    if ( is_taintable( op ) )
        return Taint( placeholder ).generate();
    if ( placeholder::is_to_i1( placeholder ) )
        return ToBoolTaint( placeholder ).generate();
    if ( placeholder::is_assume( placeholder ) )
        return AssumeTaint( placeholder ).generate();
    UNREACHABLE( "Unknown placeholder", placeholder );
}

// ---------------------------- FreezeStores ---------------------------

void FreezeStores::run( Module &m ) {
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

void FreezeStores::process( StoreInst *store ) {
    auto m = get_module( store );
    auto meta = domain_metadata( *m, MDValue( store ).domain() );
    auto dom = meta.domain();

    auto val = store->getValueOperand();
    auto ptr = store->getPointerOperand();

    auto ty = store->getValueOperand()->getType();
    auto aty = meta.base_type();

    auto flag = Type::getInt1Ty( store->getContext() );
    auto freeze_fty = FunctionType::get( ty, { flag, ty, flag, aty, flag, ptr->getType() }, false );
    auto name = "lart." + dom.name() + ".freeze." + llvm_name( ty );
    auto freeze = get_or_insert_function( m, freeze_fty, name );

    Value *abstract;
    if ( has_placeholder_in_domain( val, dom ) )
        abstract = get_placeholder_in_domain( val, dom );
    else
        abstract = meta.default_value();

    Values args = { freeze, val, val, abstract, ptr };

    auto fty = FunctionType::get( ty, types_of( args ), false );
    auto tname = "__vm_test_taint." + freeze->getName().str();
    auto tfn = get_or_insert_function( m, fty, tname );

    IRBuilder<> irb( store );
    auto taint = irb.CreateCall( tfn, args );
    irb.CreateStore( taint, ptr );
}

// ---------------------------- Synthesize ---------------------------

void Synthesize::run( Module &m ) {
    for ( auto &t : taints( m ) )
        process( cast< CallInst >( t ) );
}

void Synthesize::process( CallInst *taint ) {
    auto fn = cast< Function >( taint->getOperand( 0 ) );
    if ( !fn->empty() )
        return;

    if ( is_taint_of_type( fn, ".thaw" ) ) {
        ThawLifter( taint ).syntetize();
    } else if ( is_taint_of_type( fn, ".freeze" ) ) {
        FreezeLifter( taint ).syntetize();
    } else if ( is_taint_of_type( fn, ".to_i1" ) ) {
        ToBoolLifter( taint ).syntetize();
    } else if ( is_taint_of_type( fn, ".assume" ) ) {
        AssumeLifter( taint ).syntetize();
    } else if ( is_taint_of_type( fn, ".get" ) ) {
        GetLifter( taint ).syntetize();
    } else if ( taint_args_size( taint ) == 1 ) {
        UnaryLifter( taint ).syntetize();
    } else if ( taint_args_size( taint ) == 2 ) {
        BinaryLifter( taint ).syntetize();
    } else {
        UNREACHABLE( "Unknown taint function", taint );
    }
}

} // namespace abstract
} // namespace lart
