// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <llvm/IR/PassManager.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/CallSite.h>

#include <lart/support/util.h>

#include <unordered_map>

#include <lart/support/query.h>

#ifndef LART_ESCAPE_ANALYSIS_H
#define LART_ESCAPE_ANALYSIS_H

namespace lart {
namespace escape {

struct EscapeAnalysis {

    EscapeAnalysis( llvm::Module &module ) :
        _module( module ), _layout( &module ), _ptrBits( _layout.getPointerSizeInBits() )
    { }

    struct Analysis {

        bool dirty() const { return _dirty; }
        void startIteration() { _dirty = false; ++_iteration; }
        int iteration() const { return _iteration; }

        void addEscape( llvm::Value *what, llvm::Instruction *where ) {
            if ( _escapes[ what ].insert( where ).second ) {
                _dirty = true;
            }
        }
        void addUse( llvm::Value *what, llvm::Instruction *where ) {
            auto it = _escapes.find( where );
            if ( it != _escapes.end() )
                for ( auto i : it->second )
                    if ( _escapes[ what ].insert( i ).second ) {
                        _dirty = true;
                    }
        }

        const std::unordered_set< llvm::Instruction * > &escapes( llvm::Instruction *what ) {
            return _escapes[ what ];
        }

      private:
        bool _dirty = true;
        int _iteration = 0;
        // mapping instruction -> where it escapes
        std::unordered_map< llvm::Value *, std::unordered_set< llvm::Instruction * > > _escapes;
    };
    using Result = Analysis;

    void dropAnalysis( llvm::Function *fn ) { _cache.erase( fn ); }

    Analysis run( llvm::Function &fn ) { return analyze( fn ); }
    Analysis analyze( llvm::Function &fn ) { return analyze( &fn ); }
    Analysis analyze( llvm::Function *fn ) {
        Analysis an;

        auto addEscape = [&]( llvm::Value *val, llvm::Instruction *inst ) {
            if ( canBePointer( val->getType() ) )
                an.addEscape( val, inst );
        };

        while ( an.dirty() ) {
            an.startIteration();
            bbPostorder( *fn, [&]( llvm::BasicBlock *bb ) {
                for ( auto &inst : util::reverse( *bb ) ) {
                    llvmcase( inst,
                        []( llvm::DbgDeclareInst * ) { }, // ignore
                        [&]( llvm::StoreInst *store ) {
                            addEscape( store->getValueOperand(), store );
                        },
                        [&]( llvm::AtomicCmpXchgInst *cas ) {
                            addEscape( cas->getNewValOperand(), cas );
                        },
                        [&]( llvm::AtomicRMWInst *rmw ) {
                            addEscape( rmw->getValOperand(), rmw );
                        },
                        [&]( llvm::CallInst *call ) {
                            for ( auto &arg : call->arg_operands() )
                                addEscape( arg, call );
                        },
                        [&]( llvm::InvokeInst *invoke ) {
                            for ( auto &arg : invoke->arg_operands() )
                                addEscape( arg, invoke );
                        },
                        [&]( llvm::Instruction *inst ) {
                            for ( auto &op : inst->operands() )
                                an.addUse( op, inst );
                        } );
                }
            } );
        }
//        std::cerr << "INFO: analyzed " << fn->getName().str() << " in " << an.iteration() << " iterations" << std::endl;
        return an;
    }

    Analysis &operator[]( llvm::Function *fn ) {
        auto it = _cache.find( fn );
        if ( it == _cache.end() )
            return _cache[ fn ] = analyze( fn );
        return it->second;
    }

  private:
    llvm::Module &_module;
    llvm::DataLayout _layout;
    std::unordered_map< llvm::Function *, Analysis > _cache;
    unsigned _ptrBits;

    bool canBePointer( llvm::Type *t ) {
        return t->isPointerTy() || t->getPrimitiveSizeInBits() > _ptrBits;
    }

};

} // namespace escape

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
            ASSERT( _sccMap.count( &bb ) || (bb.dump(), bb.getParent()->dump(), false) );
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

auto succ_begin( const BasicBlockSCC::SCC *scc ) { return scc->succs().begin(); }
auto succ_end( const BasicBlockSCC::SCC *scc ) { return scc->succs().end(); }

auto pred_begin( const BasicBlockSCC::SCC *scc ) { return scc->preds().begin(); }
auto pred_end( const BasicBlockSCC::SCC *scc ) { return scc->preds().end(); }

/*
Reachablility for basic blocks and instructions in function

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

  private: // has to be defined befor use
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
        // this includes case when from is before to in the same bb and there
        // is cycle going through the bb, and the case of to being
        // in any reachable bb
        if ( strictlyReachable( from->getParent(), to->getParent() ) )
            return true;

        // the remaining reachable case is only if from and to are in same bb,
        // but to occurs later
        if ( from->getParent() != to->getParent() )
            return false;

        // check if to is in same bb, but later then from
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

}
} // namespace lart

#endif // LART_ESCAPE_ANALYSIS_H
