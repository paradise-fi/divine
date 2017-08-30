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
#include <iterator>

#include <lart/support/util.h>
#include <lart/support/query.h>
#include <lart/analysis/postorder.h>
#include <lart/abstract/value.h>
#include <lart/abstract/util.h>

namespace lart {
namespace abstract {

struct ValuesPostOrder {
    ValuesPostOrder( const AbstractValues & roots )
        : roots( roots ) {}

    auto succs() const {
        return [&] ( const AbstractValue& n ) -> AbstractValues {
            // do not propagate through calls that are not in roots
            // i.e. that are those calls which do not return an abstract value
            bool root = std::find( roots.begin(), roots.end(), n ) != roots.end();
            if ( n.isa< llvm::CallInst >() && !root )
                return {};
            if ( !n.isAbstract() )
                return {};

            return query::query( n.value->users() )
            .map( [&] ( const auto & val ) -> AbstractValue {
                return { val, n.domain };
            } ).freeze();
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

AbstractValue getValueFromAnnotation( const llvm::CallInst * call ) {
    auto data = llvm::cast< llvm::GlobalVariable >(
                call->getOperand( 1 )->stripPointerCasts() );
    auto annotation = llvm::cast< llvm::ConstantDataArray >(
                      data->getInitializer() )->getAsCString();
    const std::string prefix = "lart.abstract.";
    assert( annotation.startswith( prefix ) );
    auto dom = DomainTable[ annotation.str().substr( prefix.size() ) ];
    return AbstractValue( call->getOperand( 0 )->stripPointerCasts(), dom );
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
            for ( const auto & call : llvmFilter< llvm::CallInst >( users ) )
                values.push_back( getValueFromAnnotation( call ) );
        }
    }

    return query::query( values )
        .map( []( const auto & v ) { return v.getFunction(); } )
        .unstableUnique()
        .map( [&]( llvm::Function * fn ) {
            auto owned = query::query( values )
                .filter( [&]( const auto & n ) {
                    return n.getFunction() == fn;
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

using StoreAccess = std::pair< llvm::StoreInst *, Domain >;
using Accesses = std::vector< StoreAccess >;

template< typename Roots >
Accesses abstractStores( const Roots & roots ) {
    Accesses accs;
    for ( const auto & av : ValuesPostOrder( roots ).get() ) {
        // Do not propagate through calls that do not return an abstract value
        // i.e. are not roots.
        if ( av.isa< llvm::CallInst >() &&
             std::find( roots.begin(), roots.end(), av ) == roots.end() )
                continue;
        auto stores = query::query( llvmFilter< llvm::StoreInst >( av.value->users() ) )
            .filter( [&] ( llvm::StoreInst * store ) {
                return store->getValueOperand() == av.value;
            } ).freeze();
        for ( const auto & store : stores )
            accs.emplace_back( store, av.domain );
    }
    return accs;
}

size_t GEPIdx( llvm::GetElementPtrInst * gep, size_t idx ) {
    return llvm::cast< llvm::ConstantInt >( ( gep->idx_begin() + idx )->get() )->getZExtValue();
}

ValueField< llvm::Value * > createValueField( llvm::GetElementPtrInst * gep ) {
    Indices inds;
    llvm::Value * curr = gep;
    while( auto gep = llvm::dyn_cast< llvm::GetElementPtrInst >( curr ) ) {
        assert( gep->getNumIndices() == 2 );
        inds.push_back( GEPIdx( gep, 1 ) );
        curr = gep->getPointerOperand();
    }
    std::reverse( inds.begin(), inds.end() );
    return { curr, inds };
}

// Propagate abstract value through stores to allocas.
// Whenever some obstract value is stored to some alloca we mark this alloca
// as abstract origin, same with GEP instruction.
Set< AbstractValue > Walker::origins( const FunctionNodePtr & processed ) {
    Set< AbstractValue > reached = { processed->origins };
    Set< AbstractValue > abstract;

    auto loads = llvmFilter< llvm::LoadInst >( processed->function );

    do {
        abstract = reached;
        auto stores = abstractStores( AbstractValues{ reached.begin(), reached.end() } );
        for ( const auto & s : stores ) {
            auto po = s.first->getPointerOperand();
            if ( auto a = llvm::dyn_cast< llvm::AllocaInst >( po ) )
                reached.emplace( a, s.second );
            if ( auto gep = llvm::dyn_cast< llvm::GetElementPtrInst >( po ) ) {
                fields.insert( createValueField( gep ), s.second );
                reached.emplace( gep, s.second );
            }
        }

        for ( const auto & l : loads ) {
            auto po = l->getPointerOperand();
            if ( auto gep = llvm::dyn_cast< llvm::GetElementPtrInst >( po ) )
                if  ( auto dom = fields.getDomain( createValueField( gep ) ) )
                    reached.emplace( gep, dom.value() );
        }
    } while ( abstract != reached );
    return abstract;
}

AbstractValues abstractArgs( const FunctionNodePtr & processed, const AbstractValue & vn ) {
    auto call = vn.get< llvm::CallInst >();
    auto fn = call->getCalledFunction();
    auto abstract = processed->postorder();

    AbstractValues res;
    for ( size_t i = 0; i < call->getNumArgOperands(); ++i ) {
        auto find = std::find_if( abstract.begin(), abstract.end(),
            [&] ( const auto & vn ) {
                return vn.value == call->getArgOperand( i );
            } );
        if ( find != abstract.end() )
            res.emplace_back( std::next( fn->arg_begin(), i ), find->domain );
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
                return o.template safeGet< llvm::Argument >();
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
        if ( n.isa< llvm::ReturnInst >() )
            return Maybe< AbstractValue >::Just( n );
    return Maybe< AbstractValue >::Nothing();
}

bool Walker::propagateThroughCalls( FunctionNodePtr & processed ) {
    bool newentry = false;

    auto calls = query::query( processed->postorder() )
        .filter( []( const auto & n ) {
            if ( auto call = n.template safeGet< llvm::CallInst >() )
                return !call->getCalledFunction()->isIntrinsic();
            return false;
        } )
        .freeze();

    for ( auto & call : calls ) {
        AbstractValues args = abstractArgs( processed, call );
        Set< AbstractValue > origins( args.begin(), args.end() );
        auto fn = std::make_shared< FunctionNode >(
                  call.get< llvm::CallInst >()->getCalledFunction(),
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
            auto fn = getFunction( call );
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
                return o.template isa< llvm::Argument >();
            } );

        for ( const auto & p : preds ) {
            for ( const auto & e : user.second ) {
                bool has_entry = query::query( p->origins )
                    .any( [&] ( const auto & o ) {
                        return o.value == e;
                    } );

                if ( has_entry )
                    continue;
                // If this entry needs to be called - test whether it is reachable from
                // other origins of function else add this entry to node directly
                bool reachable = false;
                if ( called ) {
                    reachable = query::query( p->postorder() )
                        .filter( [] ( const auto & n ) {
                            return n.template isa< llvm::CallInst >();
                        } )
                        .any( [&] ( const auto & n ) {
                            const auto& call = n.template get< llvm::CallInst >();
                            return call->getCalledFunction() == processed->function;
                        } );
                }

                if ( !called || reachable ) {
                    p->origins.emplace( e, ret.domain );
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
        return o.template isa< llvm::Argument >();
    } );

    bool preprocessed = query::query( functions ).any( [&]( const auto & fn ) {
        return fn->function == processed->function;
    } );

    if ( !preprocessed )
        preprocess( processed->function );

    do {
        auto orgs = origins( processed );
        processed->origins.insert( orgs.begin(), orgs.end() );
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

Maybe< AbstractValue > FunctionNode::isOrigin( llvm::Value * v ) const {
    auto find = std::find_if( origins.begin(), origins.end(), [&] ( const auto & o ) {
        return o.value == v;
    } );

    if ( find != origins.end() )
        return Maybe< AbstractValue >::Just( *find );
    return Maybe< AbstractValue >::Nothing();
}

} // namespace abstract
} // namespace lart
