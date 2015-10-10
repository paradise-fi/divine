// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>
//
//
#include <type_traits>
#include <llvm/Support/Casting.h>
#include <brick-assert.h>
#include <brick-types.h>
#include <chrono>
#include <string>

#ifndef LART_SUPPORT_UTIL_H
#define LART_SUPPORT_UTIL_H

namespace lart {

namespace util {

template< typename T >
using Set = std::unordered_set< T >;

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

template< typename It >
struct FixLLVMIt : It {
    using reference = typename std::conditional<
            std::is_same< typename It::value_type &, typename It::reference >::value,
            typename It::value_type,
            typename It::value_type &
        >::type;

    reference operator*() { return *It::operator*(); }
    using It::It;
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

}

#endif // LART_SUPPORT_UTIL_H
