// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
DIVINE_RELAX_WARNINGS
#include <llvm/IR/PassManager.h>

#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/ValueMap.h>

#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/query.h>

#include <lart/analysis/postorder.h>

#include <lart/abstract/types.h>
#include <lart/abstract/annotation.h>

#include <string>

namespace lart {
namespace abstract {

struct Abstraction : lart::Pass {

    using V = llvm::Value;
    using T = llvm::Type;
    using I = llvm::Instruction;
    using F = llvm::Function;
	using Dependencies = std::vector< V * >;

	virtual ~Abstraction() {}

	static PassMeta meta() {
    	return passMeta< Abstraction >(
        	"Abstraction", "Substitutes annotated values and by abstract values." );
    }

	llvm::PreservedAnalyses run( llvm::Module &m ) override {
        //init tristate
        _ctx = &m.getContext();
        auto bt = bool_t( *_ctx );
        auto tristate = lart::abstract::Tristate::get( bt );
        type_store.insert( { bt, tristate } );
        abstract_types.insert( tristate );

        auto annotations = getAbstractAnnotations( m );

        lart::util::Store< std::map< llvm::Function *, std::vector< Annotation > > > functions;
        for ( const auto &a : annotations ) {
            auto parent = a.allocaInst->getParent()->getParent();
            if ( functions.contains( parent ) )
                functions[ parent ].push_back( a );
            else
                functions.insert( { parent, { a } } );
        }

        for ( auto &f : functions ) {
            // create new abstract types
            auto& annotatedValues = f.second;
            for ( auto &a : annotatedValues )
                storeType( a.allocaInst->getAllocatedType() );

            bool changeReturn = false;
            llvm::Type * rty;

            // compute return dependency
            for ( auto &val : annotatedValues ) {
                auto deps = analysis::postorder< llvm::Value * >( val.allocaInst );
                changeReturn = changeReturn || dependentReturn( deps );
                if ( changeReturn ) {
                    rty = type_store[ val.allocaInst->getAllocatedType() ]->getPointerTo();
                    break;
                }
            }

            //unify return value
            if ( changeReturn ) {
                llvm::UnifyFunctionExitNodes ufen;
                ufen.runOnFunction( *f.first );
            }

            //substitute abstract values
            for ( auto &v : annotatedValues ) {
                propagateValue( v.allocaInst, v.allocaInst->getAllocatedType() );
            }
            storeFunction( f.first, changeReturn, rty );
        }

        //annotate anonymous lart functions
        for ( auto &a : toAnnotate ) {
            annotateAnonymous( a.first, a.second );
            a.first->eraseFromParent();
        }
        return llvm::PreservedAnalyses::none();
	}

    F * propagateArgument( F * f, V * v, T * t ) {
        auto atp = type_store[ t ]->getPointerTo();

		auto deps = analysis::postorder< V * >( v );
        bool changeReturn = dependentReturn( deps );

        if ( changeReturn ) {
            llvm::UnifyFunctionExitNodes ufen;
            ufen.runOnFunction( *f );
        }

        propagateValue( v, t );
        return storeFunction( f, changeReturn, atp );
	}

    void propagateValue( V * v, T * t ) {
        auto deps = analysis::postorder< V * >( v );
        for ( auto dep : lart::util::reverse( deps ) )
            if ( auto inst = llvm::dyn_cast< I >( dep ) )
                process( inst, t );
        // obscure removing because of cyclic dependencies in phinodes
        for ( auto dep : deps )
        	if ( auto inst = llvm::dyn_cast< I >( dep ) )
                inst->removeFromParent();
        for ( auto dep : deps )
            if ( auto inst = llvm::dyn_cast< I >( dep ) )
                inst->replaceAllUsesWith( llvm::UndefValue::get( inst->getType() ) );
        for ( auto dep : deps )
        	if ( auto inst = llvm::dyn_cast< I >( dep ) ) {
                auto val = value_store.find( inst );
                if ( val != value_store.end() )
                    value_store.erase( val );
                delete inst;
            }
    }

    void process( I * inst, T * t ) {
        llvmcase( inst,
     		//[&]( llvm::AllocaInst * ) { /*TODO*/ },
            //[&]( llvm::LoadInst * ) { /*TODO*/ },
            //[&]( llvm::StoreInst * ) { /*TODO*/ },
            [&]( llvm::ICmpInst * i ) {
                 doICmp( i );
            },
            [&]( llvm::SelectInst * i ) {
                doSelect( i, t );
            },
            [&]( llvm::BranchInst * i ) {
                doBranch( i );
   	        },
			[&]( llvm::BinaryOperator * i ) {
				doBinary( i, t );
			},
			[&]( llvm::CastInst * i ) {
                doCast( i );
            },
			[&]( llvm::PHINode * i ) {
                doPhi( i, t );
            },
			[&]( llvm::CallInst * i ) {
                if ( isAbstractValue( i ) ) {
        			auto rty = type_store[ t ]->getPointerTo();
                    annotate( i, rty, "lart.abstract.create." + getTypeName( t ) );
                } else if ( isLift( i ) ) {
                    i->replaceAllUsesWith( value_store[ i->getArgOperand( 0 ) ] );
                } else if ( isExplicate( i ) ) {
                    auto clone = i->clone();
                    clone->insertBefore( i );
                    i->replaceAllUsesWith( clone );
                } else {
					doCall( i, t );
                }
            },
 			[&]( llvm::ReturnInst * i ) {
                doReturn( i );
            },
			[&]( I *inst ) {
				std::cerr << "ERR: unknown instruction: ";
                inst->dump();
                std::exit( EXIT_FAILURE );
			} );
    }

    llvm::CallInst * createNamedCall( I * inst, T * rty, const std::string &tag,
                               std::vector< V * > args = {} )
    {
        auto acall = createCall( inst, rty, tag, args );
        value_store[ inst ] = acall;
        return acall;
    }

    void doICmp( llvm::ICmpInst * i ) {
	    auto args = getBinaryArgs( i );
        auto tag = "lart.abstract.icmp." + predicate.at( i->getPredicate() );
        auto bt = bool_t( *_ctx );
		createAnonymousCall( i, type_store[ bt ]->getPointerTo(), tag, args );
    }

    void doSelect( llvm::SelectInst * i, T * t ) {
        auto cond = value_store.contains( i->getCondition() )
                    ? explicate( i, i->getCondition() )
                    : i->getCondition();

        auto tv = i->getTrueValue();
		auto fv = i->getFalseValue();

        if ( !isAbstractType( tv->getType() ) )
            tv = value_store.contains( tv ) ? value_store[ tv ] : lift( tv, t, i );
        if ( !isAbstractType( fv->getType() ) )
            fv = value_store.contains( fv ) ? value_store[ fv ] : lift( fv, t, i );
        llvm::IRBuilder<> irb( i );
        auto sub = irb.CreateSelect( cond, tv, fv );
        value_store.insert( { i, sub } );
    }

    void doBranch( llvm::BranchInst * i ) {
        llvm::IRBuilder<> irb( i );
        if ( i->isUnconditional() ) {
            auto dest = i->getSuccessor( 0 );
            irb.CreateBr( dest );
        } else {
            //conditional branching
            auto cond = i->getCondition();
            if ( isAbstractType( i->getCondition()->getType() )
                 || value_store.contains( i->getCondition() ) )
                cond = explicate( i, i->getCondition() );
            auto tbb = i->getSuccessor( 0 );
            auto fbb = i->getSuccessor( 1 );
            irb.CreateCondBr( cond, tbb, fbb );
        }
    }

	void doBinary( llvm::BinaryOperator * i, T * t ) {
        auto args = getBinaryArgs( i );
        auto tag = "lart.abstract." + std::string( i->getOpcodeName() );
        auto rty = type_store[ t ]->getPointerTo();
        createAnonymousCall( i, rty, tag, args );
    }

    void doCast( llvm::CastInst * i ) {
        auto args = getUnaryArgs( i );
        auto tag = "lart.abstract." + std::string( i->getOpcodeName() ) + "."
                   + getTypeName( i->getSrcTy() ) + "." + getTypeName( i->getDestTy() );
        storeType( i->getDestTy() );
        auto rty = type_store[ i->getDestTy() ]->getPointerTo();
        createAnonymousCall( i, rty, tag, args );
    }

    void doPhi( llvm::PHINode * n, T * t ) {
        auto at = type_store[ t ]->getPointerTo();

        unsigned int niv = n->getNumIncomingValues();
        llvm::IRBuilder<> irb( n );
        auto sub = irb.CreatePHI( at, niv );
        value_store.insert( { n , sub } );
        if ( n->getType() == at )
            n->replaceAllUsesWith( sub );

        for ( unsigned int i = 0; i < niv; ++i ) {
            auto val = llvm::cast< I >( n->getIncomingValue( i ) );
            auto parent = n->getIncomingBlock( i );
            if ( value_store.contains( val ) ) {
                sub->addIncoming( value_store[ val ], parent );
            } else {
                if ( isAbstractType( val->getType() ) )
                    sub->addIncoming( val, parent );
                else {
                    auto nbb =  parent->splitBasicBlock( parent->getTerminator() );
                    auto nval = lift( val, t, nbb->getTerminator() );
                    sub->addIncoming( nval, nbb );
                }
            }
        }

        removeRedundantLifts( n, sub );
    }

    void doCall( llvm::CallInst * i, T * t ) {
        std::vector < V * > args;
		std::vector < T * > arg_types;

		auto at = type_store[ t ]->getPointerTo();

        for ( auto &arg : i->arg_operands() )
			if ( value_store.contains( arg ) ) {
            	arg_types.push_back( at );
                args.push_back( value_store[ arg ] );
            } else {
            	arg_types.push_back( arg->getType() );
                args.push_back( arg );
            }

        auto fn = i->getCalledFunction();

        if ( toAnnotate.contains( fn ) ) {
            //lart anonymous call
            auto rty = fn->getReturnType();
            auto tag = fn->getName();
            auto call = createNamedCall( i, rty, tag, args );
            auto find = toAnnotate.find( fn );
            i->replaceAllUsesWith( call );
            toAnnotate.insert( { call->getCalledFunction(), find->second } );
        } else {
            llvm::ArrayRef< T * > params = arg_types;
            auto stored = storedFn( fn, params );

            if ( !stored ) {
		        auto fty = llvm::FunctionType::get( fn->getFunctionType()->getReturnType(),
                                                    arg_types,
                                                    fn->getFunctionType()->isVarArg() );
                stored = cloneFunction( fn, fty );
                for ( auto &arg : stored->args() )
                    if ( arg.getType() == at )
                        value_store.insert( { &arg, &arg } );
                for ( auto &arg : stored->args() )
                    if ( arg.getType() == at )
                        stored = propagateArgument( stored, &arg, t );
                storeFunction( fn );
            }

            llvm::IRBuilder<> irb( i );
            auto abs = irb.CreateCall( stored, args );
            value_store.insert( { i, abs } );
        }
    }

    void doReturn( llvm::ReturnInst *i ) {
        llvm::IRBuilder<> irb( i );
        auto ret = value_store.contains( i->getReturnValue() )
                   ? value_store[ i->getReturnValue() ]
                   : i->getReturnValue();
        irb.CreateRet( ret );
    }

    std::vector< V * > getBinaryArgs( I * i ) {
        std::vector< V * > args;
        auto a = i->getOperand( 0 );
		auto b = i->getOperand( 1 );

		if ( value_store.contains( a ) )
			args.push_back( value_store[ a ] );
		else
			args.push_back( a );

		if ( value_store.contains( b ) )
			args.push_back( value_store[ b ] );
		else
			args.push_back( b );

        return args;
    }

    std::vector< V * > getUnaryArgs( I * i ) {
        auto a = i->getOperand( 0 );
		auto val = value_store.contains( a ) ? value_store[ a ] : a ;
        return { val };
    }

    llvm::CallInst * explicate( I * i, V * v ) {
        auto cond = value_store[ v ];
        auto tag = "lart.tristate.explicate";
        return createCall( i, bool_t( *_ctx ), tag, { cond } );
    }

    I * lift( V * v, T * t, I * to ) {
        auto at = type_store[ t ]->getPointerTo();

        llvm::IRBuilder<> irb( to );
        auto fty = llvm::FunctionType::get( at, { v->getType() }, false );
        auto tag = "lart.abstract.lift";
        auto ncall = to->getModule()->getOrInsertFunction( tag, fty );
        auto sub = irb.CreateCall( ncall, v );
        //value_store.insert( { v, sub } ); //FIXME minimeze number of lifts

        return sub;
    }

    //Helper functions
    bool isAbstractType( T * t ) {
        if ( t->isPointerTy() )
            t = t->getPointerElementType();
        return abstract_types.find( t ) != abstract_types.end();
    }

    bool dependentReturn( Dependencies deps ) {
        return query::query( deps ).any( [&] ( V *v ) {
                    return llvm::dyn_cast< llvm::ReturnInst >( v );
                } );
    }

    llvm::CallInst * createCall( I * inst, T * rty, const std::string &tag,
                                 std::vector< V * > args = {} )
    {
		std::vector< T * > arg_types = {};
		for ( auto &arg : args )
			arg_types.push_back( arg->getType() );

		llvm::ArrayRef < T * > params = arg_types;

		auto fty = llvm::FunctionType::get( rty, params, false );
        auto call = inst->getModule()->getOrInsertFunction( tag, fty );
        llvm::IRBuilder<> irb( inst );
        return irb.CreateCall( call, args );
    }

    F * storeFunction( F * f, bool change_ret = false, T * rty = nullptr ) {
        auto newf = change_ret ? lart::changeReturnValue( f, rty ) : f;

        if ( function_store.contains( f ) )
		    function_store[ f ].push_back( newf );
		else
		    function_store[ f ] = { newf };
        return newf;
    }

    void storeType( llvm::CallInst * call ) {
        auto type = call->getCalledFunction()->getFunctionType()->getReturnType();
        storeType( type );
    }

    void storeType( T * t ) {
        if ( t->isPointerTy() )
            t = t->getPointerElementType();
        if ( !type_store.contains( t ) ) {
            auto at = lart::abstract::Type::get( t );
            type_store.insert( { t, at } );
            abstract_types.insert( at );
        }
    }

    T * bool_t( llvm::LLVMContext &ctx ) {
        return llvm::IntegerType::getInt1Ty( ctx );
    }

    bool isLift( llvm::CallInst * call ) {
        return call->getCalledFunction()->getName() == "lart.abstract.lift";
    }

    bool isExplicate( llvm::CallInst * call ) {
        return call->getCalledFunction()->getName() == "lart.tristate.explicate";
    }

    void removeRedundantLifts( I * i, I * ni ) {
        std::vector< llvm::CallInst * > toRemove;

        for ( auto user : i->users() )
            if ( auto call = llvm::dyn_cast< llvm::CallInst >( user ) )
                if ( isLift( call ) ) {
                    call->replaceAllUsesWith( ni );
                    toRemove.push_back( call );
                }

        for ( auto call : toRemove )
            call->eraseFromParent();
    }

    //Function manipulations
	F * changeReturnValue( F *fn, T *rty ) {
        auto newfn = lart::changeReturnValue( fn, rty );

        auto find = function_store.find( fn );
        if ( find != function_store.end() )
            function_store.erase( find );
        fn->eraseFromParent();

        return newfn;
	}

    // Anonymous calls
    void createAnonymousCall( I * i, T * rty, const std::string &tag,
                           std::vector< V * > args = {} )
    {
        std::vector< T * > arg_types = {};
		for ( auto &arg : args )
			arg_types.push_back( arg->getType() );

        llvm::ArrayRef < T * > params = arg_types;

		auto fty = llvm::FunctionType::get( rty, params, false );
        using Linkage = llvm::GlobalValue::LinkageTypes;
        auto call = F::Create( fty, Linkage::ExternalLinkage, "", i->getModule() );

        llvm::IRBuilder<> irb( i );
        auto acall = irb.CreateCall( call, args );

        toAnnotate.insert( { acall->getCalledFunction(), tag } );
        value_store.insert( { i, acall } );

        removeRedundantLifts( i, acall );
    }

    void annotateAnonymous( F * f, std::string name ) {
        auto rty = f->getReturnType();

        std::vector< std::pair< llvm::CallInst *, llvm::CallInst *> > toReplace;
        for ( auto user : f->users() ) {
            auto call = llvm::cast< llvm::CallInst >( user );

            std::vector < V * > args;
            for ( auto &arg : call->arg_operands() )
                if ( isAbstractType( arg->getType() ) )
                    args.push_back( arg );
                else {
                    auto lifted = lift( arg, arg->getType(), call );
                    args.push_back( lifted );
                }

            std::vector< T * > arg_types;
		    for ( auto &arg : args )
			    arg_types.push_back( arg->getType() );

		    llvm::ArrayRef < T * > params = arg_types;

		    auto fty = llvm::FunctionType::get( rty, params, false );
            auto nf = call->getModule()->getOrInsertFunction( name, fty );
            auto ncall = llvm::CallInst::Create( nf, args );
            toReplace.push_back( { call, ncall } );
        }

        for ( auto &replace : toReplace )
            llvm::ReplaceInstWithInst( replace.first, replace.second );
    }

private:
	using Predicate = llvm::CmpInst::Predicate;
    const std::map< Predicate, std::string > predicate = {
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

    template < typename V >
    using AbstractStore = lart::util::Store< std::map < V, V > >;

    template < typename K, typename V >
    using FunctionStore = lart::util::Store< std::map < K, std::vector< V > > >;

	F * storedFn( F * fn, llvm::ArrayRef< T * > &params ) {
    	if ( function_store.contains( fn ) )
        	for ( auto store_fn : function_store[ fn ] )
            	if ( store_fn->getFunctionType()->params() == params )
                	return store_fn;
 		return nullptr;
  	}

    // substituted instructions
    AbstractStore < V * > value_store;

    // substituted types
    AbstractStore< T * > type_store;
    std::set< T * > abstract_types;

	// substituted functions
    FunctionStore < F *, F * > function_store;

    // functions to annotate
    lart::util::Store< std::map < F *, std::string > > toAnnotate;

    static llvm::StringRef _abstractName;

    llvm::LLVMContext *_ctx;
};

llvm::StringRef Abstraction::_abstractName = "__VERIFIER_nondet_";

struct Substitution : lart::Pass {
    Substitution( std::string type ) {
        if ( type == "zero" )
            abstractionName = "__abstract_zero_";
    }

    virtual ~Substitution() {}

    static PassMeta meta() {
    	return passMetaC( "Substitution",
                "Substitutes abstract values by concrete abstraction.",
                []( llvm::ModulePassManager &mgr, std::string opt ) {
                    return mgr.addPass( Substitution( opt ) );
                } );
    }

	llvm::PreservedAnalyses run( llvm::Module &m ) override {
        std::vector< std::pair< llvm::Function *, llvm::Type * > > toChangeReturnValue;

        abstractionType = m.getFunction( abstractionName + "alloca" )->getReturnType();
        for ( auto &fn : m ) {
            //find creation of abstract values
            auto abstract = query::query( fn ).flatten()
                 .map( query::refToPtr )
                 .map( query::llvmdyncast< llvm::CallInst > )
                 .filter( query::notnull )
                 .filter( [&]( llvm::CallInst *i ) {
                    auto call = i->getCalledFunction();
                    if ( call && call->hasName() )
                        return call->getName().startswith( "lart.abstract.alloca" )
                            || call->getName().startswith( "lart.abstract.lift" );
                    return false;
                 } )
                 .freeze();

            //substitute all dependencies of abstract values
            for ( auto a : abstract ) {
                auto deps = analysis::postorder< llvm::Value * >( a );
                for ( auto dep : lart::util::reverse( deps ) )
                    if ( auto inst = llvm::dyn_cast< llvm::Instruction >( dep ) )
                        process( m, inst );
            }

            removeAbstractedValues();

            if ( !abstract.empty() ) {
                auto typeName = lart::abstract::getTypeName( fn.getReturnType() );
                std::string prefix = "%lart.abstract";
                if ( !typeName.compare( 0, prefix.size(), prefix ) )
                    toChangeReturnValue.push_back( { &fn, abstractionType } );
            }
        }

        for ( auto &fn : toChangeReturnValue )
            lart::changeReturnValue( fn.first, fn.second );

        cleanup( m );
        return llvm::PreservedAnalyses::none();
    }

    void process( llvm::Module &m, llvm::Instruction * inst ) {
        llvmcase( inst,
     		//[&]( llvm::AllocaInst * ) { /*TODO*/ },
            //[&]( llvm::LoadInst * ) { /*TODO*/ },
            //[&]( llvm::StoreInst * ) { /*TODO*/ },
            //[&]( llvm::ICmpInst * ) { /*TODO*/ },
            //[&]( llvm::SelectInst * ) { /*TODO*/ },
            [&]( llvm::BranchInst * i ) {
                doBranch( i );
            },
			//[&]( llvm::CastInst * ) { /* TODO*/ },
			[&]( llvm::PHINode * i ) {
                doPhi( i );
            },
			[&]( llvm::CallInst * call ) {
                if ( call->getCalledFunction()->getName().startswith( "lart.abstract.lift" ) )
                    doLift( m, call );
                else
                    doCall( m, call );
            },
 			[&]( llvm::ReturnInst * i ) {
                doReturn( i );
            },
			[&]( llvm::Instruction *i ) {
				std::cerr << "ERR: unknown instruction: ";
                i->dump();
                std::exit( EXIT_FAILURE );
			} );
    }

    void doLift( llvm::Module & m, llvm::CallInst * call ) {
        auto name = getFunctionName( call->getCalledFunction()->getName() );
        auto fn = m.getFunction( name );

        llvm::IRBuilder<> irb( call );
        auto ncall = irb.CreateCall( fn, { call->getArgOperand( 0 ) } );
        abstraction_store[ call ] = ncall;
        abstractedValues.insert( call );
    }

    void doCall( llvm::Module & m, llvm::CallInst * call ) {
        auto name = getFunctionName( call->getCalledFunction()->getName() );
        auto fn = m.getFunction( name );

        std::vector < llvm::Value * > args;

        for ( auto &arg : call->arg_operands() ) {
            if ( abstraction_store.find( arg ) == abstraction_store.end() ) {
                //not all incoming values substituted
                //wait till have all args
                break;
            }
            args.push_back( abstraction_store[ arg ] );
        }

        //skip if do not have enough substituted arguments
        if ( call->getNumArgOperands() == args.size() ) {
            llvm::IRBuilder<> irb( call );
            auto ncall = irb.CreateCall( fn, args );
            abstraction_store[ call ] = ncall;
            abstractedValues.insert( call );
        }
    }

    std::string getFunctionName( llvm::StringRef callName ) {
        llvm::SmallVector< llvm::StringRef, 4 > nameParts;
        callName.split( nameParts, "." );
        if ( nameParts[ 1 ].startswith( "tristate" ) )
            return "__abstract_tristate_" + nameParts[ 2 ].str();
        if ( nameParts[ 2 ].startswith( "icmp" ) )
            return abstractionName + "icmp_" + nameParts[ 3 ].str();
        else
            return abstractionName + nameParts[ 2 ].str();
    }

    void doBranch( llvm::BranchInst * i ) {
        llvm::IRBuilder<> irb( i );
        if ( i->isConditional() ) {
            //conditional branching
            auto cond = i->getCondition();
            if ( abstraction_store.contains( cond ) ) {
                cond = abstraction_store[ cond ];
                auto tbb = i->getSuccessor( 0 );
                auto fbb = i->getSuccessor( 1 );
                irb.CreateCondBr( cond, tbb, fbb );
                abstractedValues.insert( i );
            }
        }
    }

    void doPhi( llvm::PHINode * phi ) {
        unsigned int niv = phi->getNumIncomingValues();


        llvm::IRBuilder<> irb( phi );
        auto nphi = irb.CreatePHI( abstractionType, niv );

        //removing duplicities
        auto find = abstraction_store.find( phi );
        if ( find != abstraction_store.end() ){
            auto abstracted = llvm::cast< llvm::Instruction >( abstraction_store[ phi ] );
            abstracted->replaceAllUsesWith( nphi );
            auto val = abstractedValues.find( abstracted );
            if ( val != abstractedValues.end() )
                abstractedValues.erase( val );
            abstraction_store.erase( find );
            abstracted->eraseFromParent();
        }

        abstraction_store.insert( { phi , nphi } );

        if ( phi->getType() == abstractionType )
            phi->replaceAllUsesWith( nphi );

        for ( unsigned int i = 0; i < niv; ++i ) {
            auto val = llvm::cast< llvm::Instruction >( phi->getIncomingValue( i ) );
            auto parent = phi->getIncomingBlock( i );
            if ( abstraction_store.contains( val ) )
                nphi->addIncoming( abstraction_store[ val ], parent );
            else {
                if ( val->getType() == abstractionType )
                    nphi->addIncoming( val, parent );
            }
        }

        abstractedValues.insert( phi );
    }

    void doReturn( llvm::ReturnInst * i ) {
        if ( abstraction_store.contains( i->getReturnValue() ) ) {
            llvm::IRBuilder<> irb( i );
            auto ret = abstraction_store[ i->getReturnValue() ];
            irb.CreateRet( ret );
            abstraction_store[ i ] = ret;
            abstractedValues.insert( i );
        }
    }

    void removeAbstractedValues() {
        for ( auto &inst : abstractedValues )
            inst->replaceAllUsesWith( llvm::UndefValue::get( inst->getType() ) );
        for ( auto &inst : abstractedValues ) {
            auto stored = abstraction_store.find( inst );
            if ( stored != abstraction_store.end() )
                abstraction_store.erase( stored );
            inst->eraseFromParent();
        }
        abstractedValues.clear();
    }

    void cleanup( llvm::Module & m ) {
        auto toErase = query::query( m )
                        .map( query::refToPtr )
                        .filter( []( auto &fn ) {
                            return fn->getName().startswith( "lart.abstract" )
                                || fn->getName().startswith( "lart.tristate" );
                        } ).freeze();

        for ( auto &fn : toErase )
            fn->eraseFromParent();
    }

private:
    template < typename V >
    using AbstractStore = lart::util::Store< std::map< V, V > >;

    AbstractStore< llvm::Value * > abstraction_store;

    std::set< llvm::Instruction * > abstractedValues;

    std::string abstractionName;
    llvm::Type * abstractionType;
};

PassMeta abstractionPass() {
    return passMetaC< Substitution >( "abstraction", "",
        []( llvm::ModulePassManager &mgr, std::string opt ) {
            Abstraction::meta().create( mgr, "" );
            Substitution::meta().create( mgr, opt );
        } );
};

} /* lart */
} /* abstract */

