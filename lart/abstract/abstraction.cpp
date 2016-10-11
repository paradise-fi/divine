// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
DIVINE_RELAX_WARNINGS
#include <llvm/IR/PassManager.h>

#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
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

        //functions containing abstract values
        auto funs = query::query( m )
                    .filter( [&]( F &f ) {
                        auto calls = query::query( f ).flatten()
                             .map( query::llvmdyncast< llvm::CallInst > )
                             .filter( query::notnull ).freeze();
                        return query::any( calls, [&]( llvm::CallInst * call ) {
                              return isAbstractValue( call );
                            } );
                    } )
                    .map( query::refToPtr ).freeze();

        std::vector< F * > abstractDeclarations;
        for ( auto &f : m )
            if ( f.getName().startswith( _abstractName ) )
                abstractDeclarations.push_back( &f );

        for ( auto &f : funs ) {
            // compute all abstract values in function f
            auto vals = query::query( *f ).flatten()
						.map( query::refToPtr )
						.map( query::llvmdyncast< llvm::CallInst > )
						.filter( query::notnull )
						.filter( [&]( llvm::CallInst *call ) {
							return isAbstractValue( call );
						} ).freeze();

            // create new abstract types
            for ( auto v : vals )
                storeType( v );

            // compute return dependency
            bool change_ret = false;
            T * rty;
            auto it = vals.begin();
            while ( !change_ret && it != vals.end() ) {
                auto deps = analysis::postorder< V * >( *it );
                change_ret = dependentReturn( deps );
                if ( change_ret ) {
                    auto t = (*it)->getCalledFunction()->getFunctionType()->getReturnType();
                    rty = type_store[ t ]->getPointerTo();
                }
                ++it;
            }

            //unify return value
            if ( change_ret ) {
                llvm::UnifyFunctionExitNodes ufen;
                ufen.runOnFunction( *f );
            }

            //substitute abstract values
            for ( auto v : vals ) {
                auto t = v->getCalledFunction()->getFunctionType()->getReturnType();
                propagateValue( v, t );
            }

            storeFunction( f, change_ret, rty );
        }

        //annotate anonym lart functions
        for ( auto &a : to_annotate ) {
            annotateAnonym( a.first, a.second );
            a.first->eraseFromParent();
        }

        for ( auto &f : abstractDeclarations )
            f->eraseFromParent();
        return llvm::PreservedAnalyses::none();
	}

    F * propagateArgument( F * f, V * v, T * t ) {
        auto atp = type_store[ t ]->getPointerTo();

		auto deps = analysis::postorder< V * >( v );
        bool change_ret = dependentReturn( deps );

        if ( change_ret ) {
            llvm::UnifyFunctionExitNodes ufen;
            ufen.runOnFunction( *f );
        }

        propagateValue( v, t );
        return storeFunction( f, change_ret, atp );
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
			//[&]( llvm::CastInst * ) { /* TODO*/ },
			[&]( llvm::PHINode * i ) {
                doPhi( i, t );
            },
			[&]( llvm::CallInst * i ) {
                if ( isAbstractValue( i ) ) {
        			auto rty = type_store[ t ]->getPointerTo();
                    annotate( i, rty, "lart.abstract.create" );
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

    llvm::CallInst * annotate( I * inst, T * rty, const std::string &tag,
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
		createAnonymCall( i, type_store[ bt ], tag, args );
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
        createAnonymCall( i, rty, tag, args );
    }

    void doPhi( llvm::PHINode * n, T * t ) {
        auto at = type_store[ t ]->getPointerTo();

        unsigned int niv = n->getNumIncomingValues();
        llvm::IRBuilder<> irb( n );
        auto sub = irb.CreatePHI( at, niv );
        value_store.insert( { n , sub } );

        for ( unsigned int i = 0; i < niv; ++i ) {
            auto val = llvm::cast< I >( n->getIncomingValue( i ) );
            auto parent = n->getIncomingBlock( i );
            if ( value_store.contains( val ) ) {
                sub->addIncoming( value_store[ val ], parent );
            } else {
                auto nbb =  parent->splitBasicBlock( parent->getTerminator() );
                auto nval = lift( val, t, nbb->getTerminator() );
                sub->addIncoming( nval, nbb );
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

        if ( to_annotate.contains( fn ) ) {
            //lart anonym call
            auto rty = fn->getReturnType();
            auto tag = fn->getName();
            auto call = annotate( i, rty, tag, args );
            auto find = to_annotate.find( fn );
            i->replaceAllUsesWith( call );
            to_annotate.insert( { call->getCalledFunction(), find->second } );
        } else {
            llvm::ArrayRef< T * > params = arg_types;
            auto stored = storedFn( fn, params );

            if ( !stored ) {
                llvm::ValueToValueMapTy vmap;
                stored = cloneFn( fn, arg_types, vmap );
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
    bool isAbstractValue( llvm::CallInst *call ) {
		if ( auto callee = getCalledFunction( call ) )
        	return callee->getName().startswith( _abstractName );
		return false;
    }

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
        auto newf = change_ret ? changeRetValue( f, rty ) : f;
		if ( function_store.contains( f ) )
		    function_store[ f ].push_back( newf );
		else
		    function_store[ f ] = { newf };
        return newf;
    }

    void storeType( llvm::CallInst * call ) {
        auto type = call->getCalledFunction()->getFunctionType()->getReturnType();

        if ( !type_store.contains( type ) ) {
            auto at = lart::abstract::Type::get( type );
            type_store.insert( { type, at } );
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
	F * cloneFn( F *fn, std::vector< T * > &arg_types, llvm::ValueToValueMapTy &vmap ) {
		auto fty = llvm::FunctionType::get( fn->getFunctionType()->getReturnType(),
                                            arg_types,
                                            fn->getFunctionType()->isVarArg() );
		return changeSignature( fn, fty, fn->getParent(), vmap );
	}

	F * changeRetValue( F *fn, T *rty ) {
		llvm::ValueToValueMapTy vmap;
        auto fty = llvm::FunctionType::get( rty,
                                            fn->getFunctionType()->params(),
                                            fn->getFunctionType()->isVarArg() );
        auto m = fn->getParent();
        fn->removeFromParent();
        auto newFn = changeSignature( fn, fty, m, vmap );
        auto find = function_store.find( fn );
        if ( find != function_store.end() )
            function_store.erase( find );
        delete fn;
        return newFn;
	}

	F * changeSignature( F * fn, llvm::FunctionType * fty, llvm::Module * m,
                         llvm::ValueToValueMapTy &vmap )
 	{
    	auto newfn = llvm::Function::Create( fty, fn->getLinkage(),fn->getName() );
        m->getFunctionList().push_back( newfn );

        auto destIt = newfn->arg_begin();
        for ( const auto &arg : fn->args() )
        	if ( vmap.count( &arg ) == 0 ) {
            	destIt->setName( arg.getName() );
               	vmap[&arg] = &*destIt++;
           	}

        // FIXME CloneDebugInfoMeatadata
        llvm::SmallVector< llvm::ReturnInst *, 8 > returns;
        llvm::CloneFunctionInto( newfn, fn, vmap, false, returns, "", nullptr );

        return newfn;
	}

    // Anonym calls
    void createAnonymCall( I * i, T * rty, const std::string &tag,
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

        to_annotate.insert( { acall->getCalledFunction(), tag } );
        value_store.insert( { i, acall } );

        removeRedundantLifts( i, acall );
    }

    void annotateAnonym( F * f, std::string name ) {
        auto rty = f->getReturnType();

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

            std::vector< T * > arg_types = {};
		    for ( auto &arg : args )
			    arg_types.push_back( arg->getType() );

		    llvm::ArrayRef < T * > params = arg_types;

		    auto fty = llvm::FunctionType::get( rty, params, false );
            auto nf = call->getModule()->getOrInsertFunction( name, fty );
            auto ncall = llvm::CallInst::Create( nf, args );
            llvm::ReplaceInstWithInst( call, ncall );
        }
    }

private:
    template < typename K, typename V >
    using Map = std::map< K, V >;

	using Predicate = llvm::CmpInst::Predicate;
    const Map< Predicate, std::string > predicate = {
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

    template < typename Container >
    struct Store : public Container {
   		using K = typename Container::key_type;

        bool contains( K key ) const {
        	return this->find( key ) != this->end();
        }
  	};

    template < typename V >
    using AbstractStore = Store< Map < V, V > >;

    template < typename K, typename V >
    using FunctionStore = Store< Map < K, std::vector< V > > >;

	using FunctionRecord = std::pair< F *, std::bitset< 256 > >;

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
    Store< Map < F *, std::string > > to_annotate;

    static llvm::StringRef _abstractName;

    llvm::LLVMContext *_ctx;
};

llvm::StringRef Abstraction::_abstractName = "__VERIFIER_nondet_";

PassMeta abstractionPass() {
    return passMeta< Abstraction >( "abstraction", "" );
};

} /* lart */
} /* abstract */

