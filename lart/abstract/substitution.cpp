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
    auto tag = std::string( meta::tag::dual ) + "." + dom.name();
    return meta::has( inst, tag );
}

llvm::Instruction * get_dual_in_domain( Instruction *inst, Domain dom ) {
    auto tag = std::string( meta::tag::dual ) + "." + dom.name();
    return llvm::cast< llvm::Instruction >( meta::get_value_from_meta( inst, tag ) );
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
    auto dom = Domain::get( call );

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

bool is_thaw( Instruction *inst ) {
    return is_placeholder_of_name( inst, ".thaw." );
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

bool is_load( Instruction *inst ) {
    return is_placeholder_of_name( inst, ".load." );
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
            assert( DomainMetadata::get( inst->getModule(), dom ).kind() == DomainKind::content );
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
        auto dom = DomainMetadata::get( m, domain() );

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

struct StoreLifter : Lifter {
    using Lifter::Lifter;

    void syntetize() final {
        auto args_begin = function()->arg_begin();
        auto val = std::next( args_begin, 1 );
        auto ptr = std::next( args_begin, 3 );

        IRBuilder<> irb( make_bb( function(), "entry" ) );
        Values vals = { val, ptr };
        auto lifter = LifterBuilder( domain() ).process( taint, std::move( vals ) );
        irb.Insert( cast< Instruction >( lifter ) );
        irb.CreateRet( UndefValue::get( Type::getInt1Ty( lifter->getContext() ) ) );
    }
};

struct LoadLifter : Lifter {
    using Lifter::Lifter;

    void syntetize() final {
        auto args_begin = function()->arg_begin();
        auto ptr = std::next( args_begin, 1 );

        IRBuilder<> irb( make_bb( function(), "entry" ) );
        Values vals = { ptr };
        auto lifter = LifterBuilder( domain() ).process( taint, std::move( vals ) );
        irb.Insert( cast< Instruction >( lifter ) );
        irb.CreateRet( lifter );
    }
};

} // anonymous namespace

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
    } else if ( is_taint_of_type( fn, ".to_i1" ) ) {
        ToBoolLifter( taint ).syntetize();
    } else if ( is_taint_of_type( fn, ".assume" ) ) {
        AssumeLifter( taint ).syntetize();
    } else if ( is_taint_of_type( fn, ".store" ) ) {
        StoreLifter( taint ).syntetize();
    } else if ( is_taint_of_type( fn, ".load" ) ) {
        LoadLifter( taint ).syntetize();
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
