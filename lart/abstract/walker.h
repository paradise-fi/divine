// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
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
#include <lart/abstract/walkergraph.h>

namespace lart {
namespace abstract {

namespace {

template < typename K, typename V >
using Map = std::map< K, V >;

template < typename V >
using Set = std::unordered_set< V >;

static llvm::StringRef getAnnotationType( const llvm::CallInst * call ) {
	return llvm::cast< llvm::ConstantDataArray > ( llvm::cast< llvm::GlobalVariable > (
		   call->getOperand( 1 )->stripPointerCasts() )->getInitializer() )->getAsCString();
}

static bool isAbstractAnnotation( const llvm::CallInst * call ) {
	return getAnnotationType( call ).startswith( "lart.abstract" );
}

static ValueNode getAnnotation( llvm::CallInst * call ) {
	assert( isAbstractAnnotation( call ) );
	return { call->getOperand( 0 )->stripPointerCasts(), getAnnotationType( call ) };
}

static std::vector< ValueNode > getAbstractAnnotations( llvm::Module * m ) {
    std::vector< std::string > annotationFunctions = {
        "llvm.var.annotation",
        "llvm.ptr.annotation"
    };

    std::vector< ValueNode > annotations;
    for ( const auto & a : annotationFunctions ) {
        for ( auto & f : m->functions() ) {
            if ( f.getName().startswith( a ) )
            for ( auto user : f.users() ) {
                auto call = llvm::cast< llvm::CallInst >( user );
                if ( isAbstractAnnotation( call ) )
                    annotations.emplace_back( getAnnotation( call ) );
            }
        }
    }
	return annotations;
}
} // anonymous namespace

template< typename Preprocess, typename Node >
struct Walker {
    using Nodes = std::vector< Node >;

    Walker( Preprocess preprocess, llvm::Module & m ) :
        preprocess( preprocess )
    {
        compute_nodes( m );
    }

    Nodes postorder() {
        return analysis::postorder< Node >( _nodes, [] ( const Node & n ) -> Nodes {
            return n->succs;
        } );
    }

private:
    using Value = llvm::Value;
    using Function = llvm::Function;
    using ValueNodes = std::vector< ValueNode >;
    using Entries = FunctionNode::Entries;

    // Computes the functions for abstraction
    void compute_nodes( llvm::Module & m ) {
        Nodes queue = annotated( m );
        for ( auto & n : queue )
            _reachednodes.insert( n );

        while ( !queue.empty() ) {
            auto current = queue.back();
            queue.pop_back();

            auto deps = process( current );
            queue.insert( queue.end(), std::make_move_iterator( deps.begin() )
                                     , std::make_move_iterator( deps.end() ) );
        }
    }

    Nodes process( Node & node ) {
        auto find = std::find_if( _nodes.begin(), _nodes.end(), [&] ( const Node & n ) {
            return node == n;
        } );
        if ( find != _nodes.end() )
            return {};

        bool called = query::query( node->entries ).all( [] ( const ValueNode & n ) {
            return llvm::isa< llvm::Argument >( n.value );
        } );

        auto fn = node->function;
        if ( query::query( _nodes ).none( [&]( Node n ) { return n->function == fn; } ) )
            preprocess( fn );

        do {
            auto allocas = abstract_allocas( node );
            node->entries.insert( allocas.begin(), allocas.end() );
        } while ( propagate_through_calls( node ) );

        _nodes.push_back( node );

        Nodes deps;
        if ( !called && returns_abstract( node ) )
            for ( auto & dep : calls_of( node ) ) {
                _reachednodes.insert( dep );
                deps.push_back( dep );
            }

        return deps;
    }

    auto abstract_args_of_call( const Node & node, const llvm::CallInst * call ) {
        std::vector< size_t > args_idx;
        for ( size_t i = 0; i < call->getNumArgOperands(); ++i ) {
            bool abstract = query::query( node->postorder() )
                .any( [&] ( const ValueNode & vn ) {
                    return vn.value == call->getArgOperand( i );
                } );
            if ( abstract ) {
                args_idx.push_back( i );
            }
        }
        return args_idx;
    }

    bool propagate_through_calls( Node & node ) {
        bool newentry = false;
        auto calls = query::query( node->postorder() )
            .filter( []( const ValueNode & n ) {
                return llvm::isa< llvm::CallInst >( n.value );
            } )
            .filter( []( const ValueNode & n ) {
                auto call = llvm::cast< llvm::CallInst >( n.value );
                return !call->getCalledFunction()->isIntrinsic();
            } )
            .freeze();

        auto CreateFunctionNode = [&]( const ValueNode & n ) {
                auto call = llvm::cast< llvm::CallInst >( n.value );
                auto fn = call->getCalledFunction();
                auto args_idx = abstract_args_of_call( node, call );
                Entries e;
                for ( auto a : args_idx )
                    e.emplace( std::next( fn->arg_begin(), a ), n.annotation );
                return std::make_shared< FunctionNode >( fn, e );
        };

        for ( auto & call : calls ) {
            auto fn = CreateFunctionNode( call );
            auto deps = reached( fn );
            if ( deps.empty() ) {
                _reachednodes.insert( fn );
                process( fn );
                deps.push_back( fn );
            }

            for ( auto & dep : deps ) {
                auto succ = query::query( node->succs ).any( [&] ( auto & s ) {
                    return s == dep;
                } );
                if ( !succ )
                    node->succs.push_back( dep );
                // TODO solve indirect recursion
                if ( returns_abstract( dep ) )
                    newentry |= node->entries.emplace( call.value, call.annotation, true ).second;
            }
        }
        return newentry;
    }

    Nodes annotated( llvm::Module & m ) {
        auto annotations = getAbstractAnnotations( &m );
        return query::query( annotations )
            .map( []( const ValueNode & ann ) {
                auto inst = llvm::dyn_cast< llvm::Instruction >( ann.value );
                return inst->getParent()->getParent();
            } )
            .unstableUnique()
            .map( [&]( Function * fn ) {
                auto values = query::query( annotations )
                    .filter( [&]( const ValueNode & n ) {
                        const auto & inst = llvm::dyn_cast< llvm::Instruction >( n.value );
                        return inst->getParent()->getParent() == fn;
                    } )
                    .map( []( const ValueNode & n ) {
                        return ValueNode{ n.value, n.annotation, true };
                    } ).freeze();

                preprocess( fn );
                Entries entries{ values.begin(), values.end() };

                return std::make_shared< FunctionNode >( fn, entries );
            } ).freeze();
    }

    Entries abstract_allocas( const Node & node ) {
        Entries reached = { node->entries };
        Entries abstract;

        auto allocas = query::query( *node->function ).flatten()
            .map( query::refToPtr )
            .filter( []( llvm::Value * v ) { return llvm::isa< llvm::AllocaInst >( v ); } )
            .map( []( llvm::Value * v ) { return ValueNode{ v, "alloca", true }; } )
            .freeze();

        do {
            abstract = reached;
            ValueNodes nodes = { reached.begin(), reached.end() };
            auto postorder = analysis::postorder< ValueNode >( nodes, value_succs );
            for ( auto & a : allocas ) {
                auto stores = query::query( postorder )
                    .filter( [&] ( const ValueNode & n ) {
                        if ( auto s = llvm::dyn_cast< llvm::StoreInst >( n.value ) )
                            return s->getPointerOperand() == a.value;
                        return false;
                    } ).freeze();
                auto loads = query::query( postorder )
                    .filter( [&] ( const ValueNode & n ) {
                        if ( auto l = llvm::dyn_cast< llvm::LoadInst >( n.value ) )
                            return l->getPointerOperand() == a.value;
                        return false;
                    } ).freeze();

                if ( loads.size() || stores.size() ) {
                    a.annotation = loads.size() ? loads[0].annotation : stores[0].annotation;
                    reached.insert( a );
                }
            }
        } while ( abstract != reached );
        return abstract;
    }

    Nodes reached( const FunctionNodePtr & node ) {
        return query::query( _reachednodes )
            .filter( [&] ( const Node & n ) {
                if ( n->function != node->function )
                        return false;
                // check whether entries match
                auto args = query::query( n->entries )
                    .filter( []( const ValueNode & v ) {
                        return llvm::dyn_cast< llvm::Argument >( v.value );
                    } ).freeze();
                if ( args.size() != node->entries.size() )
                    return false;
                return query::query( node->entries ).all( [&] ( const ValueNode & e ) {
                    return std::find( args.begin(), args.end(), e ) != args.end();
                } );
        } ).freeze();
    }

    bool returns_abstract( const Node & node ) {
        for ( const auto & n : node->postorder() )
            if ( llvm::dyn_cast< llvm::ReturnInst >( n.value ) )
                return true;
        return false;
    }

    ValueNode get_return( const Node & node ) {
        for ( const auto & n : node->postorder() )
            if ( llvm::dyn_cast< llvm::ReturnInst >( n.value ) )
                return n;
        UNREACHABLE( "Function without abstract return." );
    }

    Nodes calls_of( const Node & node ) {
        Nodes ns;
        auto ret = get_return( node );
        Map< Function *, std::vector< llvm::CallInst * > > users;
        for ( auto user : node->function->users() ) {
            auto call = llvm::cast< llvm::CallInst >( user );
            auto fn = call->getParent()->getParent();
            users[ fn ].push_back( call );
        }

        for ( const auto & user : users ) {
            // Compute already reached predecessors of given user function
            auto preds = query::query( _reachednodes )
                .filter( [&] ( const Node & n ) { return n->function == user.first; } ).freeze();
            preds.emplace_back( new FunctionNode{ user.first, {} } );

            bool called = query::query( node->entries ).any( [] ( const ValueNode & n ) {
                return llvm::isa< llvm::Argument >( n.value );
            } );

            for ( const auto & p : preds ) {
                for ( const auto & e : user.second ) {
                    bool has_entry = query::query( p->entries ).any( [&] ( const ValueNode & n ) {
                        return n.value == e;
                    } );
                    if ( has_entry )
                        continue;
                    // If this entry is need to be called - test whether it is reachable from
                    // other entries of function else add this entry to node directly
                    bool reachable = false;
                    if ( called ) {
                       reachable = query::query( p->postorder() )
                        .filter( [] ( const ValueNode & n ) {
                                return llvm::isa< llvm::CallInst >( n.value );
                        } )
                        .any( [&] ( const ValueNode & n ) {
                            auto call = llvm::cast< llvm::CallInst >( n.value );
                            return call->getCalledFunction() == node->function;
                        } );
                    }
                    if ( !called || reachable ) {
                        p->entries.emplace( e, ret.annotation, true );
                        ns.push_back( p );
                    }
                }
            }
        }
        return ns;
    }

    const Preprocess preprocess;
    Nodes _nodes;
    Set< Node > _reachednodes;
};


template< typename Preprocess >
using AbstractWalker = Walker< Preprocess, FunctionNodePtr >;

} // namespace abstract
} // namespace lart
