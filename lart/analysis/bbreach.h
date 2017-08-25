// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/CallSite.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>
#include <lart/analysis/bbscc.h>

#include <unordered_map>

#ifndef LART_ANALYSIS_BBREACH_H
#define LART_ANALYSIS_BBREACH_H

namespace lart {
namespace analysis {

/*
Reachability for basic blocks and instructions in function

Internally, this uses transitive closure of SCC collapse of basic block graph,
both for forward and backward transition relation

*/
struct Reachability {
    using SCC = BasicBlockSCC::SCC;
    using BB = llvm::BasicBlock;
    using I = llvm::Instruction;

    Reachability( llvm::Function &fn, BasicBlockSCC *scc ) : _sccs( scc ) {
        if ( !fn.empty() )
            _mk( fn );
    }

  private: // has to be defined before use
    template< typename T >
    auto _refEqPtr( T *ptr ) { return [ptr]( T &ref ) { return ptr == &ref; }; }
  public:

    bool reachable( BB *from, BB *to ) const {
        return from == to || strictlyReachable( from, to );
    }

    bool strictlyReachable( BB *from, BB *to ) const {
        return _strictlyReachable( from, to, _forwSccDagClosure );
    }

    bool backwardReachable( BB *from, BB *to ) const {
        return from == to || strictlyBackwardReachable( from, to );
    }

    bool strictlyBackwardReachable( BB *from, BB *to ) const {
        return _strictlyReachable( from, to, _backSccDagClosure );
    }

    bool reachable( I *from, I *to ) {
        return from == to || strictlyReachable( from, to );
    }

    bool strictlyReachable( I *from, I *to ) {
        // as a special case, invoke is considered to be defined at the beginning
        // of 'normal' patch block, that is it does not reach 'unwind' block, but
        // it reaches all instructions in 'normal' block (and any block reachable
        // from the normal block)
        if ( auto inv = llvm::dyn_cast< llvm::InvokeInst >( from ) )
            return reachable( inv->getNormalDest(), to->getParent() );

        // this includes case when from is before to in the same bb and there
        // is a cycle going through the bb, and the case of to being
        // in any reachable bb
        if ( strictlyReachable( from->getParent(), to->getParent() ) )
            return true;

        // the remaining reachable case is only if from and to are in the same bb,
        // but to occurs later
        if ( from->getParent() != to->getParent() )
            return false;

        // check if to is in the same bb, but later than from
        auto it = std::next( llvm::BasicBlock::iterator( from ) );
        return std::find_if( it, from->getParent()->end(), _refEqPtr( to ) )
                    != from->getParent()->end();
    }

    bool backwardReachable( I *from, I *to ) {
        return from == to || strictlyBackwardReachable( from, to );
    }

    bool strictlyBackwardReachable( I *from, I *to ) {
        auto fromBB = from->getParent();
        auto toBB = to->getParent();

        // again, invoke is considered to be part of its 'norma' destination
        // block (see strictlyReachable for more details)
        if ( auto inv = llvm::dyn_cast< llvm::InvokeInst >( to ) )
            return backwardReachable( fromBB, inv->getNormalDest() );

        if ( strictlyBackwardReachable( fromBB, toBB ) )
            return true;

        if ( fromBB != toBB )
            return false;

        auto it = std::find_if( fromBB->rbegin(), fromBB->rend(), _refEqPtr( from ) );
        return std::find_if( it, fromBB->rend(), _refEqPtr( to ) )
                    != fromBB->rend();
    }

  private:

    using Map = std::unordered_map< const SCC *, std::unordered_set< const SCC * > >;

    bool _strictlyReachable( BB *from, BB *to, const Map &clo ) const {
        auto fromSCC = _sccs->get( from );
        auto toSCC = _sccs->get( to );
        return fromSCC->canonical() == toSCC->canonical() // on cycle
            || clo.find( fromSCC )->second.count( toSCC ); // reachable dag node
    }

    void _mk( llvm::Function &fn ) {
        std::unordered_set< const SCC * > seen;
        for ( auto &bb : fn )
            _closure( _sccs->get( &bb ), seen, _forwSccDagClosure, &SCC::succs );
        for ( auto &bb : fn )
            _closure( _sccs->get( &bb ), seen, _backSccDagClosure, &SCC::preds );
    }

    template< typename Seen, typename Nx >
    void _closure( const SCC *scc, Seen &seen, Map &closure, Nx nx ) {
        if ( seen.insert( scc ).second ) {
            auto &nxclo = closure[ scc ];
            for ( SCC *s : (scc->*nx)() ) {
                // recursively calculate closure of successor
                _closure( s, seen, closure, nx );

                // then add it to closure of this SCC, together with successor
                nxclo.insert( s );
                auto &sclo = closure[ s ];
                nxclo.insert( sclo.begin(), sclo.end() );
            }
        }
    }

    BasicBlockSCC *_sccs;
    Map _forwSccDagClosure;
    Map _backSccDagClosure;
};

} // namespace analysis
} // namespace lart

#endif // LART_ANALYSIS_BBREACH_H
