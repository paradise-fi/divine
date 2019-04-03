// -*- C++ -*- (c) 2015 Petr Rockai <me@mornfall.net>
//             (c) 2015-2019 Vladimír Štill <xstill@fi.muni.cz>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/CallSite.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Analysis/CaptureTracking.h>
DIVINE_UNRELAX_WARNINGS
#include <brick-llvm>
#include <brick-string>
#include <unordered_set>
#include <string>
#include <iostream>

#include <lart/weakmem/pass.h>
#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/cleanup.h>
#include <lart/reduction/passes.h>
#include <lart/lart.h>

namespace lart {
namespace weakmem {

using namespace std::literals;

using LLVMFunctionSet = std::unordered_set< llvm::Function * >;

/*
 * Replace non-scalar load and store instructions with series of scalar-based
 * ones, along with requisite aggregate value (de)composition.
 */
struct ScalarMemory
{

    static PassMeta meta() {
        return passMeta< ScalarMemory >( "ScalarMemory", "breaks down loads and stores larger than 64 bits" );
    }

    static char ID;
    const unsigned _wordsize = 8;
    unsigned _silentID;

    void run( llvm::Module &m ) {
        _silentID = m.getMDKindID( reduction::silentTag );
        for ( auto &f : m )
            transform( f );
    }

    void transform( llvm::Function &f ) {
        std::vector< llvm::LoadInst * > loads;
        std::vector< llvm::StoreInst * > stores;

        for ( auto &bb : f )
            for ( auto &i : bb ) {
                if ( reduction::isSilent( i, _silentID ) )
                    continue;
                if ( auto load = llvm::dyn_cast< llvm::LoadInst >( &i ) )
                    loads.push_back( load );
                if ( auto store = llvm::dyn_cast< llvm::StoreInst >( &i ) )
                    stores.push_back( store );
            }
        for ( auto &s : stores )
            transform( s );
        for ( auto &l : loads )
            transform( l );
    }

    llvm::Type *nonscalar( llvm::Value *v ) {
        auto ty = v->getType();
        if ( ( ty->getPrimitiveSizeInBits() && ty->getPrimitiveSizeInBits() <= 8u * _wordsize )
             || ty->isPointerTy() )
            return nullptr; /* already scalar */
        if ( ty->getPrimitiveSizeInBits() )
            return nullptr; /* TODO */
        return ty;
    }

    llvm::Instruction *transform( llvm::LoadInst *load ) {
        auto ty = nonscalar( load );
        if ( !ty )
            return load;

        llvm::IRBuilder<> builder( load );
        llvm::Value *agg = llvm::UndefValue::get( ty );
        if ( auto sty = llvm::dyn_cast< llvm::CompositeType >( ty ) )
            for ( unsigned i = 0; i < sty->getNumContainedTypes(); ++i ) {
                auto geptr = builder.CreateConstGEP2_32( sty, load->getOperand(0), 0, i );
                auto val = builder.CreateLoad( geptr );
                auto tval = transform( val );
                agg = builder.CreateInsertValue( agg, tval, i );
            }
        load->replaceAllUsesWith( agg );
        load->eraseFromParent();
        return llvm::cast< llvm::Instruction >( agg );
    }

    llvm::Instruction *transform( llvm::StoreInst *store ) {
        auto ty = nonscalar( store->getValueOperand() );
        if ( !ty )
            return store;

        llvm::IRBuilder<> builder( store );
        llvm::Value *agg = store->getValueOperand();
        llvm::Instruction *replace = nullptr;
        if ( auto sty = llvm::dyn_cast< llvm::CompositeType >( ty ) )
            for ( unsigned i = 0; i < sty->getNumContainedTypes(); ++i ) {
                auto geptr = builder.CreateConstGEP2_32( sty, store->getPointerOperand(), 0, i );
                auto val = builder.CreateExtractValue( agg, i );
                auto nstore = builder.CreateStore( val, geptr );
                replace = transform( nstore );
            }
        if ( replace ) {
            store->replaceAllUsesWith( replace );
            store->eraseFromParent();
            return replace;
        }
        return store;
    }

};

struct Substitute {

    Substitute( int bufferSize )
        : _bufferSize( bufferSize )
    { }

    static MemoryOrder castmemord( llvm::AtomicOrdering mo ) {
        switch ( mo ) {
            case llvm::AtomicOrdering::NotAtomic: return MemoryOrder::NotAtomic;
            case llvm::AtomicOrdering::Unordered: return MemoryOrder::Unordered;
            case llvm::AtomicOrdering::Monotonic: return MemoryOrder::Monotonic;
            case llvm::AtomicOrdering::Acquire: return MemoryOrder::Acquire;
            case llvm::AtomicOrdering::Release: return MemoryOrder::Release;
            case llvm::AtomicOrdering::AcquireRelease: return MemoryOrder::AcqRel;
            case llvm::AtomicOrdering::SequentiallyConsistent: return MemoryOrder::SeqCst;
        }
    }

    enum class InstType { Load, Store, Other };

    static PassMeta meta() {
        return passMetaC( "Substitute",
                "Substitute loads and stores (and other memory manipulations) with appropriate "
                "weak memory model versions.",
                []( PassVector &ps, std::string opt ) {
                    std::stringstream ss( opt );
                    std::string par, model;
                    int bufferSize = -1;

                    std::getline( ss, model, ':' );
                    if ( std::getline( ss, par, ':' ) ) {
                        if ( !par.empty() )
                            bufferSize = std::stoi( par );
                    }
                    ASSERT( model == "tso" || model == "x86" );

                    ps.emplace_back< Substitute >( bufferSize );
                } );
    }

    auto getCallSites( llvm::Function *fn ) {
        return query::query( fn->users() )
                    .filter( query::is< llvm::CallInst > || query::is< llvm::InvokeInst > )
                    .filter( [&]( llvm::User *i ) {
                        return _bypass.count( llvm::cast< llvm::Instruction >( i )->getParent()->getParent() ) == 0;
                    } )
                    .map( []( llvm::User *i ) { return llvm::CallSite( i ); } )
                    .freeze();
    }

    void transformFree( llvm::Function *free ) {
        ASSERT( free );
        auto &ctx = free->getParent()->getContext();

        for ( auto &cs : getCallSites( free ) ) {
            std::vector< llvm::Value * > args;
            args.emplace_back( llvm::ConstantInt::get( llvm::Type::getInt32Ty( ctx ), 1 ) );
            args.emplace_back( cs.getArgOperand( 0 ) );
            llvm::CallInst::Create( _cleanup, args, "", cs.getInstruction() );
        }
    }

    void transformResize( llvm::Function *resize ) {
        ASSERT( resize );
        auto i32 = llvm::IntegerType::getInt32Ty( resize->getParent()->getContext() );

        for ( auto &cs : getCallSites( resize ) ) {
            std::vector< llvm::Value * > args;
            llvm::IRBuilder<> irb( cs.getInstruction() );
            args.push_back( cs.getArgOperand( 0 ) );
            auto size = cs.getArgOperand( 1 );
            if ( size->getType() != i32 ) {
                args.push_back( irb.CreateIntCast( size, i32, false ) );
            } else {
                args.push_back( size );
            }
            irb.CreateCall( _resize, args );
        }
    }

    void run( llvm::Module &m )
    {
        llvm::DataLayout dl( &m );

        auto get = [&m]( const char *n ) {
            auto *f = m.getFunction( n );
            if ( !f )
                UNREACHABLE( "could not find required function", n );
            return f;
        };

        _store = get( "__lart_weakmem_store" );
        _load = get( "__lart_weakmem_load" );
        _fence = get( "__lart_weakmem_fence" );
        _cas = get( "__lart_weakmem_cas" );
        _cleanup = get( "__lart_weakmem_cleanup" );
        _resize = get( "__lart_weakmem_resize" );
        _memmove = get( "memmove" );
        _memcpy = get( "memcpy" );
        _memset = get( "memset" );
        auto flusher = m.getFunction( "__lart_weakmem_flusher_main" );
        auto fsize = get( "__lart_weakmem_buffer_size" );
        auto dump = get( "__lart_weakmem_dump" );
        auto debug_fence = get( "__lart_weakmem_debug_fence" );
        _mask = get( "__lart_weakmem_mask_enter" );
        _unmask = get( "__lart_weakmem_mask_leave" );
        auto lart_init = get( "__lart_globals_initialize" );
        auto weakmem_init = get( "__lart_weakmem_init" );
        auto state = get( "__lart_weakmem_state" );

        _moTy = _fence->getFunctionType()->getParamType( 0 );

        util::Map< llvm::Function *, llvm::Function * > cloneMap;

        std::vector< llvm::Function * > inteface = { _store, _load, _fence, _cas, _cleanup, _resize,
                                                     fsize, debug_fence, state, weakmem_init };
        if ( flusher )
            inteface.push_back( flusher );
        for ( auto i : inteface ) {
            cloneMap.emplace( i, i );
        }

        auto not_vm = []( llvm::Function &fn ) { return !fn.getName().startswith( "__vm_" ); };
        auto const_null = []( auto & ) { return nullptr; };

        for ( auto i : inteface ) {
            cloneCalleesRecursively( i, cloneMap, not_vm, const_null );
        }

        using P = std::pair< llvm::Function *, llvm::Function ** >;
        for ( auto p : { P{ _memmove, &_scmemmove },
                         P{ _memcpy, &_scmemcpy },
                         P{ _memset, &_scmemset } } )
        {
            *p.second = cloneFunctionRecursively( p.first, cloneMap, not_vm, const_null );
        }

        /* Make sure divine.debugfn (DbgCall) functions do not use weakmem --
         * these functions run only in debug mode (trace, sim, draw), and must
         * not call __vm_choose. To ensure they see consistent memory, we
         * insert fence at their start, this is OK, as any changes to memory
         * performed by them are not persisted.
         */
        brick::llvm::Annotation debuganno( "divine.debugfn" ),
                                directanno( "lart.weakmem.direct" );
        brick::llvm::enumerateFunctionAnnos( m, [&]( llvm::Function *fn, const auto &anno )
            {
                if ( anno != directanno && anno != debuganno )
                    return;
                _bypass.emplace( fn );
                if ( anno == debuganno && fn != dump ) {
                    llvm::IRBuilder<> irb( &*fn->begin()->getFirstInsertionPt() );
                    irb.CreateCall( debug_fence, { } );
                }
            } );

        for ( auto p : cloneMap ) {
            _bypass.emplace( p.second );
        }

        auto *free = m.getFunction( "__vm_obj_free" ) ?: m.getFunction( "free" );
        transformFree( free );
        if ( auto *resize = m.getFunction( "__vm_obj_resize" ) )
            transformResize( resize );

        auto silentID = m.getMDKindID( reduction::silentTag );

        for ( auto &f : m ) {
            if ( _bypass.count( &f ) )
                transformBypass( f );
            else
                transformWeak( f, dl, silentID );
        }

        if ( _bufferSize >= 0 ) {
            makeReturnConstant( fsize, _bufferSize );
        }
        inlineIntoCallers( fsize );
        mk_init( lart_init, weakmem_init, state );
    }

    void mk_init( llvm::Function *lart_init, llvm::Function *weakmem_init, llvm::Function *weakmem_state )
    {
        auto &m = *weakmem_state->getParent();
        auto state_ty = llvm::cast< llvm::PointerType >( weakmem_state->getReturnType() )
                                                       ->getElementType();
        auto *state_glob = new llvm::GlobalVariable( m, state_ty, false,
                                      llvm::GlobalValue::ExternalLinkage,
                                      llvm::ConstantAggregateZero::get( state_ty ),
                                      "__lart_weakmem_state_var" );

        weakmem_state->setLinkage( llvm::GlobalValue::ExternalLinkage );
        auto *wminit_bb = &*weakmem_state->begin();
        wminit_bb->begin()->eraseFromParent();
        llvm::IRBuilder<> irb( wminit_bb );
        irb.CreateRet( state_glob );

        irb.SetInsertPoint( &*lart_init->begin()->getFirstInsertionPt() );
        irb.CreateCall( weakmem_init, { } );

        inlineIntoCallers( weakmem_state );
    }

  private:

    static void makeReturnConstant( llvm::Function *f, int val ) {
        f->deleteBody();
        auto *bb = llvm::BasicBlock::Create( f->getContext(), "entry", f );
        llvm::IRBuilder<> irb( bb );
        irb.CreateRet( llvm::ConstantInt::getSigned( f->getReturnType(), val ) );
    }

    llvm::Type *intTypeOfSize( int size, llvm::LLVMContext &ctx ) {
        ASSERT_LEQ( size, 8 );
        return llvm::IntegerType::get( ctx, size * 8 );
    }

    llvm::Value *getSize( llvm::Type *type, llvm::LLVMContext &ctx, llvm::DataLayout &dl ) {
        return llvm::ConstantInt::get( llvm::IntegerType::getInt32Ty( ctx ),
                    type->isPointerTy()
                        ? dl.getPointerSize()
                        : std::max< int >( 1, type->getPrimitiveSizeInBits() / 8 ) );
    }

    // in bypass functions we replace all memory transfer intrinsics with clones
    // of original functions which will not be transformed
    void transformBypass( llvm::Function &fn ) {
        transformMemTrans< llvm::MemSetInst >( fn, _scmemset );
        transformMemTrans< llvm::MemCpyInst >( fn, _scmemcpy );
        transformMemTrans< llvm::MemMoveInst >( fn, _scmemmove );
    }

    llvm::Instruction::CastOps castOpFrom( llvm::Type *from, llvm::Type *to, bool isSigned = false ) {
        if ( from->getScalarSizeInBits() == to->getScalarSizeInBits() )
            return llvm::Instruction::BitCast;
        if ( from->getScalarSizeInBits() > to->getScalarSizeInBits() )
            return llvm::Instruction::Trunc;
        if ( isSigned )
            return llvm::Instruction::SExt;
        return llvm::Instruction::ZExt;
    }

    template< typename IntrinsicType >
    void transformMemTrans( llvm::Function &fn, llvm::Function *rfn ) {
        ASSERT( rfn );

        auto *repTy = rfn->getFunctionType();
        auto *dstTy = repTy->getParamType( 0 );
        auto *srcTy = repTy->getParamType( 1 );
        auto *lenTy = repTy->getParamType( 2 );

        auto mems = query::query( fn ).flatten()
                      .map( query::refToPtr )
                      .map( query::llvmdyncast< IntrinsicType > )
                      .filter( query::notnull )
                      .freeze();

        for ( auto mem : mems ) {
            llvm::IRBuilder<> builder( mem );
            auto dst = mem->getDest();
            if ( dst->getType() != dstTy )
                dst = builder.CreateCast( llvm::Instruction::BitCast, dst, dstTy );
            auto src = mem->getArgOperand( 1 )->stripPointerCasts();
            if ( src->getType() != srcTy )
                src = builder.CreateCast( castOpFrom( src->getType(), srcTy, true ), src, srcTy );
            auto len = mem->getLength();
            if ( len->getType() != lenTy )
                len = builder.CreateCast( castOpFrom( len->getType(), lenTy, true ), len, lenTy );
            auto replace = builder.CreateCall( rfn, { dst, src, len } );
            if ( mem->getType() != builder.getVoidTy() )
                mem->replaceAllUsesWith( replace );
            mem->eraseFromParent();
        }
    }

    void transformWeak( llvm::Function &f, llvm::DataLayout &dl, unsigned silentID )
    {
        using OpCode = llvm::Instruction;

        std::vector< llvm::AtomicCmpXchgInst * > cass;
        std::vector< llvm::AtomicRMWInst * > ats;
        std::vector< llvm::StoreInst * > stores;
        std::vector< llvm::LoadInst * > loads;
        std::vector< llvm::FenceInst * > fences;
        util::Map< llvm::Instruction *, llvm::Value * > masks;
        util::Set< llvm::Instruction * > atomicOps;

        for ( auto *i : query::query( f ).flatten().map( query::refToPtr ).freeze() )
        {
            if ( reduction::isSilent( *i, silentID ) )
                continue;
            switch ( i->getOpcode() )
            {
                case OpCode::AtomicRMW:
                    ats.push_back( llvm::cast< llvm::AtomicRMWInst >( i ) );
                    goto mask;
                case OpCode::AtomicCmpXchg:
                    cass.push_back( llvm::cast< llvm::AtomicCmpXchgInst >( i ) );
                    goto mask;
                case OpCode::Store:
                    stores.push_back( llvm::cast< llvm::StoreInst >( i ) );
                    goto mask;
                case OpCode::Load:
                    loads.push_back( llvm::cast< llvm::LoadInst >( i ) );
                    goto mask;
                case OpCode::Fence:
                    fences.push_back( llvm::cast< llvm::FenceInst >( i ) );
                  mask:
                    {
                        llvm::BasicBlock::iterator instr_it( i );
                        llvm::IRBuilder<> irb( &*instr_it );
                        auto m = masks[ i ] = irb.CreateCall( _mask, { } );
                        ++instr_it;
                        irb.SetInsertPoint( &*instr_it );
                        irb.CreateCall( _unmask, { m } );
                        break;
                    }
                default: break;
            }
        }

        auto check_terminators = [&] {
#ifndef NDEBUG
            for ( auto &bb : f )
                ASSERT( bb.getTerminator() );
#endif
        };

        check_terminators();

        auto &ctx = f.getContext();
        auto *i8ptr = llvm::Type::getInt8PtrTy( ctx );
        auto *i64 = llvm::Type::getInt64Ty( ctx );

        auto addr_cast = [i8ptr]( llvm::Value *op, llvm::IRBuilder<> &builder )
                        -> llvm::Value *
        {
            return builder.CreateBitOrPointerCast( op, i8ptr );
        };
        auto value_cast = [i64]( llvm::Value *op, llvm::IRBuilder<> &builder ) {
            auto opty = op->getType();
            ASSERT_LEQ( opty->getScalarSizeInBits(), 64 );
            if ( opty->isPointerTy() )
                return builder.CreateBitOrPointerCast( op, i64 );
            else if ( opty->isFloatingPointTy() )
                op = builder.CreateBitCast( op,
                                builder.getIntNTy( opty->getScalarSizeInBits() ) );
            return builder.CreateIntCast( op, i64, false );
        };

        auto result_cast = []( llvm::Value *orig, llvm::Type *dstt, llvm::IRBuilder<> &builder ) {
            ASSERT_LEQ( dstt->getScalarSizeInBits(), 64 );
            if ( dstt->isFloatingPointTy() ) {
                auto *i = builder.CreateIntCast( orig,
                              builder.getIntNTy( dstt->getScalarSizeInBits() ), false );
                return builder.CreateBitCast( i, dstt );
            }
            else if ( dstt->isIntegerTy() )
                return builder.CreateIntCast( orig, dstt, false );
            return builder.CreateBitOrPointerCast( orig, dstt );
        };

        auto sz = [&]( llvm::Value *op ) {
            return getSize( op->getType(), ctx, dl );
        };

        auto mo_ = [this]( MemoryOrder mo, bool atomic = false ) {
            return llvm::ConstantInt::get( _moTy, uint64_t( mo | (atomic ? MemoryOrder::AtomicOp : MemoryOrder(0)) ) );
        };
        auto mo = brick::types::overloaded( mo_,
                    [mo_]( llvm::AtomicOrdering mo, bool atomic = false ) {
                        return mo_( castmemord( mo ), atomic );
                    } );

        auto get_mask = [&masks]( llvm::Instruction *i ) {
            auto it = masks.find( i );
            ASSERT( it != masks.end() );
            return it->second;
        };

        for ( auto *cas : cass )
        {
            auto casord = (cas->isWeak() ? MemoryOrder::WeakCAS : MemoryOrder( 0 ))
                          | MemoryOrder::AtomicOp;
            auto succord = castmemord( cas->getSuccessOrdering() ) | casord;
            auto failord = castmemord( cas->getFailureOrdering() ) | casord;
            /* transform:
             *    something
             *    %valsucc = cmpxchg %ptr, %cmp, %val succord failord
             *    something_else
             *
             * into:
             *    something
             *    %orig = call wm_cas(%ptr, %cmp, %val, sz, succord, failord)
             *    %eq = icmp eq %orig, %cmp
             *    %r0 = insertvalue undef, %orig, 0
             *    %valsucc = insertvalue %0, %eq, 1
             *    something_else
            */
            auto *ptr = cas->getPointerOperand();
            auto *cmp = cas->getCompareOperand();
            auto *val = cas->getNewValOperand();
            auto *rt = llvm::cast< llvm::StructType >( cas->getType() );
            auto *rtval = rt->getElementType( 0 );
            auto *rtbool = rt->getElementType( 1 );

            llvm::IRBuilder<> builder( cas );
            auto addr = addr_cast( ptr, builder );
            auto mask = get_mask( cas );
            ASSERT_EQ( rtbool, builder.getInt1Ty() );
            auto *raw = builder.CreateCall( _cas, { addr,
                                                    value_cast( cmp, builder ),
                                                    value_cast( val, builder ),
                                                    sz( val ),
                                                    mo( succord, true ),
                                                    mo( failord, true ),
                                                    mask } );
            auto *orig = builder.CreateExtractValue( raw, { 0 } );
            auto *req = builder.CreateExtractValue( raw, { 1 } );
            auto *eq = builder.CreateICmpNE( req, llvm::ConstantInt::get( req->getType(), 0 ) );
            orig = result_cast( orig, rtval, builder );
            auto *r0 = builder.CreateInsertValue( llvm::UndefValue::get( rt ), orig, { 0 } );
            auto *r = llvm::cast< llvm::Instruction >( builder.CreateInsertValue( r0, eq, { 1 } ) );
            r->removeFromParent();
            llvm::ReplaceInstWithInst( cas, r );
        }

        for ( auto *at : ats )
        {
            auto aord = at->getOrdering();
            auto *ptr = at->getPointerOperand();
            auto *val = at->getValOperand();
            auto op = at->getOperation();

            llvm::IRBuilder<> irb( at );
            auto *mask = get_mask( at );
            auto *orig = irb.CreateLoad( ptr, "lart.weakmem.atomicrmw.orig" );
            masks[ orig ] = mask;
            orig->setAtomic( aord == llvm::AtomicOrdering::AcquireRelease
                                ? llvm::AtomicOrdering::Acquire
                                : aord );
            loads.push_back( orig );
            atomicOps.insert( orig );

            switch ( op ) {
                case llvm::AtomicRMWInst::Xchg: break;
                case llvm::AtomicRMWInst::Add:
                    val = irb.CreateAdd( orig, val );
                    break;
                case llvm::AtomicRMWInst::Sub:
                    val = irb.CreateSub( orig, val );
                    break;
                case llvm::AtomicRMWInst::And:
                    val = irb.CreateAnd( orig, val );
                    break;
                case llvm::AtomicRMWInst::Nand:
                    val = irb.CreateNot( irb.CreateAnd( orig, val ) );
                    break;
                case llvm::AtomicRMWInst::Or:
                    val = irb.CreateOr( orig, val );
                    break;
                case llvm::AtomicRMWInst::Xor:
                    val = irb.CreateXor( orig, val );
                    break;
                case llvm::AtomicRMWInst::Max:
                    val = irb.CreateSelect( irb.CreateICmpSGT( orig, val ), orig, val );
                    break;
                case llvm::AtomicRMWInst::Min:
                    val = irb.CreateSelect( irb.CreateICmpSLT( orig, val ), orig, val );
                    break;
                case llvm::AtomicRMWInst::UMax:
                    val = irb.CreateSelect( irb.CreateICmpUGT( orig, val ), orig, val );
                    break;
                case llvm::AtomicRMWInst::UMin:
                    val = irb.CreateSelect( irb.CreateICmpULT( orig, val ), orig, val );
                    break;
                case llvm::AtomicRMWInst::BAD_BINOP:
                    UNREACHABLE( "weakmem: bad binop in AtomicRMW:", at );
            }

            auto *store = irb.CreateStore( val, ptr );
            masks[ store ] = mask;
            store->setAtomic( aord == llvm::AtomicOrdering::AcquireRelease
                                ? llvm::AtomicOrdering::Release
                                : aord );
            stores.push_back( store );
            atomicOps.insert( store );

            at->replaceAllUsesWith( orig );
            at->eraseFromParent();
        }

        for ( auto load : loads )
        {
            auto ptr = load->getPointerOperand();
            auto ptrty = llvm::cast< llvm::PointerType >( ptr->getType() );
            auto ety = ptrty->getElementType();
            if ( ety->getPrimitiveSizeInBits() > 64 )
                continue; // TODO
            ASSERT( ety->isPointerTy() || ety->getPrimitiveSizeInBits() || (ety->dump(), false) );

            llvm::IRBuilder<> builder( load );
            auto *addr = addr_cast( ptr, builder );

            auto mask = get_mask( load );
            auto call = builder.CreateCall( _load, { addr, getSize( ety, ctx, dl ),
                                                     mo( load->getOrdering(), atomicOps.count( load ) ),
                                                     mask } );
            llvm::Value *result = result_cast( call, ety, builder );

            load->replaceAllUsesWith( result );
            load->eraseFromParent();
        }

        for ( auto store : stores )
        {
            auto value = store->getValueOperand();
            auto ptr = store->getPointerOperand();

            llvm::IRBuilder<> builder( store );
            auto addr = addr_cast( ptr, builder );
            auto mask = get_mask( store );

            if ( value->getType()->getPrimitiveSizeInBits() > 64 )
                continue; // TODO

            auto call = builder.CreateCall( _store, { addr, value_cast( value, builder ),
                                                      sz( value ), mo( store->getOrdering(), atomicOps.count( store ) ),
                                                      mask } );
            store->replaceAllUsesWith( call );
            store->eraseFromParent();
        }

        for ( auto fence : fences )
        {
            auto callFlush = llvm::CallInst::Create( _fence, {
                                      mo( fence->getOrdering() ), get_mask( fence ) } );
            llvm::ReplaceInstWithInst( fence, callFlush );
        }

        check_terminators();

        // add cleanups
        cleanup::addAllocaCleanups( cleanup::EhInfo::cpp( *f.getParent() ), f,
            [&]( llvm::AllocaInst *alloca ) {
                return !reduction::isSilent( *alloca, silentID );
            },
            [&]( llvm::Instruction *insPoint, auto &allocas ) {
                if ( allocas.empty() )
                    return;

                std::vector< llvm::Value * > args;
                llvm::IRBuilder<> irb( insPoint );
                args.emplace_back( irb.getInt32( allocas.size() ) );

                args.insert( args.end(), allocas.begin(), allocas.end() );
                auto c = irb.CreateCall( _cleanup, args );
                ASSERT( c->getParent()->getTerminator() );
            } );

        check_terminators();
    }

    int _bufferSize;
    LLVMFunctionSet _bypass;
    llvm::Function *_store = nullptr, *_load = nullptr, *_fence = nullptr, *_cas = nullptr;
    llvm::Function *_memmove = nullptr, *_memcpy = nullptr, *_memset = nullptr;
    llvm::Function *_scmemmove = nullptr, *_scmemcpy = nullptr, *_scmemset = nullptr;
    llvm::Function *_cleanup = nullptr, *_resize = nullptr;
    llvm::Function *_mask = nullptr, *_unmask = nullptr;
    llvm::Type *_moTy = nullptr;
};

PassMeta meta() {
    return passMetaC< ScalarMemory, Substitute >( "weakmem",
            "Transform SC code to code with given weak memory order approximation\n"
            "\n"
            "options: [ x86 | tso ]:BUFFER_SIZE\n",

            []( PassVector &mgr, std::string opt ) {
                ScalarMemory::meta().create( mgr, "" );
                Substitute::meta().create( mgr, opt );
            } );
}

}
}
