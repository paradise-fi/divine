// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/walker.h>

DIVINE_RELAX_WARNINGS
#include <llvm/ADT/Hashing.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>
DIVINE_UNRELAX_WARNINGS

#include <string>
#include <vector>
#include <utility>
#include <algorithm>

#include <lart/support/util.h>
#include <lart/support/query.h>
#include <lart/analysis/postorder.h>
#include <lart/abstract/value.h>
#include <lart/abstract/types/common.h>

using namespace lart;
using namespace lart::abstract;

template < typename K, typename V >
using Map = std::map< K, V >;

template < typename V >
using Set = std::unordered_set< V >;

using AbstractValues = std::vector< AbstractValue >;

template< typename I >
auto Insts( std::vector< llvm::Value * > values ) {
    return query::query( values )
        .map( query::llvmdyncast< I > )
        .filter( query::notnull )
        .freeze();
};

template< typename I >
auto Insts( llvm::Function * fn ) {
    return query::query( *fn ).flatten()
        .map( query::refToPtr )
        .map( query::llvmdyncast< I > )
        .filter( query::notnull )
        .freeze();
};


struct ValuesPostOrder {
    ValuesPostOrder( const AbstractValues & roots )
        : roots( roots ) {}

    auto succs() const {
        return [&] ( const AbstractValue& n ) -> AbstractValues {
            // do not propagate through calls that does not roots
            // ie. that are those calls which does not return abstract value
            bool root = std::find( roots.begin(), roots.end(), n ) != roots.end();
            if ( llvm::isa< llvm::CallInst >( n.value() ) && !root )
                return {};

            return query::query( n.value()->users() )
                .map( [&] ( const auto & val ) {
                    return AbstractValue{ val, n.domain() };
                } )
                .freeze();
        };
    }

    AbstractValues get() const {
        return lart::analysis::postorder( roots, succs() );
    }

    const AbstractValues & roots;
};


// FunctionNode
AbstractValues FunctionNode::postorder() const {
    AbstractValues unordered = { origins.begin(), origins.end() };
    return ValuesPostOrder( unordered ).get();
}

// Walker
std::vector< FunctionNodePtr > Walker::postorder() const {
    auto succs = [] ( const FunctionNodePtr& f ) { return f->succs; };
    return analysis::postorder( functions, succs );
}


Maybe< AbstractValue > getValueFromAnnotation( const llvm::CallInst * call ) {
    auto data = llvm::cast< llvm::GlobalVariable >(
                call->getOperand( 1 )->stripPointerCasts() );
    auto annotation = llvm::cast< llvm::ConstantDataArray >(
                      data->getInitializer() )->getAsCString();
    const std::string prefix = "lart.abstract.";
    if ( annotation.startswith( prefix ) ) {
        auto tmp = annotation.str().substr( prefix.size() );
        auto domain = Domain::value( annotation.str().substr( prefix.size() ) );
        if ( domain.isNothing() )
            throw std::runtime_error( "unknown abstraction domain" );

        return Maybe< AbstractValue >::Just( AbstractValue(
                                             call->getOperand( 0 )->stripPointerCasts(),
                                             domain.value() ) );
    }

    return Maybe< AbstractValue >::Nothing();
}

std::vector< FunctionNodePtr > Walker::annotations( llvm::Module & m ) {
    std::vector< std::string > prefixes = {
        "llvm.var.annotation",
        "llvm.ptr.annotation"
    };

    AbstractValues values;

    for ( const auto & pref : prefixes ) {
        auto annotated = query::query( m )
            .map( query::refToPtr )
            .filter( [&]( llvm::Function * fn ) {
                return fn->getName().startswith( pref );
            } ).freeze();

        for ( auto a : annotated ) {
            std::vector< llvm::Value * > users = { a->user_begin(), a->user_end() };
            for ( const auto & call : Insts< llvm::CallInst >( users ) ) {
                auto val = getValueFromAnnotation( call );
                if ( val.isJust() )
                    values.push_back( val.value() );
            }
        }
    }

    return query::query( values )
        .map( []( const auto & v ) {
            return llvm::cast< llvm::Instruction >( v.value() )
                   ->getParent()->getParent();
        } )
        .unstableUnique()
        .map( [&]( llvm::Function * fn ) {
            auto owned = query::query( values )
                .filter( [&]( const auto & n ) {
                    const auto & inst =
                        llvm::dyn_cast< llvm::Instruction >( n.value() );
                    return inst->getParent()->getParent() == fn;
                } )
                .freeze();

            preprocess( fn );
            Set< AbstractValue > origins{ owned.begin(), owned.end() };

            return std::make_shared< FunctionNode >( fn, origins );
        } ).freeze();
}

void Walker::init( llvm::Module & m ) {
    auto queue = annotations( m );
    seen.insert( queue.begin(), queue.end() );

    while ( !queue.empty() ) {
        auto current = queue.back();
        queue.pop_back();

        auto deps = process( current );
        queue.insert( queue.end(), deps.begin(), deps.end() );
    }
}

// Write access to abstract value is produced by store:
// void store %abstract_value, %value*
// we need %abstract_value type information for correct propagation of domains
using WriteAccess = std::pair< llvm::StoreInst *, AbstractTypePtr >;


// Propagate abstract value through write and read accesses
// 1. whenever some obstract value is stored to some alloca we mark this alloca
// as abstract origin, same with storing to value obtained by GEP
Set< AbstractValue > abstractOrigins( const FunctionNodePtr & processed ) {
    Set< AbstractValue > reached = { processed->origins };
    Set< AbstractValue > abstract;

    auto allocas = query::query( Insts< llvm::AllocaInst >( processed->function ) )
        .map( []( llvm::Value * v ) {
            return AbstractValue{ v, LLVMDomain };
        } ).freeze();
    auto geps = query::query( Insts< llvm::GetElementPtrInst >( processed->function ) )
        .map( []( llvm::Value * v ) {
            return AbstractValue{ v, LLVMDomain };
        } ).freeze();

    auto writes = [] ( const auto & roots ) {
        std::vector< WriteAccess > accs;
        for ( const auto & av : ValuesPostOrder( roots ).get() ) {
            // Propagate only through root calls, because they return abstract values
            if ( llvm::isa< llvm::CallInst >( av.value() ) ) {
                if ( std::find( roots.begin(), roots.end(), av ) == roots.end() )
                    continue;
            }
            auto stores = query::query( av.value()->users() )
                .map( query::llvmdyncast< llvm::StoreInst > )
                .filter( query::notnull )
                .filter( [&] ( llvm::StoreInst * store ) {
                    return store->getValueOperand() == av.value();
                } ).freeze();
            for ( const auto & store : stores )
                accs.emplace_back( store, av.type() );
        }
        return accs;
    };

    auto MapAccessed = [&] ( auto & insts, auto & accesses, auto functor ) {
        for ( auto & i : insts ) {
            auto instAccesses = query::query( accesses )
                .filter( [&] ( const auto & accs ) {
                    return accs.first->getPointerOperand() == i.value();
                } ).freeze();
            if ( instAccesses.size() )
                functor( i , instAccesses[0] );
        }
    };

    do {
        abstract = reached;
        // TODO do not propagate nonabstract elements of struct
        AbstractValues unordered( reached.begin(), reached.end() );
        auto accesses = writes( unordered );
        MapAccessed( allocas, accesses,
            [&] ( const auto & n, const auto & accs ) {
                // TODO compute struct domain
                if ( n.value()->getType()->isStructTy() ) {
                    DomainMap dmap = { { 0, accs.second->domain() } };
                    auto val = AbstractValue( n.value(), dmap );
                    reached.insert( val );
                } else {
                    auto val = AbstractValue( n.value(), accs.second->domain() );
                    reached.insert( val );
                }
            } );

        MapAccessed( geps, accesses,
            [&] ( auto & n , const auto & accs ) {
                auto gep = llvm::cast< llvm::GetElementPtrInst >( n.value() );
                auto av = AbstractValue( gep->getPointerOperand(), accs.second->domain() );
                reached.insert( av );
            } );
    } while ( abstract != reached );

    return abstract;
}

AbstractValues abstractArgs( const FunctionNodePtr & processed, const AbstractValue & vn ) {
    auto call = llvm::cast< llvm::CallInst >( vn.value() );
    auto fn = call->getCalledFunction();
    auto abstract = processed->postorder();

    AbstractValues res;
    for ( size_t i = 0; i < call->getNumArgOperands(); ++i ) {
        auto find = std::find_if( abstract.begin(), abstract.end(),
            [&] ( const auto & vn ) {
                return vn.value() == call->getArgOperand( i );
            } );
        if ( find != abstract.end() )
            res.emplace_back( std::next( fn->arg_begin(), i ), find->domain() );
    }
    return res;
}

std::vector< FunctionNodePtr > Walker::dependencies( const FunctionNodePtr & processed ) {
    return query::query( seen )
    .filter( [&] ( const auto & n ) {
        if ( n->function != processed->function )
                return false;

        auto args = query::query( n->origins )
            .filter( []( const auto & o ) {
                return llvm::dyn_cast< llvm::Argument >( o.value() );
            } ).freeze();

        if ( args.size() != processed->origins.size() )
            return false;

        return query::query( processed->origins )
            .all( [&] ( const auto & o ) {
                return std::find( args.begin(), args.end(), o ) != args.end();
            } );
    } ).freeze();
}

Maybe< AbstractValue > getRet( const FunctionNodePtr & processed ) {
    for ( const auto & n : processed->postorder() )
        if ( llvm::dyn_cast< llvm::ReturnInst >( n.value() ) )
            return Maybe< AbstractValue >::Just( n );
    return Maybe< AbstractValue >::Nothing();
}

bool Walker::propagateThroughCalls( FunctionNodePtr & processed ) {
    bool newentry = false;

    auto calls = query::query( processed->postorder() )
        .filter( []( const auto & n ) {
            if ( auto call = llvm::dyn_cast< llvm::CallInst >( n.value() ) )
                return !call->getCalledFunction()->isIntrinsic();
            return false;
        } )
        .freeze();

    for ( auto & call : calls ) {
        AbstractValues args = abstractArgs( processed, call );
        Set< AbstractValue > origins( args.begin(), args.end() );
        auto fn = std::make_shared< FunctionNode >(
                  llvm::cast< llvm::CallInst >( call.value() )->getCalledFunction(),
                  origins );

        auto deps = dependencies( fn );
        if ( deps.empty() ) {
            seen.insert( fn );
            process( fn );
            deps.push_back( fn );
        }

        for ( auto & dep : deps ) {
            bool succ = query::query( processed->succs ).any( [&] ( auto & s ) {
                return s == dep;
            } );
            if ( !succ )
                processed->succs.push_back( dep );
            // TODO solve indirect recursion
            if ( getRet( dep ).isJust() )
                newentry |= processed->origins.emplace( call ).second;
        }
    }

    return newentry;
}

std::vector< FunctionNodePtr > Walker::callsOf( const FunctionNodePtr & processed ) {
    auto ret = getRet( processed ).value();
    Map< llvm::Function *, std::vector< llvm::CallInst * > > users;
    for ( auto user : processed->function->users() ) {
        if ( auto call = llvm::dyn_cast< llvm::CallInst >( user ) ) {
            auto fn = call->getParent()->getParent();
            users[ fn ].push_back( call );
        }
    }

    std::vector< FunctionNodePtr > res;
    for ( const auto & user : users ) {
        // Get already seen predecessors of given user function
        auto preds = query::query( seen )
            .filter( [&] ( const auto & n ) {
                return n->function == user.first;
            } ).freeze();

        preds.emplace_back( new FunctionNode( user.first, {} ) );

        bool called = query::query( processed->origins )
            .any( [] ( const auto & o ) {
                return llvm::isa< llvm::Argument >( o.value() );
            } );

        for ( const auto & p : preds ) {
            for ( const auto & e : user.second ) {
                bool has_entry = query::query( p->origins )
                    .any( [&] ( const auto & o ) {
                        return o.value() == e;
                    } );

                if ( has_entry )
                    continue;
                // If this entry is need to be called - test whether it is reachable from
                // other origins of function else add this entry to node directly
                bool reachable = false;
                if ( called ) {
                    reachable = query::query( p->postorder() )
                        .filter( [] ( const auto & n ) {
                            return llvm::isa< llvm::CallInst >( n.value() );
                        } )
                        .any( [&] ( const auto & n ) {
                            const auto& call = llvm::cast< llvm::CallInst >( n.value() );
                            return call->getCalledFunction() == processed->function;
                        } );
                }

                if ( !called || reachable ) {
                    p->origins.emplace( e, ret.domain() );
                    res.push_back( p );
                }
            }
        }
    }
    return res;
}

std::vector< FunctionNodePtr > Walker::process( FunctionNodePtr & processed ) {
    if ( std::find( functions.begin(), functions.end(), processed ) != functions.end() )
        return {};

    bool called = query::query( processed->origins ).all( [] ( const auto & o ) {
        return llvm::isa< llvm::Argument >( o.value() );
    } );

    bool preprocessed = query::query( functions ).any( [&]( const auto & fn ) {
        return fn->function == processed->function;
    } );

    if ( !preprocessed )
        preprocess( processed->function );

    do {
        auto origins = abstractOrigins( processed );
        processed->origins.insert( origins.begin(), origins.end() );
    } while ( propagateThroughCalls( processed ) );

    functions.push_back( processed );

    std::vector< FunctionNodePtr > deps;
    auto ret = getRet( processed );
    if ( !called && ret.isJust() ) {
        for ( auto & dep : callsOf( processed ) ) {
            seen.insert( dep );
            deps.push_back( dep );
        }
    }

    return deps;
}
