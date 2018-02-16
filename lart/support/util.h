// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>
//
//
#include <type_traits>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Constants.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Support/Casting.h>
#include <llvm/IR/CFG.h>
#include <llvm/Analysis/CaptureTracking.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/ValueMap.h>
#include <llvm/IR/DebugInfo.h>

#include <llvm/Transforms/Utils/Cloning.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/query.h>
#include <brick-assert>
#include <brick-types>
#include <brick-string>
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

/* Data structure which keeps data ordered in the order of insertion, but remove duplicates.
 * Internally, this containts a vector and a set.
 * Data must not be changed after insertion.
 */
template< typename T >
struct StableSet {

    using value_type = T;
    using iterator = typename std::vector< T >::const_iterator;
    using const_iterator = iterator;

    // O(log n), retuns true it x was not present prior to insertion
    bool insert( const T &x ) {
        if ( _set.insert( x ).second ) {
            _vec.push_back( x );
            return true;
        }
        return false;
    }

    iterator begin() const { return _vec.begin(); }
    iterator end() const { return _vec.end(); }

  private:
    std::vector< T > _vec;
    Set< T > _set;
};

template < typename Container >
struct Store : public Container {
    using K = typename Container::key_type;

    bool contains( K key ) const {
        return this->find( key ) != this->end();
    }
};

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

inline llvm::ArrayType *resizeArrayType( llvm::ArrayType *t, long size ) {
    auto *elemType = llvm::cast< llvm::ArrayType >( t )->getElementType();
    return llvm::ArrayType::get( elemType, size );
}

inline unsigned getOpcode( llvm::Value *v ) {
    if ( auto *i = llvm::dyn_cast< llvm::Instruction >( v ) )
        return i->getOpcode();
    if ( auto *ce = llvm::dyn_cast< llvm::ConstantExpr >( v ) )
        return ce->getOpcode();
    return 0;
}

inline void replaceGlobalArray( llvm::GlobalVariable *glo, llvm::Constant *init )
{
    auto *arrType = init->getType();
    auto *newGlo = new llvm::GlobalVariable( *glo->getParent(), arrType, true,
                llvm::GlobalValue::ExternalLinkage, init,
                "lart.module." + glo->getName().str() , glo );

    for ( auto u : query::query( glo->users() ).freeze() ) { // avoid iterating over a list while we delete from it
        auto *inst = llvm::dyn_cast< llvm::Instruction >( u );
        llvm::ConstantExpr *constant = nullptr;
        if ( !inst ) {
            constant = llvm::cast< llvm::ConstantExpr >( u );
            inst = constant->getAsInstruction();
        }
        ASSERT( inst );

        switch ( inst->getOpcode() ) {
            case llvm::Instruction::GetElementPtr:
            {
                llvm::GetElementPtrInst *gep = llvm::cast< llvm::GetElementPtrInst >( inst );
                std::vector< llvm::Value * > idxs;
                for ( auto &i : brick::query::range( gep->idx_begin(), gep->idx_end() ) )
                    idxs.push_back( *&i );
                if ( constant )
                    constant->replaceAllUsesWith( llvm::ConstantExpr::getGetElementPtr(
                                                      nullptr, newGlo, idxs ) );
                else
                    llvm::ReplaceInstWithInst( gep, llvm::GetElementPtrInst::Create(
                                                     nullptr, newGlo, idxs ) );
                break;
            }
            case llvm::Instruction::PtrToInt:
                if ( constant )
                    constant->replaceAllUsesWith( llvm::ConstantExpr::getPtrToInt( newGlo, inst->getType() ) );
                else
                    llvm::ReplaceInstWithInst( inst, new llvm::PtrToIntInst( newGlo, inst->getType() ) );
                break;
            case llvm::Instruction::BitCast:
                if ( constant )
                    constant->replaceAllUsesWith( llvm::ConstantExpr::getBitCast( newGlo, inst->getType() ) );
                else
                    llvm::ReplaceInstWithInst( inst, new llvm::BitCastInst( newGlo, inst->getType() ) );
                break;
            default:
                u->dump();
                inst->dump();
                UNREACHABLE_F( "Unsupported ocode in replaceGlobalArray: %d", inst->getOpcode() );
        }

        if ( constant )
            delete inst;
    }
    auto name = glo->getName().str();
    glo->eraseFromParent();
    newGlo->setName( name );
}

template< typename GetInit >
void replaceGlobalArray( llvm::Module &m, std::string name, GetInit init )
{
    auto *glo = m.getGlobalVariable( name );
    ASSERT( glo );
    return replaceGlobalArray( glo, init( llvm::cast< llvm::ArrayType >(
                    llvm::cast< llvm::PointerType >( glo->getType() )->getElementType()
                    )->getElementType() ) );
}

template< typename T >
inline llvm::Constant *getStringGlobal( const T &value, llvm::Module &m ) {
    auto *init = llvm::ConstantDataArray::get( m.getContext(),
                        llvm::ArrayRef< uint8_t >(
                            reinterpret_cast< const uint8_t * >( value.data() ),
                            value.size() ) );
    return new llvm::GlobalVariable( m, init->getType(), true,
                            llvm::GlobalValue::ExternalLinkage, init );
}

namespace _detail {

inline llvm::Constant *buildInit( std::vector< std::tuple< std::string, std::vector< uint8_t > > > value,
                                       llvm::Module &m, llvm::Type *elem )
{
    auto *structT = llvm::cast< llvm::StructType >( elem );
    auto *charPtrT = llvm::cast< llvm::PointerType >( structT->getElementType( 0 ) );
    auto *intT = llvm::cast< llvm::IntegerType >( structT->getElementType( 2 ) );

    auto data = query::query( value ).map( [&]( auto p ) {
            std::get< 0 >( p ).push_back( 0 );
            auto *key = llvm::ConstantExpr::getPointerCast(
                                getStringGlobal( std::get< 0 >( p ), m ), charPtrT );
            auto *value = llvm::ConstantExpr::getPointerCast(
                                getStringGlobal( std::get< 1 >( p ), m ), charPtrT );
            auto *size = llvm::ConstantInt::get(
                                llvm::Type::getInt32Ty( m.getContext() ), std::get< 1 >( p ).size() );
            return llvm::ConstantStruct::get( structT, { key, value, size } );
        } ).freeze();
    data.push_back( llvm::ConstantStruct::get( structT, {
                        llvm::ConstantPointerNull::get( charPtrT ),
                        llvm::ConstantPointerNull::get( charPtrT ),
                        llvm::ConstantInt::get( intT, 0 ) } ) );

    return llvm::ConstantArray::get( llvm::ArrayType::get( structT, data.size() ), data );
}

}

template< typename T >
void replaceGlobalArray( llvm::Module &m, std::string name, std::vector< T > value )
{
    replaceGlobalArray( m, name,
            [&]( llvm::Type *t ) { return _detail::buildInit( value, m, t ); } );
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
auto apply( T *i, TypeList, Yield &&yield ) -> typename std::enable_if< TypeList::length != 0 >::type
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
auto apply( T *i, TypeList, R def, Yield &&yield ) -> typename std::enable_if< TypeList::length != 0, R >::type
{
    if ( auto *ii = llvm::dyn_cast< typename TypeList::Head >( i ) )
        return yield( ii );
    return apply( i, typename TypeList::Tail(), def, yield );
}

template< typename Yield >
auto applyInst( llvm::Instruction *i, Yield &&y ) {
    switch ( i->getOpcode() ) {
#define HANDLE_INST( opcode, _x, Class ) case opcode: return y( llvm::cast< llvm::Class >( i ) );
#include <llvm/IR/Instruction.def>
        default:
            UNREACHABLE_F( "Invalid instruction %d", i->getOpcode() );
    }
}

template< typename Yield >
auto applyInst( llvm::Instruction &i, Yield &&y ) {
    return applyInst( &i, std::forward< Yield >( y ) );
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

inline util::StableSet< BBEdge > getBackEdges( llvm::Function &fn ) {
    std::vector< llvm::succ_iterator > stack;
    util::Set< llvm::BasicBlock * > inStack;
    util::Set< llvm::BasicBlock * > seen;
    util::StableSet< BBEdge > backedges;
    stack.emplace_back( succ_begin( fn.begin() ) );

    while ( !stack.empty() ) {
        auto *bb = stack.back().getSource();
        if ( stack.back() == succ_end( bb ) ) { // post
            stack.pop_back();
            inStack.erase( bb );
        } else {
            auto *dst = *stack.back();
            if ( inStack.count( dst ) )
                backedges.insert( BBEdge{ stack.back().getSource(),
                                          stack.back().getSuccessorIndex() } );
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
            [&]( llvm::Value * ) { calledVal = nullptr; }
        );
    } while ( !called && calledVal );

    return called;
}

using UserSet = util::Set< llvm::User * >;
using OpCode = llvm::Instruction;

// can the type fit a pointer type inside?
inline bool canContainPointer( llvm::Type *ty, const llvm::DataLayout &dl ) {
    return ty->isSized() && dl.getTypeSizeInBits( ty ) >= dl.getPointerSizeInBits();
}

inline bool canContainPointer( llvm::Type *ty, const llvm::Module *m ) {
    return canContainPointer( ty, m->getDataLayout() );
}

inline llvm::Value *getPointerOperand( llvm::Value *v ) {
    llvm::Value *ptr = nullptr;
    llvmcase( v,
            [&]( llvm::LoadInst *l ) { ptr = l->getPointerOperand(); },
            [&]( llvm::AtomicCmpXchgInst *c ) { ptr = c->getPointerOperand(); },
            [&]( llvm::StoreInst *s ) { ptr = s->getPointerOperand(); },
            [&]( llvm::AtomicRMWInst *r ) { ptr = r->getPointerOperand(); } );
    return ptr;
}

inline llvm::Value *getStoredPtr( llvm::Value *v ) {
    llvm::Value *ptr = nullptr;
    llvmcase( v,
            [&]( llvm::AtomicCmpXchgInst *c ) { ptr = c->getNewValOperand(); },
            [&]( llvm::StoreInst *s ) { ptr = s->getValueOperand(); },
            [&]( llvm::AtomicRMWInst *r ) { ptr = r->getValOperand(); } );
    return ptr;
}

namespace _detail {
    inline void pointerTransitiveUsers( UserSet &users,
                                        util::Set< std::pair< llvm::Value *, llvm::User * > > &edges,
                                        llvm::Value *def, llvm::User *_use,
                                        int maxRefDepth )
    {
        auto *use = llvm::dyn_cast< llvm::Instruction >( _use );
        if ( !def || !use || !llvm::isa< llvm::Instruction >( def ) )
            return; // constats can't really use any pointer, they are spurious
        if ( !edges.emplace( def, use ).second )
            return; // already done
        users.insert( use );

        auto op = use->getOpcode();
        bool captures = false;
        switch ( op ) {
            case OpCode::BitCast:
            case OpCode::GetElementPtr:
            case OpCode::PHI:
            case OpCode::Select:
                captures = true;
                break;
            case OpCode::Call:
                if ( llvm::isa< llvm::IntrinsicInst >( use ) ) {
                    if ( auto *trans = llvm::dyn_cast< llvm::MemTransferInst >( use ) ) {
                        // if it is possible that alloca's address is in memory,
                        // we need to look for uses of copy destination
                        if ( maxRefDepth > 0 && trans->getRawSource() == def )
                            for ( auto *u : trans->getDest()->users() )
                                pointerTransitiveUsers( users, edges, trans->getDest(),
                                                        u, maxRefDepth );
                    }
                    break; // all other intrinsics should be safe to ignore
                }
            case OpCode::Invoke:
            {
                // if it is a call and we don't have capture information we
                // must assume the return value depends on the alloca argument
                // if it can fit a pointer inside
                llvm::CallSite cs( use );

                for ( unsigned i = 0; i < cs.arg_size(); ++i )
                    if ( cs.getArgument( i ) == def && !cs.doesNotCapture( i ) )
                        captures = true;
                // track arguments which can possibly receive a copy of 'def'
                if ( captures ) {
                    for ( unsigned i = 0; i < cs.arg_size(); ++i ) {
                        if ( cs.getArgument( i ) != def && !cs.onlyReadsMemory()
                                && !cs.onlyReadsMemory( i ) )
                        {
                            for ( auto *u : cs.getArgument( i )->users() )
                                pointerTransitiveUsers( users, edges,
                                        cs.getArgument( i ),
                                        u, std::numeric_limits< int >::max() );
                        }
                    }
                }
                auto *ty = use->getType();
                auto *module = cs->getParent()->getParent()->getParent();
                captures = captures && canContainPointer( ty, module );
                maxRefDepth = std::numeric_limits< int >::max(); // can't do any better :-/
                break;
            }
            case OpCode::Load:
            case OpCode::Store:
            case OpCode::AtomicRMW:
            case OpCode::AtomicCmpXchg:
            {
                // dyn_cast: if it is a constant or argument, we don't even need to look at it
                auto *storedPtr = llvm::dyn_cast_or_null< llvm::Instruction >( getStoredPtr( use ) );
                auto *ptr = getPointerOperand( use );
                if ( storedPtr == def ) {
                    // track the stored-to pointer
                    for ( auto *u : ptr->users() )
                        pointerTransitiveUsers( users, edges, ptr, u, maxRefDepth + 1 );
                } else if ( op == OpCode::Load && maxRefDepth > 0 && ptr == def ) {
                    --maxRefDepth;
                    captures = true;
                }
                break;
            }
            default:
                captures = llvm::isa< llvm::BinaryOperator >( use );
        }
        if ( captures )
            for ( auto *u : use->users() )
                pointerTransitiveUsers( users, edges, use, u, maxRefDepth );
    }
}

enum class TrackPointers { None, Alloca, All };

// if v is of specified pointer type (either alloca or any type which can contain
// pointer) returns all uses which can depend on the pointed-to value
// for v of other type returns v.users() (as a set)
inline UserSet pointerTransitiveUsers( llvm::Instruction &v, TrackPointers track )
{
    auto *m = v.getParent()->getParent()->getParent();

    if ( ( track >= TrackPointers::Alloca && llvm::isa< llvm::AllocaInst >( v ) )
            || ( canContainPointer( v.getType(), m ) && track == TrackPointers::All ) )
    {
        UserSet users;
        util::Set< std::pair< llvm::Value *, llvm::User * > > edges;
        for ( auto *u : v.users() )
            _detail::pointerTransitiveUsers( users, edges, &v, u, 0 );
        return users;
    }
    return UserSet{ v.user_begin(), v.user_end() };
}

// function manipulations
namespace llvmstolen {

using namespace llvm;
// NOTE: these functions are copied from LLVM 3.7's lib/Transforms/Utils/CloneFunction.cpp
// as they are static in LLVM and therefore cannot be used from the library

// Add an operand to an existing MDNode. The new operand will be added at the
// back of the operand list.
inline void AddOperand(DICompileUnit *CU, DISubprogramArray SPs,
                       Metadata *NewSP) {
  SmallVector<Metadata *, 16> NewSPs;
  NewSPs.reserve(SPs.size() + 1);
  for (auto *SP : SPs)
    NewSPs.push_back(SP);
  NewSPs.push_back(NewSP);
  CU->replaceSubprograms(MDTuple::get(CU->getContext(), NewSPs));
}

// Find the MDNode which corresponds to the subprogram data that described F.
inline DISubprogram *FindSubprogram(const Function *F,
                                    DebugInfoFinder &Finder) {
  for (DISubprogram *Subprogram : Finder.subprograms()) {
    if (Subprogram->describes(F))
      return Subprogram;
  }
  return nullptr;
}

// Clone the module-level debug info associated with OldFunc. The cloned data
// will point to NewFunc instead.
inline void CloneDebugInfoMetadata(Function *NewFunc, const Function *OldFunc,
                            ValueToValueMapTy &VMap) {
  DebugInfoFinder Finder;
  Finder.processModule(*OldFunc->getParent());

  const DISubprogram *OldSubprogramMDNode = FindSubprogram(OldFunc, Finder);
  if (!OldSubprogramMDNode) return;

  // Ensure that OldFunc appears in the map.
  // (if it's already there it must point to NewFunc anyway)
  VMap[OldFunc] = NewFunc;
  auto *NewSubprogram =
      cast<DISubprogram>(MapMetadata(OldSubprogramMDNode, VMap));

  for (auto *CU : Finder.compile_units()) {
    auto Subprograms = CU->getSubprograms();
    // If the compile unit's function list contains the old function, it should
    // also contain the new one.
    for (auto *SP : Subprograms) {
      if (SP == OldSubprogramMDNode) {
        AddOperand(CU, Subprograms, NewSubprogram);
        break;
      }
    }
  }
}

}

inline void remapArgs( llvm::Function * from,
                       llvm::Function * to,
                       llvm::ValueToValueMapTy & vmap )
{
    auto destIt = to->arg_begin();
    for ( const auto &arg : from->args() )
        if ( vmap.count( &arg ) == 0 ) {
            destIt->setName( arg.getName() );
            vmap[&arg] = &*destIt++;
        }
}

inline llvm::Function * createFunctionSignature( llvm::Function *fn,
                                                 llvm::FunctionType *fty,
                                                 llvm::ValueToValueMapTy &vmap )
{
    auto newfn = llvm::Function::Create( fty, fn->getLinkage(),fn->getName() );
    remapArgs( fn, newfn, vmap );
    return newfn;
}

inline void cloneFunctionInto( llvm::Function * to,
                               llvm::Function * from,
                               llvm::ValueToValueMapTy &vmap )
{
    remapArgs( from, to, vmap );
    llvm::SmallVector< llvm::ReturnInst *, 8 > returns;
    llvmstolen::CloneDebugInfoMetadata( to, from, vmap );
    llvm::CloneFunctionInto( to, from, vmap, true, returns, "", nullptr );
}

inline llvm::Function * cloneFunction( llvm::Function *fn, llvm::FunctionType *fty )
{
    llvm::ValueToValueMapTy vmap;
    auto m = fn->getParent();
    auto newfn = createFunctionSignature( fn, fty, vmap );
    m->getFunctionList().push_back( newfn );
    cloneFunctionInto( newfn, fn, vmap );
    return newfn;
}

inline llvm::Function * cloneFunction( llvm::Function * fn )
{
    auto fty = fn->getFunctionType();
    return cloneFunction( fn, fty );
}

inline llvm::Function * changeFunctionSignature( llvm::Function * fn, llvm::FunctionType * fty )
{
    llvm::Function * newfn;
    if ( !fn->empty() )
        newfn= cloneFunction( fn, fty );
    else {
        llvm::ValueToValueMapTy vmap;
        auto m = fn->getParent();
        newfn = createFunctionSignature( fn, fty, vmap );
        m->getFunctionList().push_back( newfn );
    }
    return newfn;
}

inline llvm::Function * changeReturnType( llvm::Function *fn, llvm::Type *rty )
{
    auto fty = llvm::FunctionType::get( rty,
                                        fn->getFunctionType()->params(),
                                        fn->getFunctionType()->isVarArg() );
    return changeFunctionSignature( fn, fty );
}

namespace detail {
inline llvm::Function *throwOnUnknown( llvm::CallSite &cs ) {
    cs.getCalledValue()->dump();
    throw std::runtime_error( "could not clone function: calling unknown value" );
}

inline bool cloneAll( llvm::Function & ) {  return true; }
}

template< typename FunMap,
          typename Filter = bool (*)( llvm::Function & ),
          typename OnUnknown = llvm::Function *(*)( llvm::CallSite & ),
          typename = decltype( std::declval< FunMap & >().emplace( nullptr, nullptr ) ),
          typename = std::result_of_t< Filter( llvm::Function & ) >,
          typename = std::result_of_t< OnUnknown( llvm::CallSite & ) > >
llvm::Function *cloneFunctionRecursively( llvm::Function *fn, FunMap &map,
                                        Filter filter = detail::cloneAll,
                                        OnUnknown onUnknown = detail::throwOnUnknown,
                                        bool cloneFunctionPointers = false );

// map can be initialized to fnptr -> fnptr to skip cloning *fnptr
template< typename FunMap,
          typename Filter = bool (*)( llvm::Function & ),
          typename OnUnknown = llvm::Function *(*)( llvm::CallSite & ),
          typename = decltype( std::declval< FunMap & >().emplace( nullptr, nullptr ) ),
          typename = std::result_of_t< Filter( llvm::Function & ) >,
          typename = std::result_of_t< OnUnknown( llvm::CallSite & ) > >
void cloneCalleesRecursively( llvm::Function *fn, FunMap &map,
                              Filter filter = detail::cloneAll,
                              OnUnknown onUnknown = detail::throwOnUnknown,
                              bool cloneFunctionPointers = false )
{
    for ( auto &bb : *fn ) {
        for ( auto &ins : bb ) {
            if ( llvm::isa< llvm::IntrinsicInst >( ins ) )
                continue;
            llvm::CallSite cs( &ins );
            if ( cs ) {
                auto *callee = cs.getCalledFunction();
                callee = callee ? callee : onUnknown( cs );
                if ( !callee )
                    continue; // ignored by onUnknown
                cs.setCalledFunction( cloneFunctionRecursively( callee, map, filter, onUnknown, cloneFunctionPointers ) );
            }

            if ( !cloneFunctionPointers )
                continue;
            for ( int i = 0, n = ins.getNumOperands(); i < n; ++i ) {
                if ( ins.getOpcode() == llvm::Instruction::Call && i == n - 1 )
                    continue;
                if ( ins.getOpcode() == llvm::Instruction::Invoke && i == n - 3 )
                    continue;
                auto *v = ins.getOperand( i );
                if ( auto *fptr = llvm::dyn_cast< llvm::Function >( v ) ) {
                    ins.setOperand( i,
                            cloneFunctionRecursively( fptr, map, filter, onUnknown, cloneFunctionPointers ) );
                }
            }
        }
    }
}

// map can be initialized to fnptr -> fnptr to skip cloning *fnptr
template< typename FunMap, typename Filter, typename OnUnknown, typename, typename, typename >
llvm::Function *cloneFunctionRecursively( llvm::Function *fn, FunMap &map,
                                          Filter filter, OnUnknown onUnknown,
                                          bool cloneFunctionPointers )
{
    if ( !filter( *fn ) )
        return fn;
    auto it = map.find( fn );
    if ( it != map.end() )
        return it->second;
    auto *clone = cloneFunction( fn );
    map.emplace( fn, clone );
    map.emplace( clone, clone );
    cloneCalleesRecursively( clone, map, filter, onUnknown, cloneFunctionPointers );
    return clone;
}

inline llvm::Function *cloneFunctionRecursively( llvm::Function *fn )
{
    util::Map< llvm::Function *, llvm::Function * > map;
    return cloneFunctionRecursively( fn, map, detail::cloneAll, detail::throwOnUnknown );
}

inline void inlineIntoCallers( llvm::Function *fn ) {
    std::vector< llvm::Value * > users{ fn->user_begin(), fn->user_end() };
    for ( auto *u : users ) {
        llvm::CallSite cs{ u };
        if ( !cs )
            continue;
        llvm::InlineFunctionInfo ifi;
        llvm::InlineFunction( cs, ifi );
    }
}

}
#endif // LART_SUPPORT_UTIL_H
