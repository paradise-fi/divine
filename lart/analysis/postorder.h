// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
DIVINE_RELAX_WARNINGS
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/Analysis/CallGraph.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>
#include <lart/support/query.h>

#include <stack>
#include <unordered_set>
#include <algorithm>
#include <functional>

#ifndef LART_ANALYSIS_POSTORDERDEPS_H
#define LART_ANALYSIS_POSTORDERDEPS_H

namespace lart {
namespace analysis {

template< typename N >
using Succs = std::function< std::vector< N > ( const N & ) >;

template< typename Node >
std::vector< Node > roots( const std::vector< Node > & nodes, const Succs< Node > & succs ) {
    auto has_no_predecessor = [&] ( const auto & node ) {
        return query::query( nodes )
            .all( [&] ( const auto & n ) {
                return query::query( succs( n ) )
                    .none( [&] ( const auto & succ ) { return succ == node; } );
            } );
    };

    return query::query( nodes ).filter( has_no_predecessor ).freeze();
}

template< typename Node >
std::vector< Node > postorder( const std::vector< Node > & nodes, const Succs< Node > & succs ) {
    std::unordered_set< Node > visited;
    std::stack< std::pair< bool, Node > > stack;
    std::vector< Node > order;

    for ( const auto & r : roots< Node >( nodes, succs ) )
        stack.emplace( false, r );

    while ( !stack.empty() ) {
        auto node = stack.top();
        stack.pop();

        if ( node.first ) {
            if ( std::find( order.begin(), order.end(), node.second ) == order.end() )
                order.push_back( node.second );
            continue;
        }

        visited.insert( node.second );

        stack.emplace( true, node.second );
        for ( const auto & s : succs( node.second ) )
            // TODO does not recognize that Node is already in visited
            if ( visited.find( s ) == visited.end() )
                stack.emplace( false, s );
    }
    return order;
}

    //FIXME generalize
    template < typename V >
    std::vector< V > postorder( V dep ) {
        util::Set< V> visited;
        std::vector< std::pair< bool, V > > stack;
        std::vector< V > post;

        stack.push_back( { false, dep } );

        while ( !stack.empty() ) {
            auto node = stack.back();
            stack.pop_back();

            if ( node.first ) {
                if ( std::find( post.begin(), post.end(), node.second ) == post.end() )
                    post.push_back( node.second );
                continue;
            }

            visited.insert( node.second );

            stack.push_back( { true, node.second } );
            for ( auto user : node.second->users() )
                if ( visited.find( user ) == visited.end() )
                    stack.push_back( { false, user } );
        }

        return post;
    }

    template < typename F >
    std::vector< F > callPostorder( llvm::Module &m, std::vector< F > functions ) {
        auto & ctx = m.getContext();
        auto fty = llvm::FunctionType::get( llvm::Type::getVoidTy( ctx ), false );

        auto c = m.getOrInsertFunction( "__callgraph_root", fty );
        llvm::Function * root = llvm::cast< llvm::Function >( c );

        auto bb = llvm::BasicBlock::Create( ctx, "entry", root );
        llvm::IRBuilder<> irb( bb );
        for ( auto &f : functions ) {
            std::vector< llvm::Value * > args;
            for ( auto &arg : f->args() )
                args.push_back( llvm::UndefValue::get( arg.getType() ) );
            irb.CreateCall( f, args );
        }

        std::vector< llvm::Function * > order;
        {
            llvm::CallGraph cg( m );
            auto node = cg[ root ];
            for ( auto it = po_begin( node ); it != po_end( node ); ++it) {
                auto fn = (*it)->getFunction();
                if ( fn != nullptr && fn != root )
                     order.push_back( fn );
            }
        }

        root->eraseFromParent();

        auto filter = query::query( order )
                      .filter( [&]( llvm::Function *fn ) {
                          return std::find( functions.begin(), functions.end(), fn )
                              != functions.end();
                      } )
                      .freeze();

        return filter;
    }

} // namespace analysis
} // namespace lart

#endif

