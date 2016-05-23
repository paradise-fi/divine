// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>
//
//
#include <type_traits>

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Casting.h>
#include <llvm/IR/CFG.h>
#include <llvm/Analysis/CaptureTracking.h>

#include <brick-assert>
#include <brick-types>
#include <chrono>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <map>
#include <iostream>

#ifndef LART_SUPPORT_UTIL_H
#define LART_SUPPORT_UTIL_H

namespace std {

template< typename A, typename B >
struct hash< std::pair< A, B > > {
    size_t operator()( const std::pair< A, B > &pair ) const {
        return ha( pair.first ) ^ hb( pair.second );
    }

  private:
    std::hash< A > ha;
    std::hash< B > hb;
};

}

namespace lart {

namespace util {

template< typename T >
using Set = std::set< T >;

template< typename K, typename V >
using Map = std::map< K, V >;

template< typename >
struct ReturnType { };

template< typename R, typename... Args >
struct ReturnType< R (*)( Args... ) > { using T = R; };

template< unsigned i, typename... Empty >
struct _Get { };

template< typename _T, typename... Ts >
struct _Get< 0, _T, Ts... > { using T = _T; };

template< unsigned i, typename _T, typename... Ts >
struct _Get< i, _T, Ts... > { using T = typename _Get< i - 1, Ts... >::T; };

template< unsigned, typename >
struct MethodArg { };

template< unsigned i, typename Obj, typename R, typename... Args >
struct MethodArg< i, R (Obj::*)( Args... ) > {
    using T = typename _Get< i, Args... >::T;
};

template< unsigned i, typename Obj, typename R, typename... Args >
struct MethodArg< i, R (Obj::*)( Args... ) const > {
    using T = typename _Get< i, Args... >::T;
};

template< typename It >
struct Range {
    using iterator = It;

    Range( It begin, It end ) : _begin( begin ), _end( end ) { }

    It begin() const { return _begin; }
    It end() const { return _end; }

  private:
    const It _begin;
    const It _end;
};

template< typename To, typename From >
struct Deref;

template< typename T >
struct Deref< T &, T * > { static T &run( T *x ) { return *x; } };

template< typename T >
struct Deref< T, T > { static T run( T x ) { return x; } };

template< typename It >
struct FixLLVMIt : It {
    using reference = typename It::value_type &;

    reference operator*() { return Deref< reference, typename It::reference >::run( It::operator*() ); }
    using It::It;
    FixLLVMIt() = default;
    FixLLVMIt( const It &it ) : It( it ) { }
    FixLLVMIt( It &&it ) : It( std::move( it ) ) { }
};

using LLVMBBSuccIt = llvm::SuccIterator< llvm::TerminatorInst *, llvm::BasicBlock >;
template<>
struct FixLLVMIt< LLVMBBSuccIt > : LLVMBBSuccIt
{
    using It = LLVMBBSuccIt;
    using reference = llvm::BasicBlock &;

    reference operator*() { return *It::operator*(); }
    using It::It;
    FixLLVMIt() : It( nullptr ) { }
    FixLLVMIt( const It &it ) : It( it ) { }
    FixLLVMIt( It &&it ) : It( std::move( it ) ) { }
};

template< typename It >
auto fixLLVMIt( It it ) { return FixLLVMIt< It >( it ); }

template< typename It >
auto range( It begin, It end ) { return Range< FixLLVMIt< It > >( begin, end ); }

template< typename T, typename It = typename T::reverse_iterator >
auto reverse( T &t ) { return Range< It >( t.rbegin(), t.rend() ); }

template< typename T >
struct PtrPlusBit {
    explicit PtrPlusBit( T *ptr ) : ptr( ptr ) {
        ASSERT( (uintptr_t( ptr ) & 0x1) == 0 );
    }

    bool bit() { return (uintptr_t( ptr ) & 0x1u) == 0x1u; }
    void bit( bool v ) { ptr = reinterpret_cast< T * >( uintptr_t( get() ) | uintptr_t( v ) ); }
    T *get() { return reinterpret_cast< T * >( uintptr_t( ptr ) & ~uintptr_t(0x1) ); }
    T &operator*() { return *get(); }
    T *operator->() { return get(); }

  private:
    T *ptr;
};

template< typename X >
auto succs( X *x ) { return range( succ_begin( x ), succ_end( x ) ); }

template< typename X >
auto succs( X &x ) { return succs( &x ); }

template< typename X >
auto preds( X *x ) { return range( pred_begin( x ), pred_end( x ) ); }

template< typename X >
auto preds( X &x ) { return preds( &x ); }

struct Timer {
    explicit Timer( std::string msg ) : _msg( msg ), _begin( std::chrono::high_resolution_clock::now() ) { }
    ~Timer() {
        std::cerr << "INFO: " << _msg << ": "
                  << std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::high_resolution_clock::now() - _begin ).count()
                  << "ms" << std::endl;
    }

  private:
    std::string _msg;
    std::chrono::high_resolution_clock::time_point _begin;
};

template< typename M = Map< llvm::Value *, bool >  >
bool canBeCaptured( llvm::Value *v, M *map = nullptr ) {
    if ( !llvm::isa< llvm::AllocaInst >( v->stripPointerCasts() ) ) // llvm::isa< llvm::GlobalValue >( v ) )
        return true;
    if ( !map )
        return llvm::PointerMayBeCaptured( v, false, true );

    auto it = map->find( v );
    return it != map->end()
            ? it->second
            : ( (*map)[ v ] = llvm::PointerMayBeCaptured( v, false, true ) );
}

}

template< typename What >
bool llvmcase( What * ) { return false; }

template< typename What, typename Lambda, typename... Lambdas >
bool llvmcase( What *w, Lambda lambda, Lambdas &&...lambdas ) {
    if ( auto val = llvm::dyn_cast<
            typename std::remove_pointer<
                typename util::MethodArg< 0, decltype( &Lambda::operator() ) >::T
            >::type >( w ) )
    {
        lambda( val );
        return true;
    }
    return llvmcase( w, std::forward< Lambdas >( lambdas )... );
}

template< typename What, typename... Lambdas >
bool llvmcase( What &w, Lambdas &&...lambdas ) {
    return llvmcase( &w, std::forward< Lambdas >( lambdas )... );
}

template< typename T, typename TypeList, typename Yield >
auto apply( T *, TypeList, Yield && ) -> typename std::enable_if< TypeList::length == 0 >::type { }

// if i's dynamic type matches any in TypeList yield will be called with
// parameter of the matched type
template< typename T, typename TypeList, typename Yield >
auto apply( T *i, TypeList tl, Yield &&yield ) -> typename std::enable_if< TypeList::length != 0 >::type
{
    if ( auto *ii = llvm::dyn_cast< typename TypeList::Head >( i ) )
        yield( ii );
    apply( i, typename TypeList::Tail(), yield );
}

template< typename R, typename T, typename TypeList, typename Yield >
auto apply( T *, TypeList, R r, Yield && ) -> typename std::enable_if< TypeList::length == 0, R >::type { return r; }

// if i's dynamic type matches any in TypeList, Yield will be called with
// parameter of the matched type and its result returned
// otherwise def will be returned
template< typename R, typename T, typename TypeList, typename Yield >
auto apply( T *i, TypeList tl, R def, Yield &&yield ) -> typename std::enable_if< TypeList::length != 0, R >::type
{
    if ( auto *ii = llvm::dyn_cast< typename TypeList::Head >( i ) )
        return yield( ii );
    return apply( i, typename TypeList::Tail(), def, yield );
}

template< typename Pre, typename Post >
void bbDfs( llvm::BasicBlock &bb, util::Set< llvm::BasicBlock * > &seen, Pre pre, Post post ) {
    std::vector< util::PtrPlusBit< llvm::BasicBlock > > stack;
    stack.emplace_back( &bb );
    while ( !stack.empty() ) {
        auto &v = stack.back();
        if ( v.bit() ) {
            post( v.get() );
            stack.pop_back();
        } else {
            bool inserted = seen.insert( v.get() ).second;
            if ( inserted ) {
                pre( v.get() );
                v.bit( true );
                for ( auto &succ : util::succs( v.get() ) )
                    stack.emplace_back( &succ );
            } else
                stack.pop_back();
        }
    }
}

template< typename Pre, typename Post >
void bbDfs( llvm::Function &fn, util::Set< llvm::BasicBlock * > &seen, Pre pre, Post post ) {
    if ( fn.empty() )
        return;

    bbDfs( fn.getEntryBlock(), seen, pre, post );
}

template< typename Pre, typename Post >
void bbDfs( llvm::Function &fn, Pre pre, Post post ) {
    util::Set< llvm::BasicBlock * > seen;
    bbDfs( fn, seen, pre, post );
}

template< typename Pre, typename Post >
void bbDfs( llvm::BasicBlock &bb, Pre pre, Post post ) {
    util::Set< llvm::BasicBlock * > seen;
    bbDfs( bb, seen, pre, post );
}

template< typename Post >
void bbPostorder( llvm::Function &fn, Post post ) {
    return bbDfs( fn, []( llvm::BasicBlock * ) { }, post );
}

template< typename Post >
void bbPostorder( llvm::BasicBlock &bb, Post post ) {
    return bbDfs( bb, []( llvm::BasicBlock * ) { }, post );
}

template< typename Pre >
void bbPreorder( llvm::Function &fn, Pre pre ) {
    return bbDfs( fn, pre, []( llvm::BasicBlock * ) { } );
}

template< typename Pre >
void bbPreorder( llvm::BasicBlock &bb, Pre pre ) {
    return bbDfs( bb, pre, []( llvm::BasicBlock * ) { } );
}

struct BBEdge : brick::types::Ord {
    BBEdge() = default;
    BBEdge( llvm::BasicBlock *from, unsigned idx ) : from( from ), succIndex( idx ) { }
    llvm::BasicBlock *from = nullptr;
    unsigned succIndex = 0;

    bool operator==( const BBEdge &o ) const {
        return from == o.from && succIndex == o.succIndex;
    }

    bool operator<( const BBEdge &o ) const {
        return from < o.from || (from == o.from && succIndex < o.succIndex);
    }
};

inline util::Set< BBEdge > getBackEdges( llvm::Function &fn ) {
    std::vector< llvm::succ_iterator > stack;
    util::Set< llvm::BasicBlock * > inStack;
    util::Set< llvm::BasicBlock * > seen;
    util::Set< BBEdge > backedges;
    stack.emplace_back( succ_begin( fn.begin() ) );

    while ( !stack.empty() ) {
        auto *bb = stack.back().getSource();
        if ( stack.back() == succ_end( bb ) ) { // post
            stack.pop_back();
            inStack.erase( bb );
        } else {
            auto *dst = *stack.back();
            if ( inStack.count( dst ) )
                backedges.emplace( stack.back().getSource(), stack.back().getSuccessorIndex() );
            ++stack.back();
            if ( seen.insert( dst ).second ) {
                stack.emplace_back( succ_begin( dst ) );
                inStack.insert( dst );
            }
        }
    }
    return backedges;
}

inline llvm::Value *getCalledValue( llvm::CallInst *call ) { return call->getCalledValue(); }
inline llvm::Value *getCalledValue( llvm::InvokeInst *call ) { return call->getCalledValue(); }

template< typename C >
llvm::Function *getCalledFunction( C *call ) {
    llvm::Value *calledVal = getCalledValue( call );

    llvm::Function *called = nullptr;
    do {
        llvmcase( calledVal,
            [&]( llvm::Function *fn ) { called = fn; },
            [&]( llvm::BitCastInst *bc ) { calledVal = bc->getOperand( 0 ); },
            [&]( llvm::ConstantExpr *ce ) {
                if ( ce->isCast() )
                    calledVal = ce->getOperand( 0 );
            },
            [&]( llvm::Value *v ) { calledVal = nullptr; }
        );
    } while ( !called && calledVal );

    return called;
}

}

#endif // LART_SUPPORT_UTIL_H
