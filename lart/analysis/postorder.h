// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
DIVINE_RELAX_WARNINGS
#include <llvm/IR/Value.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>

#ifndef LART_ANALYSIS_POSTORDERDEPS_H
#define LART_ANALYSIS_POSTORDERDEPS_H

namespace lart {
namespace analysis {

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

} // namespace analysis
} // namespace lart

#endif

