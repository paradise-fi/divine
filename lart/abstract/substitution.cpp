// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/substitution.h>

namespace lart {
namespace abstract {

llvm::PreservedAnalyses Substitution::run( llvm::Module & ) {
    return llvm::PreservedAnalyses::none();
}

/*struct Substitution : lart::Pass {
    Substitution( std::string type ) {
        if ( type == "zero" )
            abstractionName = "__abstract_zero_";
        if ( type == "test" )
            abstractionName = "__abstract_test_";
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
        // TODO generalize
        abstractionType = m.getFunction( abstractionName + "alloca" )
                          ->getReturnType()->getPointerElementType();

        processAbstractArguments( m );

        auto nameFilter = []( std::string name ) {
            return [&]( llvm::CallInst * call ) -> bool {
                auto fn = call->getCalledFunction();
                return fn != nullptr && fn->hasName() && fn->getName().startswith( name );
            };
        };

        auto allocaFilter = nameFilter( "lart.abstract.alloca" );
        auto allocas = identify< llvm::CallInst, decltype(allocaFilter) >( m, allocaFilter );

        auto liftFilter = nameFilter( "lart.abstract.lift" );
        auto lifts = identify< llvm::CallInst, decltype(liftFilter) >( m, liftFilter );

        std::vector< llvm::Value * > abstract;
        abstract.reserve( allocas.size() + lifts.size() );
        abstract.insert( abstract.end(), allocas.begin(), allocas.end() );
        abstract.insert( abstract.end(), lifts.begin(), lifts.end() );

        TToVStoreMap< llvm::Function *, llvm::Value * > funToValMap;
        for ( const auto &a : abstract ) {
            auto inst = llvm::dyn_cast< llvm::Instruction >( a );
            assert( inst != nullptr );

            abstractedTypes.insert( inst->getType() );

            llvm::Function * fn = inst->getParent()->getParent();
            if ( function_store.contains( fn ) )
                continue;
            if ( funToValMap.contains( fn ) )
                funToValMap[ fn ].push_back( a );
            else
                funToValMap.insert( { fn, { a } } );
        }

        for ( auto & fn : funToValMap ) {
            if ( fn.first->hasName() && fn.first->getName().startswith( "lart.abstract" ) )
                continue;
            //substitute all dependencies of abstract values
            for ( auto & value : fn.second )
                propagateAndProcess( m, value );
        }

        // FIXME can be done from start in postorder of all functions
        // in cleaner way
        auto retAbsVal = query::query( m )
                        .map( query::refToPtr )
                        .filter( [&]( llvm::Function * fn ) {
                            return isAbstractType( fn->getReturnType() )
                                && ! isAbstractDeclaration( fn );
                        } )
                        .filter( [&]( llvm::Function * fn ) {
                            return ! function_store.contains( fn )
                                 || funToValMap.contains( fn );
                        } ).freeze();

        auto order = analysis::callPostorder< llvm::Function * >( m, retAbsVal );

        for ( auto fn : order ) {
            processAbstractReturn( fn );
        }

        removeInstructions< decltype( abstractedValues ) >( abstractedValues );
        abstractedValues.clear();

        //remove abstracted functions
        std::vector< llvm::Function * > functionsToRemove;
        for ( auto it : function_store )
            functionsToRemove.push_back( it.first );
        for ( auto & fn : functionsToRemove ) {
            fn->replaceAllUsesWith( llvm::UndefValue::get( fn->getType() ) );
            fn->eraseFromParent();
        }

        removeAbstractDeclarations( m );
        return llvm::PreservedAnalyses::none();
    }

    void processAbstractReturn( llvm::Function * fn ){
        auto insts = query::query( abstractedValues )
                     .map( query::llvmdyncast< llvm::Instruction > )
                     .filter( query::notnull )
                     .filter( [&]( llvm::Instruction * i ) {
                         return i->getParent()->getParent() == fn;
                     } )
                     .freeze();
        for ( auto inst : insts )
            abstractedValues.erase( inst );
        removeInstructions< decltype( insts ) >( insts );

        if ( ! function_store.contains( fn ) ) {
            auto newfn = changeReturnValue( fn, abstractionType );
            function_store.insert( { fn, newfn } );
        }

        for ( auto user : fn->users() ) {
            auto call = llvm::cast< llvm::CallInst >( user );

            llvm::IRBuilder<> irb( call );
            auto newfn = function_store[ fn ];
            std::vector< llvm::Value * > args;
            for ( auto & arg : call->arg_operands() )
                args.push_back( arg );
            auto ncall = irb.CreateCall( newfn, args );
            store( call, ncall );
            for ( auto uuser : user->users() )
                propagateAndProcess( *fn->getParent(), uuser );
        }
    }

    void processAbstractArguments( llvm::Module & m ){
        auto args = query::query( m )
                    .map( []( llvm::Function & f ) {
                        return f.args();
                    } ).flatten()
                    .map( query::refToPtr )
                    .filter( [&] ( llvm::Value * v ) {
                        return isAbstractType( v->getType() );
                    } )
                    .freeze();

        TToVStoreMap< llvm::Function *, llvm::Argument * > funToArgMap;

        for ( const auto &arg : args ) {
            llvm::Function * fn = arg->getParent();
            assert( fn != nullptr );
            if ( fn->hasName() && fn->getName().startswith( "lart." ) )
                continue;
            if ( funToArgMap.contains( fn ) )
                funToArgMap[ fn ].push_back( arg );
            else
                funToArgMap.insert( { fn, { arg } } );
        }

        std::vector< llvm::Function * > unordered;
        for ( auto it : funToArgMap )
            unordered.push_back( it.first );
        auto order = analysis::callPostorder< llvm::Function * >( m, unordered );

        for ( auto & fn : order ) {
            std::vector < llvm::Type * > arg_types;
            for ( auto &a : fn->args() ) {
                auto t = isAbstractType( a.getType() ) ? abstractionType : a.getType();
                arg_types.push_back( t );
            }
            auto rty = isAbstractType( fn->getFunctionType()->getReturnType() )
                     ? abstractionType
                     : fn->getFunctionType()->getReturnType();
            auto fty = llvm::FunctionType::get( rty,
                                                arg_types,
                                                fn->getFunctionType()->isVarArg() );
            auto newfn = cloneFunction( fn, fty );

            assert( !function_store.contains( fn ) );
            function_store.insert( { fn, newfn } );

            for ( auto & arg : funToArgMap[ fn ] ) {
                auto newarg = getArgument( newfn, arg->getArgNo() );
                abstraction_store.insert( { newarg, newarg } );
                propagateAndProcess( m, newarg );
            }
        }
    }

    void propagateAndProcess( llvm::Module & m, llvm::Value * value ) {
        auto deps = analysis::postorder< llvm::Value * >( value );
        for ( auto dep : lart::util::reverse( deps ) )
            if( auto inst = llvm::dyn_cast< llvm::Instruction >( dep ) )
                    process( m, inst );
    }


    void process( llvm::Module &m, llvm::Instruction * inst ) {
        llvmcase( inst,
            [&]( llvm::SelectInst * i ) {
                doSelect( i );
            },
            [&]( llvm::BranchInst * i ) {
                doBranch( i );
            },
			[&]( llvm::PHINode * i ) {
                doPhi( i );
            },
			[&]( llvm::CallInst * call ) {
                auto name =  call->getCalledFunction()->getName();
                if ( name.startswith( "lart.abstract.lift" ) ||
                     name.startswith( "lart.tristate.lift" ) )
                    handleLiftCall( m, call );
                else if ( name.startswith( "lart" ) )
                    handleAbstractCall( m, call );
                else
                    handleGenericCall( m, call );
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

    void handleLiftCall( llvm::Module & m, llvm::CallInst * call ) {
        auto name = getAbstractFunctionName( call->getCalledFunction()->getName() );
        auto fn = m.getFunction( name );

        llvm::IRBuilder<> irb( call );
        auto ncall = irb.CreateCall( fn, { call->getArgOperand( 0 ) } );
        store( call, ncall );

        abstractedTypes.insert( ncall->getType() );
    }

    void handleAbstractCall( llvm::Module & m, llvm::CallInst * call ) {
        auto name = getAbstractFunctionName( call->getCalledFunction()->getName() );
        auto fn = getFunctionWithName( name, m );
        processFunctionCall( fn, call );
    }

    void handleGenericCall( llvm::Module & m, llvm::CallInst * call ) {
        auto name = call->getCalledFunction()->getName();
        auto fn = getFunctionWithName( name, m );
        processFunctionCall( fn, call );
    }

    llvm::Function * getFunctionWithName( llvm::StringRef name, llvm::Module & m ) {
        auto fn = m.getFunction( name );
        if ( function_store.contains( fn ) )
            fn = function_store[ fn ];
        assert ( fn != nullptr );
        return fn;
    }

    void processFunctionCall( llvm::Function * fn, llvm::CallInst * call ) {
        std::vector < llvm::Value * > args;

        for ( auto &arg : call->arg_operands() ) {
            if ( isAbstractType( arg->getType() )
              && abstraction_store.find( arg ) == abstraction_store.end() ) {
                //not all incoming values substituted
                //wait till have all args
                break;
            }
            auto tmp = isAbstractType( arg->getType() )
                     ? abstraction_store[ arg ]
                     : arg;
            args.push_back( tmp );
        }

        //skip if do not have enough substituted arguments
        if ( call->getNumArgOperands() == args.size() ) {
            llvm::IRBuilder<> irb( call );
            auto ncall = irb.CreateCall( fn, args );
            store( call, ncall );
        }
    }

    void doSelect( llvm::SelectInst * i ) {
        auto cond = i->getCondition();
        if ( abstraction_store.contains( cond ) ) {
            cond = abstraction_store[ cond ];
            auto tv = abstraction_store.contains( i->getTrueValue() )
                    ? abstraction_store[ i->getTrueValue() ]
                    : nullptr;
            auto fv = abstraction_store.contains( i->getFalseValue() )
                    ? abstraction_store[ i->getFalseValue() ]
                    : nullptr;
            if ( tv == nullptr || fv == nullptr )
                return;
            llvm::IRBuilder<> irb( i );
            auto newsel = irb.CreateSelect( cond, tv, fv );
            store( i, newsel );
        }
    }

    void doBranch( llvm::BranchInst * i ) {
        assert( i->isConditional() );
        if ( abstraction_store.contains( i ) )
            return;
        llvm::IRBuilder<> irb( i );
        auto cond = i->getCondition();
        if ( abstraction_store.contains( cond ) ) {
            cond = abstraction_store[ cond ];
            auto tbb = i->getSuccessor( 0 );
            auto fbb = i->getSuccessor( 1 );
            auto newbr = irb.CreateCondBr( cond, tbb, fbb );
            store( i, newbr );
        }
    }

    void doPhi( llvm::PHINode * phi ) {
        unsigned int niv = phi->getNumIncomingValues();

        std::vector< std::pair< llvm::Value *, llvm::BasicBlock * > > incoming;
        for ( unsigned int i = 0; i < niv; ++i ) {
            auto val = llvm::cast< llvm::Value >( phi->getIncomingValue( i ) );
            auto parent = phi->getIncomingBlock( i );
            if ( abstraction_store.contains( val ) )
                incoming.push_back( { abstraction_store[ val ], parent } );
            else {
                if ( isAbstractedType( val->getType() ) )
                    incoming.push_back( { val, parent } );
            }
        }

        if ( incoming.size() > 0 ) {
            llvm::PHINode * node = nullptr;
            if ( abstraction_store.contains( phi ) )
                node = llvm::cast< llvm::PHINode >( abstraction_store[ phi ] );
            else {
                llvm::IRBuilder<> irb( phi );
                auto type = incoming.begin()->first->getType();
                node = irb.CreatePHI( type, niv );
                store( phi, node );
            }

            for ( size_t i = 0; i < node->getNumIncomingValues(); ++i )
                node->removeIncomingValue( i,  false );
            for ( auto & in : incoming )
                node->addIncoming( in.first, in.second );
        }
    }

    void doReturn( llvm::ReturnInst * i ) {
        if ( abstraction_store.contains( i->getReturnValue() ) ) {
            llvm::IRBuilder<> irb( i );
            auto arg = abstraction_store[ i->getReturnValue() ];
            auto ret = irb.CreateRet( arg );
            store( i, ret );
        }
    }

    template< typename Container >
    void removeInstructions( Container & insts ) {
        for ( auto &inst : insts )
            inst->replaceAllUsesWith( llvm::UndefValue::get( inst->getType() ) );
        for ( auto &inst : insts ) {
            auto stored = abstraction_store.find( inst );
            if ( stored != abstraction_store.end() )
                abstraction_store.erase( stored );
            inst->eraseFromParent();
        }
    }

    void removeAbstractDeclarations( llvm::Module & m ) {
        auto toErase = query::query( m )
                        .map( query::refToPtr )
                        .filter( []( llvm::Function * fn ) {
                            return isAbstractDeclaration( fn );
                        } ).freeze();
        for ( auto &fn : toErase )
            fn->eraseFromParent();
    }

    void store( llvm::Value * val, llvm::Value * newval ) {
        abstraction_store[ val ] = newval;
        abstractedValues.insert( llvm::cast< llvm::Instruction >( val ) );
    }

    static bool isAbstractDeclaration( llvm::Function * fn ) {
        return fn->getName().startswith( "lart.abstract" )
            || fn->getName().startswith( "lart.tristate" );
    }

    //helpers
    llvm::Argument * getArgument( llvm:: Function * fn, size_t index ) {
        auto it = fn->arg_begin();
        for ( unsigned i = 0; i < index; ++i )
            ++it;
        return &(*it);
    }

    std::string getAbstractFunctionName( llvm::StringRef callName, bool pointer = false ) {
        llvm::SmallVector< llvm::StringRef, 4 > nameParts;
        callName.split( nameParts, "." );
        auto suffix = callName.endswith( "*" ) ? "_p" : "";
        if ( nameParts[ 1 ].startswith( "tristate" ) )
            return "__abstract_tristate_" + nameParts[ 2 ].str() + suffix;
        if ( nameParts[ 2 ].startswith( "icmp" ) )
            return abstractionName + "icmp_" + nameParts[ 3 ].str() + suffix;
        if ( nameParts[ 2 ].startswith( "lift" ) )
            return abstractionName + nameParts[ 2 ].str() + suffix + "_" + nameParts[ 3 ].str();
        if ( nameParts[ 2 ].startswith( "lower" ) )
            return abstractionName + nameParts[ 2 ].str() + suffix + "_" + nameParts[ 3 ].str();
        else
            return abstractionName + nameParts[ 2 ].str() + suffix;
    }

    template < typename Value, typename Filter >
    auto identify( llvm::Module &m, Filter filter ) -> std::vector< Value * > {
         return query::query( m ).flatten().flatten()
                      .map( query::refToPtr )
                      .map( query::llvmdyncast< Value > )
                      .filter( query::notnull )
                      .filter( filter )
                      .freeze();
    }

    bool isAbstractedType( llvm::Type * type ) {
        return abstractedTypes.find( type ) != abstractedTypes.end();
    }

    bool isAbstractType( llvm::Type * t ) {
        auto typeName = lart::abstract::getTypeName( t );
        std::string prefix = "%lart";
        return !typeName.compare( 0, prefix.size(), prefix );
    }

private:
    template < typename T, typename V >
    using TToVStoreMap = lart::util::Store< std::map< T, std::vector< V  > > >;

    template < typename V >
    using AbstractStore = lart::util::Store< std::map< V, V > >;

    // substituted values
    AbstractStore< llvm::Value * > abstraction_store;

	// substituted functions
    AbstractStore < llvm::Function * > function_store;

    std::set< llvm::Instruction * > abstractedValues;

    std::set < llvm::Type * > abstractedTypes;

    std::string abstractionName;
    llvm::Type * abstractionType;
};*/

} // namespace abstract
} // namespace lart
