#include <lart/abstract/substitution.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>
#include <lart/support/query.h>
#include <lart/support/util.h>
#include <lart/abstract/stash.h>

#include <iostream>

#include <brick-llvm>

namespace lart {
namespace abstract {

using namespace llvm;

using lart::util::get_module;
using lart::util::get_or_insert_function;

namespace {

bool has_dual_in_domain( Instruction *inst, Domain dom ) {
    return inst->getMetadata( "lart.dual." + dom.name() );
}

Instruction* get_dual_in_domain( Instruction *inst, Domain dom ) {
    auto &dual = inst->getMetadata( "lart.dual." + dom.name() )->getOperand( 0 );
    auto md = cast< ValueAsMetadata >( dual.get() );
    return cast< Instruction >( md->getValue() );
}

Function * get_function( CallInst * call ) {
    auto fns = get_potentialy_called_functions( call );
    ASSERT( fns.size() == 1 );
    return fns[ 0 ];
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
    auto dom = get_domain( call );

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
    return abstract->isStructTy() ? abstract->getContainedType( 0 ) : abstract;
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
    return util::is_one_of< BinaryOperator, CmpInst, CastInst, CallInst >( v );
}

ConstantInt* bitwidth( Value *v ) {
    auto &ctx = v->getContext();
    auto bw = v->getType()->getPrimitiveSizeInBits();
    return ConstantInt::get( IntegerType::get( ctx, 32 ), bw );
}

namespace placeholder {

bool is_placeholder_of_name( Instruction *inst, std::string name ) {
    auto fn = get_function( cast< CallInst >( inst ) );
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

bool is_store( Instruction *inst ) {
    return is_placeholder_of_name( inst, ".store." );
}

Domain domain( Instruction *inst ) {
    return get_domain( inst );
}

Type* return_type( Instruction *inst ) {
    auto ty = inst->getType();
    if ( ty->isVoidTy() )
        return ty;

    return domain_metadata( *inst->getModule(), get_domain( inst ) ).base_type();
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
    if ( is_stash( inst ) ) {
        name += "stash.";
    } else if ( is_unstash( inst ) ) {
        name += "unstash.";
    } else if ( is_to_i1( inst ) ) {
        name += "to_i1.";
    } else if ( is_assume( inst ) ) {
        name += "assume";
    } else if ( is_store( inst ) ) {
        name += "store.";
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
        { Predicate::FCMP_FALSE, "false" },
        { Predicate::FCMP_OEQ, "oeq" },
        { Predicate::FCMP_OGT, "ogt" },
        { Predicate::FCMP_OGE, "oge" },
        { Predicate::FCMP_OLT, "olt" },
        { Predicate::FCMP_OLE, "ole" },
        { Predicate::FCMP_ONE, "one" },
        { Predicate::FCMP_ORD, "ord" },
        { Predicate::FCMP_UNO, "uno" },
        { Predicate::FCMP_UEQ, "ueq" },
        { Predicate::FCMP_UGT, "ugt" },
        { Predicate::FCMP_UGE, "uge" },
        { Predicate::FCMP_ULT, "ult" },
        { Predicate::FCMP_ULE, "ule" },
        { Predicate::FCMP_UNE, "une" },
        { Predicate::FCMP_TRUE, "true" },
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
        if ( auto call = dyn_cast< CallInst >( inst ) ) {
            assert( domain_metadata( *get_module( inst ), dom ).kind() == DomainKind::content );
            return dom.name() + "." + call->getCalledFunction()->getName().str();
        }

        auto pref = prefix( inst, dom );

        if ( auto cmp = dyn_cast< CmpInst >( inst ) )
            return pref + "_" + predicate.at( cmp->getPredicate() )
                        + "." + llvm_name( cmp->getOperand( 0 )->getType() );

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

    Value* create_operation( Instruction *call, const std::string& name, Values && args ) {
        if ( auto fn = get_module( call )->getFunction( name ) ) {
            IRBuilder<> irb( call->getContext() );
            return irb.CreateCall( fn, args );
        } else {
            throw std::runtime_error( "missing function in domain: " + name );
        }
    }


    Value* process_thaw( CallInst *call, Values && args ) {
        auto name = "__" + domain.name() + "_thaw";
        return create_operation( call, name, std::move( args ) );
    }

    Value* process_freeze( CallInst *call, Values && args ) {
        auto name = "__" + domain.name() + "_freeze";
        return create_operation( call, name, std::move( args ) );
    }

    Value* process_assume( CallInst *call, Values && args ) {
        auto name = "__" + domain.name() + "_assume";
        return create_operation( call, name, { args[ 1 ], args[ 1 ], args[ 2 ] } );
    }

    Value* process_tobool( CallInst *call, Values && args ) {
        if ( domain == Domain::Tristate() ) {
            return create_operation( call, "__tristate_lower", std::move( args ) );
        } else {
            auto name = "__" + domain.name() + "_bool_to_tristate";
            return create_operation( call, name, std::move( args ) );
        }
    }

    ConstantInt* cast_bitwidth( Function *fn ) {
        auto &ctx = fn->getContext();
        auto type = fn->getName().rsplit('.').second;

        int bitwidth = 0;
        if ( type == "float" )
            bitwidth = 32;
        else if ( type == "double" )
            bitwidth = 64;
        else
            bitwidth = std::stoi( type.drop_front().str() );
        return ConstantInt::get( IntegerType::get( ctx, 32 ), bitwidth );
    }

    Value* process_cast( CallInst *call, Values && args ) {
        auto name = intr_name( call );
        auto op = cast< Function >( call->getOperand( 0 ) );
        return create_operation( call, name, { args[0], cast_bitwidth( op ) } );
    }

    Value* process_op( CallInst *call, Values && args ) {
        auto name = intr_name( call );
        return create_operation( call, name, std::move( args ) );
    }

    Value * process( Value * intr, Values && args ) {
        auto call = cast< CallInst >( intr );

        if ( is_thaw( call ) )
            return process_thaw( call, std::move( args ) );
        if ( is_freeze( call ) )
            return process_freeze( call, std::move( args ) );
        if ( is_cast( call ) )
            return process_cast( call, std::move( args ) );
        if ( is_assume( call ) )
            return process_assume( call, std::move( args ) );
        if ( is_tobool( call ) )
            return process_tobool( call, std::move( args ) );
        return process_op( call, std::move( args ) );
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

    LLVMContext & ctx() { return taint->getContext(); }

    Function * function() const { return taint_function( taint ); }
protected:
    Type * i32() { return IntegerType::get( ctx(), 32 ); }
    Type * i64() { return IntegerType::get( ctx(), 64 ); }

    Constant * i32_cv( uint32_t v ) { return ConstantInt::get( i32(), v ); }
    Constant * i64_cv( uint64_t v ) { return ConstantInt::get( i64(), v ); }

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

    struct ForwardedArg {
        Value *value;
    };

    CallInst * lift( Value * val ) {
        IRBuilder<> irb( ctx() );

        auto argc = i32_cv( 1 );

        if ( val->getType()->isIntegerTy() ) {
            // TODO rename lift to lift_int
            auto fn = get_module( val )->getFunction( "__" + domain().name()  + "_lift" );
            auto val64bit = irb.CreateSExt( val, i64() );
            return irb.CreateCall( fn, { bitwidth( val ), argc, val64bit } );
        }

        if ( val->getType()->isFloatingPointTy() ) {
            auto fn = get_module( val )->getFunction( "__" + domain().name()  + "_lift_float" );
            return irb.CreateCall( fn, { bitwidth( val ), argc, val } );
        }

        auto m = get_module( val );
        auto dom = domain_metadata( *m, domain() );

        if ( dom.kind() == DomainKind::content ) {
            auto fault = m->getFunction( "__" + domain().name() + "_undef_value" );
            return irb.CreateCall( fault );
        }

        UNREACHABLE( "Unknown type to be lifted.\n" );
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

    using Arguments = Values;

    template< typename Builder >
    void build( Builder irb, Arguments && args ) {
        auto thaw = LifterBuilder( domain() ).process( taint, std::move( args ) );
        irb.Insert( cast< Instruction >( thaw ) );
        irb.CreateRet( thaw );
    }

    void syntetize() final {
        ASSERT( function()->empty() );
        IRBuilder<> irb( make_bb( function(), "entry" ) );

        Value * addr = std::next( function()->arg_begin(), 3 );
        auto base = addr->getType()->getPointerElementType();
        auto bitwidth = base->getPrimitiveSizeInBits();

        if ( !base->isIntegerTy( 8 ) )
            addr = irb.CreateBitCast( addr, Type::getInt8PtrTy( ctx() ) );

        if ( base->isIntegerTy() || base->isFloatingPointTy() ) {
            build( irb, { addr, i32_cv( bitwidth ) } );
        } else {
            UNREACHABLE( "Unsupported type for thawing." );
        }
    }
};

struct FreezeLifter : BaseLifter {
    using BaseLifter::BaseLifter;

    void syntetize() final {
        ASSERT( function()->empty() );
        IRBuilder<> irb( make_bb( function(), "entry" ) );

        auto begin = function()->arg_begin();
        auto value = std::next( begin );
        auto formula = std::next( begin, 3 );
        auto addr = std::next( begin, 5 );

        auto bcst = irb.CreateBitCast( &*addr, Type::getInt8PtrTy( addr->getContext() ) );

        Values args = { &*formula, &*bcst };
        auto freeze = LifterBuilder( domain() ).process( taint, std::move( args ) );
        irb.Insert( cast< Instruction >( freeze ) );

        irb.CreateRet( value );
    }
};

struct ToBoolLifter : Lifter {
    using Lifter::Lifter;

    void syntetize() final {
        IRBuilder<> irb( make_bb( function(), "entry" ) );
        auto arg = *args().begin();

        auto tristate = LifterBuilder( domain() ).process( taint, { arg.abstract.value } );
        irb.Insert( cast< Instruction >( tristate ) );

        auto ret = LifterBuilder( Domain::Tristate() ).process( taint, { tristate } );
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
        auto call = LifterBuilder( domain() ).process( taint, std::move( args ) );
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
        auto rep = LifterBuilder( domain() ).process( taint, std::move( vals ) );
        irb.Insert( cast< Instruction >( rep ) );
        irb.CreateRet( rep );
    }
};

struct BinaryLifter : Lifter {
    using Lifter::Lifter;

    void syntetize() final {
        auto fn = function();
        auto exit = make_bb( fn, "exit" );

        std::vector< std::variant< ArgBlock, ForwardedArg > > arguments;
        std::vector< ArgBlock > blocks;
        for ( const auto &arg_pair : args() ) {
            if ( is_base_type_in_domain( fn->getParent(), arg_pair.concrete.value, domain() ) ) {
                auto block = generate_arg_block( arg_pair, blocks.size() );

                if ( !blocks.empty() ) {
                    auto prev = blocks.back();
                    prev.exit->getTerminator()->setSuccessor( 0, block.entry );
                }

                exit->moveAfter( block.exit );
                IRBuilder<> irb( block.exit );
                irb.CreateBr( exit );

                blocks.emplace_back( block );
                arguments.emplace_back( block );
            } else {
                arguments.emplace_back( ForwardedArg{ arg_pair.concrete.value } );
            }
        }

        IRBuilder<> irb( exit );

        auto abstract_args = query::query( arguments )
            .map( [] ( auto & arg ) {
                return std::visit( []( auto& a ) -> Value * { return a.value; }, arg );
            } )
            .freeze();
        auto call = LifterBuilder( domain() ).process( taint, std::move( abstract_args ) );
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

struct StoreLifter : Lifter {
    using Lifter::Lifter;

    void syntetize() final {
        auto args_begin = function()->arg_begin();
        auto val = std::next( args_begin, 1 );
        auto origin = std::next( args_begin, 5 );
        auto offset = std::next( args_begin, 7 );

        IRBuilder<> irb( make_bb( function(), "entry" ) );
        Values vals = { val, origin, offset };
        auto lifter = LifterBuilder( domain() ).process( taint, std::move( vals ) );
        irb.Insert( cast< Instruction >( lifter ) );
        irb.CreateRet( lifter );
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
    add_abstract_metadata( call, get_domain( inst ) );

    if ( placeholder::is_to_i1( call ) )
        inst->replaceAllUsesWith( call );

    if ( call->getType() == inst->getType() )
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
        auto m = get_module( placeholder );
        if ( placeholder::is_to_i1( placeholder ) )
            return concrete();
        if ( placeholder::is_assume( placeholder ) )
            return concrete();
        if ( placeholder::is_store( placeholder ) )
            return concrete();
        if ( !is_base_type( m, concrete() ) )
            return concrete();
        auto meta = domain_metadata( *m, domain() );
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
        if ( placeholder::is_store( placeholder ) )
            // some arbitrary value, because void is not valid argument type
            return UndefValue::get( Type::getInt1Ty( placeholder->getContext() ) );
        return placeholder->getOperand( 0 );
    }

    Domain domain() const {
        auto val = placeholder::is_store( placeholder ) ? placeholder->getOperand( 1 )
                                                        : placeholder->getOperand( 0 );
        return get_domain( val );
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
        auto ops = isa< CallInst >( inst ) ? cast< CallInst >( inst )->arg_operands()
                                           : inst->operands();
        for ( auto & op : ops ) {
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

struct StoreTaint : TaintBase< StoreTaint > {
    using TaintBase< StoreTaint >::TaintBase;

    Values arguments() {
        auto val = placeholder->getOperand( 0 );
        auto ptr = placeholder->getOperand( 1 );
        auto origin = get_addr_origin( cast< Instruction >( ptr ) );
        auto aorigin = get_placeholder_in_domain( origin, domain() );
        auto offset = get_addr_offset( cast< Instruction >( ptr ) );
        return { val, ptr, aorigin, offset };
    }

    std::string name() const {
        return domain().name() + ".store";
    }
};

namespace bundle {

Values argument_placeholders( Function * fn ) {
    return query::query( fn->args() )
        .map( query::refToPtr )
        .filter( [&] ( auto arg ) {
            return !is_concrete( arg );
        } )
        .filter( [&] ( const auto & arg ) {
            return  is_base_type( fn->getParent(), arg );
        } )
        .map( [&] ( auto arg ) -> Value* {
            auto dom = get_domain( arg );
            return get_placeholder_in_domain( arg, dom );
        } ).freeze();
}


StructType* packed_type( Function * fn ) {
    auto phs = argument_placeholders( fn );
    return StructType::get( fn->getContext(), types_of( phs ) );
}

void stash_arguments( CallInst *call ) {
    auto fn = get_some_called_function( call );
    if ( fn->getMetadata( abstract_tag ) )
        return; // skip internal lart functions

    auto pack_ty = packed_type( fn );

    IRBuilder<> irb( call );
    auto pack = irb.CreateAlloca( pack_ty );

    FunctionMetadata fmd{ fn };

    Value *val = UndefValue::get( pack_ty );

    unsigned pack_pos = 0;

    auto m = fn->getParent();
    for ( auto &arg : fn->args() ) {
        auto idx = arg.getArgNo();
        auto op = call->getArgOperand( idx );
        auto dom = fmd.get_arg_domain( idx );

        Value *abstract_arg = nullptr;
        if ( is_concrete( dom ) || !is_base_type_in_domain( m, op, dom ) ) {
            continue; // skip
        } else if ( is_concrete( op ) && !is_concrete( dom ) ) {
            auto meta = domain_metadata( *m, dom );
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

Values unstash_arguments( CallInst *call, Function * fn ) {
    if ( fn->getMetadata( abstract_tag ) )
        return {}; // skip internal lart functions

    IRBuilder<> irb( &*fn->getEntryBlock().begin() );

    auto pack_ty = packed_type( fn );

    auto unfn = unstash_function( get_module( call ) );
    auto unstash = irb.CreateCall( unfn );
    auto packed = irb.CreateIntToPtr( unstash, pack_ty->getPointerTo() );
    auto loaded = irb.CreateLoad( packed );

    Values unpacked;
    for ( unsigned int i = 0; i < pack_ty->getNumElements(); ++i )
        unpacked.push_back( irb.CreateExtractValue( loaded, { i } ) );
    return unpacked;
}

void stash_return_value( CallInst *call, Function * fn ) {
    if ( fn->getMetadata( abstract_tag ) )
        return; // skip internal lart functions

    if ( auto terminator = returns_abstract_value( call, fn ) ) {
        auto ret = cast< ReturnInst >( terminator );
        auto val = ret->getReturnValue();

        IRBuilder<> irb( ret );
        auto sfn = stash_function( get_module( call ) );

        auto dom = get_domain( call );
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
}

Value* unstash_return_value( CallInst *call ) {
    Value * ret = nullptr;

    Values terminators;
    run_on_potentialy_called_functions( call, [&] ( auto fn ) {
        terminators.push_back( returns_abstract_value( call, fn ) );
    } );

    size_t noreturns = std::count( terminators.begin(), terminators.end(), nullptr );

    if ( noreturns != terminators.size() ) { // there is at least one return
        IRBuilder<> irb( call );

        auto unfn = unstash_function( get_module( call ) );
        auto unstash = irb.CreateCall( unfn );

        auto dom = get_domain( call );

        auto meta = domain_metadata( *get_module( call ), dom );
        auto base = meta.base_type();

        if ( base->isPointerTy() ) {
            ret = irb.CreateIntToPtr( unstash, base );
        } else if ( base->isIntegerTy() ) {
            ret = irb.CreateIntCast( unstash, base, false );
        }

        call->removeFromParent();
        call->insertBefore( unstash );
    }

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
        if ( is_transformable( call ) )
            return;

        if ( call->getNumArgOperands() )
            bundle::stash_arguments( call );

        auto m = get_module( call );
        run_on_potentialy_called_functions( call, [&] ( auto fn ) {
            if ( call->getNumArgOperands() ) {
                if ( !processed.count( fn ) ) {
                    // stash default arguments for non-abstract calls
                    for ( auto concrete : fn->users() )
                        if ( auto cc = dyn_cast< CallInst >( concrete ) )
                            if ( !cc->getMetadata( "lart.domains" ) )
                                bundle::stash_arguments( cc );

                    auto vals = bundle::unstash_arguments( call, fn );

                    unsigned idx = 0;
                    for ( auto ph : bundle::argument_placeholders( fn ) ) {
                        substitutes[ ph ] = vals[ idx++ ];
                    }
                }
            }

            if ( is_base_type( m, call ) )
                if ( !processed.count( fn ) )
                    bundle::stash_return_value( call, fn );

            processed.insert( fn );
        } );

        if ( is_base_type( m, call ) ) {
            if ( auto ret = bundle::unstash_return_value( call ) ) {
                auto dom = get_domain( call );
                auto ph = get_placeholder_in_domain( call, dom );
                substitutes[ ph ] = ret;
            }
        }
    }, m );

    for ( auto &sub : substitutes ) {
        sub.first->replaceAllUsesWith( sub.second );
    }

    for ( auto &ph : phs )
        ph->eraseFromParent();
}

Value* create_in_domain_phi( Instruction *placeholder ) {
    auto phi = cast< PHINode >( placeholder->getOperand( 0 ) );
    auto ty =placeholder->getType();

    IRBuilder<> irb( placeholder );
    auto abstract = irb.CreatePHI( ty, phi->getNumIncomingValues() );

    auto dom = get_domain( phi );

    for ( unsigned int i = 0; i < phi->getNumIncomingValues(); ++i ) {
        auto in = phi->getIncomingValue( i );
        auto bb = phi->getIncomingBlock( i );
        if ( !isa< Constant >( in ) && has_placeholder_in_domain( in, dom ) ) {
            auto val = get_placeholder_in_domain( in , dom );
            abstract->addIncoming( val, bb );
        } else {
            auto default_value = [&] () -> llvm::Constant * {
                if ( ty->isIntegerTy() )
                    return llvm::ConstantInt::get( ty, 0 );
                else if ( ty->isFloatingPointTy() )
                    return llvm::ConstantFP::get( ty, 0 );
                else if ( ty->isPointerTy() )
                    return llvm::ConstantPointerNull::get( cast< PointerType >( ty ) );
                else
                    UNREACHABLE( "Unsupported type" );
            }();
            abstract->addIncoming( default_value, bb );
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
    if ( placeholder::is_to_i1( placeholder ) )
        return ToBoolTaint( placeholder ).generate();
    if ( placeholder::is_assume( placeholder ) )
        return AssumeTaint( placeholder ).generate();
    if ( placeholder::is_store( placeholder ) )
        return StoreTaint( placeholder ).generate();
    if ( is_taintable( op ) )
        return Taint( placeholder ).generate();
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
        .filter( [&] ( auto store ) {
            return  is_base_type_in_domain( &m, store->getOperand( 0 ), get_domain( store ) );
        } )
        .freeze();

    for ( auto s : stores ) {
        process( cast< StoreInst >( s ) );
    }
}

void FreezeStores::process( StoreInst *store ) {
    auto m = get_module( store );
    auto & ctx = m->getContext();

    auto meta = domain_metadata( *m, get_domain( store ) );
    auto dom = meta.domain();

    auto val = store->getValueOperand();
    auto ptr = store->getPointerOperand();

    auto ty = store->getValueOperand()->getType();
    auto aty = meta.base_type();

    auto flag = Type::getInt1Ty( ctx );

    auto freeze_fty = FunctionType::get( ty, { flag, ty, flag, aty, flag, ptr->getType() }, false );
    auto name = "lart." + dom.name() + ".freeze." + llvm_name( ty );
    auto freeze = get_or_insert_function( m, freeze_fty, name );

    // used to trigger freeze in the case of rewriting frozen variable
    auto tainted = m->getNamedValue( "__tainted_ptr" );

    IRBuilder<> irb( store );

    Value *abstract = nullptr;
    if ( has_placeholder_in_domain( val, dom ) ) {
        abstract = get_placeholder_in_domain( val, dom );
    } else {
        auto taint = irb.CreateLoad( Type::getInt8PtrTy( ctx ) , tainted );
        abstract = irb.CreateBitCast( taint, aty );
    }

    Values args = { freeze, val, val, abstract, ptr };

    auto fty = FunctionType::get( ty, types_of( args ), false );
    auto tname = "__vm_test_taint." + freeze->getName().str();
    auto tfn = get_or_insert_function( m, fty, tname );

    irb.CreateCall( tfn, args );
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
    } else if ( is_taint_of_type( fn, ".store" ) ) {
        StoreLifter( taint ).syntetize();
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
