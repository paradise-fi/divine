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
#include <lart/support/query.h>

#include <unordered_map>

#include <brick-string>

#ifndef LART_ANALYSIS_BBSCC_H
#define LART_ANALYSIS_BBSCC_H

namespace lart {
namespace analysis {

struct BasicBlockSCC {

    struct SCC {

        SCC() : _canonical( nullptr ) { }
        explicit SCC( llvm::BasicBlock *canonical ) : _canonical( canonical ) {
            _insert( canonical );
        }

        llvm::BasicBlock *canonical() const { return _canonical; }
        const std::unordered_set< llvm::BasicBlock * > &basicBlocks() const { return _bbs; }
        const std::unordered_set< SCC * > &succs() const { return _succs; }
        const std::unordered_set< SCC * > &preds() const { return _preds; }
        bool nontrivial() const { return _bbs.size() > 1; }
        bool cyclic() const {
            return nontrivial() || std::find( succ_begin( _canonical ),
                    succ_end( _canonical ), _canonical ) != succ_end( _canonical );
        }


      private:
        friend struct BasicBlockSCC;
        llvm::BasicBlock *_canonical;
        std::unordered_set< llvm::BasicBlock * > _bbs;
        std::unordered_set< SCC * > _succs;
        std::unordered_set< SCC * > _preds;

        void _insert( llvm::BasicBlock *bb ) {
            _bbs.insert( bb );
        }

        void _finalize( const std::unordered_map< llvm::BasicBlock *, SCC * > &sccs ) {
            ASSERT( _succs.empty() );
            ASSERT( _preds.empty() );

            auto bbsToSCCs = [&]( auto bbnext ) {
                std::unordered_set< SCC * > ret;
                for ( llvm::BasicBlock *bb : _bbs  ) {
                    for ( llvm::BasicBlock &bbnx : bbnext( bb ) ) {
                        auto sccit = sccs.find( &bbnx );
                        ASSERT( sccit != sccs.end() );
                        if ( sccit->second != this )
                            ret.insert( sccit->second );
                    }
                }
                return ret;
            };

            _succs = bbsToSCCs( []( llvm::BasicBlock *bb ) { return util::succs( bb ); } );
            _preds = bbsToSCCs( []( llvm::BasicBlock *bb ) { return util::preds( bb ); } );
        }

    };

    using iterator = std::vector< SCC >::const_iterator;
    using value_type = SCC;

    explicit BasicBlockSCC( llvm::Function &fn ) {
        if ( !fn.empty() )
            _tarjan( fn );
    }

    const SCC *operator[]( llvm::BasicBlock *bb ) const {
        auto it = _sccMap.find( bb );
        if ( it != _sccMap.end() )
            return it->second;
        return nullptr;
    }

    const SCC *get( llvm::BasicBlock *bb ) const { return (*this)[ bb ]; }

    bool sameSCC( llvm::BasicBlock *a, llvm::BasicBlock *b ) const {
        return get( a ) == get( b );
    }

    bool sameSCC( llvm::Instruction *a, llvm::Instruction *b ) const {
        return sameSCC( a->getParent(), b->getParent() );
    }

    auto begin() const { return _sccs.begin(); }
    auto end() const { return _sccs.end(); }

  private:
    std::unordered_map< llvm::BasicBlock *, SCC * > _sccMap;
    std::vector< std::unique_ptr< SCC > > _sccs;

    struct TJ {
        static constexpr int undef = std::numeric_limits< int >::max();
        int index = undef;
        int lowlink = undef;
        bool onstack = false;
    };

    void _tarjan( llvm::Function &fn ) {
        ASSERT( _sccs.empty() );
        ASSERT( _sccMap.empty() );

        std::unordered_map< llvm::BasicBlock *, TJ > map;
        std::vector< llvm::BasicBlock * > stack;

        int index = 0;

        for ( auto &bb : fn ) {
            if ( map[ &bb ].index == TJ::undef )
                _tjscc( &bb, index, map, stack );
            ASSERT( _sccMap.count( &bb ), "bb:", bb, "parent:", *bb.getParent() );
        }

        for ( auto &scc : _sccs )
            scc->_finalize( _sccMap );
    }


    void _tjscc( llvm::BasicBlock *bb, int &index,
            std::unordered_map< llvm::BasicBlock *, TJ > &map,
            std::vector< llvm::BasicBlock * > &stack )
    {
        TJ &v = map[ bb ];
        v.lowlink = v.index = index++;

        // stack keeps all vertices which are to be explored, but node
        // stays in there iff it has path to some node earlier on stack
        v.onstack = true;
        stack.push_back( bb );

        for ( llvm::BasicBlock &succ : util::succs( bb ) ) {
            TJ &w = map[ &succ ];
            if ( w.index == TJ::undef ) {
                _tjscc( &succ, index, map, stack );
                v.lowlink = std::min( v.lowlink, w.lowlink );
            } else if ( w.onstack ) // succ in current scc
                v.lowlink = std::min( v.lowlink, w.lowlink );
        }

        // if v is root node of SCC, pop stack and fill SCC
        if ( v.lowlink == v.index ) {
            _sccs.emplace_back( std::make_unique< SCC >( bb ) );
            SCC *scc = _sccs.back().get();
            while ( true ) {
                auto w = stack.back();
                stack.pop_back();
                ASSERT( map[ w ].onstack );
                map[ w ].onstack = false;
                scc->_insert( w );
                _sccMap[ w ] = scc;

                if ( bb == w )
                    break;
            }
        }
    };

};

inline auto succ_begin( const BasicBlockSCC::SCC *scc ) { return scc->succs().begin(); }
inline auto succ_end( const BasicBlockSCC::SCC *scc ) { return scc->succs().end(); }

inline auto pred_begin( const BasicBlockSCC::SCC *scc ) { return scc->preds().begin(); }
inline auto pred_end( const BasicBlockSCC::SCC *scc ) { return scc->preds().end(); }

}
} // namespace lart

#endif // LART_ANALYSIS_BBSCC_H
