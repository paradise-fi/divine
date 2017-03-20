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

template < typename K, typename V >
using Map = std::map< K, V >;

template < typename V >
using Set = std::unordered_set< V >;

namespace {

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
}

template< typename Preprocess, typename Node >
struct Walker {
    using Nodes = std::vector< Node >;

    Walker( Preprocess preprocess, llvm::Module & m ) :
        preprocess( preprocess ), nodes( get_nodes( m ) ) {}

    Nodes postorder() {
        return analysis::postorder< Node >( nodes, [] ( const Node & n ) -> Nodes {
            return n->succs;
        } );
    }

private:
    using Value = llvm::Value;
    using Function = llvm::Function;
    using ValueNodes = std::vector< ValueNode >;
    using Entries = FunctionNode::Entries;

    // Computes the functions for abstraction
    Nodes get_nodes( llvm::Module & m ) {
        Nodes queue = annotated( m );

        // TODO what happens if i have same function with different entries (args)
        Map< Function *, Set< Function * > > succs;
        Set< Node > done;

        while ( !queue.empty() ) {
            auto current = queue.back();
            queue.pop_back();

            auto find = std::find_if( done.begin(), done.end(), [&] ( Node n ) {
                // TODO compare entries
                return current->function == n->function;
            } );
            if ( find == done.end() ) {
                auto fn = current->function;

                if ( query::query( done ).none( [&]( Node n ) { return n->function == fn; } ) ) {
                    preprocess( fn );
                }

                auto allocas = abstract_allocas( current );
                current->entries.insert( allocas.begin(), allocas.end() );

                for ( auto dep : dependent_functions( current ) ) {
                    queue.push_back( dep );
                    succs[ fn ].insert( dep->function );
                }

                // TODO do not propagate backward if has only argument entries
                if ( returns_abstract( current ) )
                    for ( auto & dep : calls_of( current ) ) {
                        queue.push_back( dep );
                    }
                done.insert( current );
            }
        }

        Nodes ns = { done.begin(), done.end() };
        for ( auto & node : ns ) {
            for ( auto & fn : succs[ node->function ] ) {
                auto succ = std::find_if( ns.begin(), ns.end(), [&] ( const Node & n ) {
                    return n->function == fn;
                } );
                assert( succ != ns.end() );
                node->succs.push_back( *succ );
            }
        }

        return { done.begin(), done.end() };
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
                        return ValueNode{ n.value, n.annotation };
                    } ).freeze();

                preprocess( fn );
                Entries entries{ values.begin(), values.end() };

                return std::make_shared< FunctionNode >( fn, entries );
            } ).freeze();
    }

    Entries abstract_allocas( const Node & node ) {
        Entries reached = { node->entries };
        Entries allocas;

        do {
            allocas = reached;
            // FIXME rework to use range
            ValueNodes nodes = { reached.begin(), reached.end() };
            // FIXME create global ValueSucc function
            auto postorder = analysis::postorder< ValueNode >( nodes,
            [] ( const ValueNode & n ) { return n.succs(); } );

            auto uses_alloca = [] ( ValueNode & n ) {
                 return query::query( llvm::cast< llvm::Instruction >( n.value )->operands() )
                    .map( query::llvmdyncast< llvm::Instruction > )
                    .filter( query::notnull )
                    .any( [] ( llvm::Instruction * inst ) {
                        return llvm::isa< llvm::AllocaInst >( inst );
                    } );
            };

            auto alloca_users = query::query( postorder )
                .filter( []( const auto & n ) {
                    return llvm::isa< llvm::Instruction >( n.value );
                } )
                .filter( uses_alloca )
                .freeze();

            for ( auto & user : alloca_users )
                for ( auto & op : llvm::cast< llvm::Instruction >( user.value )->operands() )
                    if ( llvm::isa< llvm::AllocaInst >( op ) )
                        reached.emplace( op, user.annotation );
        } while ( allocas != reached );

        return allocas;
    }

    Nodes dependent_functions( const Node & node ) {
        auto calls = query::query( node->postorder() )
            // Find calls that take abstract arguments
            .filter( []( const ValueNode & n ) {
                return llvm::dyn_cast< llvm::CallInst >( n.value );
            } )
            .filter( []( const ValueNode & n ) {
                auto call = llvm::cast< llvm::CallInst >( n.value );
                return !call->getCalledFunction()->isIntrinsic();
            } ).freeze();

        // Compute abstract entries to each called function
        Nodes functions;
        for ( auto n : calls ) {
            auto call = llvm::cast< llvm::CallInst >( n.value );
            auto fn = call->getCalledFunction();

            Entries entries;
            for ( size_t i = 0; i < call->getNumArgOperands(); ++i ) {
                bool abstract = query::query( node->postorder() )
                    .any( [&] ( const ValueNode & vn ) {
                        return vn.value == call->getArgOperand( i );
                    } );

                if ( abstract )
                    entries.emplace( std::next( fn->arg_begin(), i ), n.annotation );
            }
            functions.push_back( std::make_shared< FunctionNode >( fn, entries ) );
        }

        return functions;
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
        for ( auto user : node->function->users() ) {
            auto call = llvm::cast< llvm::CallInst >( user );
            auto fn = call->getParent()->getParent();
            Entries entries = { ValueNode{ call, ret.annotation } } ;
            ns.emplace_back( std::make_shared< FunctionNode >( fn, entries ) );
        }
        return ns;
    }

    const Preprocess preprocess;
    Nodes nodes;
};


template< typename Preprocess >
using AbstractWalker = Walker< Preprocess, FunctionNodePtr >;

} // namespace abstract
} // namespace lart
