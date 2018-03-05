// -*- C++ -*- (c) 2015 Petr Rockai <me@mornfall.net>
//             (c) 2015,2017 Vladimír Štill <xstill@fi.muni.cz>

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
#include <lart/support/error.h>
#include <lart/reduction/passes.h>
#include <runtime/abstract/weakmem.h>

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

    void run( llvm::Module &m ) {
        for ( auto &f : m )
            transform( f );
    }

    void transform( llvm::Function &f ) {
        std::vector< llvm::LoadInst * > loads;
        std::vector< llvm::StoreInst * > stores;

        for ( auto &bb : f )
            for ( auto &i : bb ) {
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

    struct OrderConfig {
        MemoryOrder load = MemoryOrder::Unordered;
        MemoryOrder store = MemoryOrder::Unordered;
        MemoryOrder rmw = MemoryOrder::Monotonic;
        MemoryOrder casFail = MemoryOrder::Monotonic;
        MemoryOrder casOK = MemoryOrder::Monotonic;
        MemoryOrder fence = MemoryOrder::Monotonic;

        void setAll( MemoryOrder mo ) {
            load = store = mo;
            rmw = mo | MemoryOrder::Monotonic;
            casFail = mo | MemoryOrder::Monotonic;
            casOK = mo | MemoryOrder::Monotonic;
            fence = mo | MemoryOrder::Monotonic;
        }

        void setCas( MemoryOrder mo ) { casFail = casOK = mo; }
    };

    Substitute( OrderConfig config, int bufferSize )
        : _config( config ), _bufferSize( bufferSize )
    { }

    static llvm::AtomicOrdering castmemord( MemoryOrder mo ) {
        mo = MemoryOrder( uint32_t( mo )
                        & ~uint32_t( MemoryOrder::AtomicOp | MemoryOrder::WeakCAS ) );
        switch ( mo ) {
            case MemoryOrder::NotAtomic: return llvm::AtomicOrdering::NotAtomic;
            case MemoryOrder::Unordered: return llvm::AtomicOrdering::Unordered;
            case MemoryOrder::Monotonic: return llvm::AtomicOrdering::Monotonic;
            case MemoryOrder::Acquire: return llvm::AtomicOrdering::Acquire;
            case MemoryOrder::Release: return llvm::AtomicOrdering::Release;
            case MemoryOrder::AcqRel: return llvm::AtomicOrdering::AcquireRelease;
            case MemoryOrder::SeqCst: return llvm::AtomicOrdering::SequentiallyConsistent;
            case MemoryOrder::AtomicOp:
            case MemoryOrder::WeakCAS:
                UNREACHABLE( "cannot happen" );
        }
    }

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

    MemoryOrder usememord( MemoryOrder x, InstType it = InstType::Other ) {
        auto check = x;
        if ( ( check == MemoryOrder::Release && it == InstType::Store )
            || ( check == MemoryOrder::Acquire && it == InstType::Load ) )
            check = MemoryOrder::AcqRel;
        if ( subseteq( check, _minMemOrd ) )
            _minMemOrd = check;
        return x;
    }

    llvm::AtomicOrdering usememord( llvm::AtomicOrdering x, InstType it = InstType::Other ) {
        usememord( castmemord( x ), it );
        return x;
    }

    static PassMeta meta() {
        return passMetaC( "Substitute",
                "Substitute loads and stores (and other memory manipulations) with appropriate "
                "weak memory model versions.",
                []( PassVector &ps, std::string opt ) {
                    OrderConfig config;

                    auto c = opt.find( ':' );
                    int bufferSize = -1;
                    if ( c != std::string::npos ) {
                        bufferSize = std::stoi( opt.substr( c + 1 ) );
                        opt = opt.substr( 0, c );
                    }

                    if ( opt == "sc" ) {
                        config.setAll( MemoryOrder::SeqCst );
                    } else if ( opt == "x86" ) {
                        config.setAll( MemoryOrder::AcqRel );
                        config.setCas( MemoryOrder::SeqCst );
                        config.rmw = MemoryOrder::SeqCst;
                    } else if ( opt == "tso" ) {
                        config.setAll( MemoryOrder::AcqRel );
                    } else if ( opt == "pso" ) {
                        // no reconfiguration
                    } else {
                        while ( !opt.empty() ) {
                            auto s = opt.find( ',' );
                            auto sub = opt.substr( 0, s );
                            opt = opt.substr( s + 1 );
                            s = sub.find( '=' );
                            auto type = sub.substr( 0, s );
                            auto spec = sub.substr( s + 1 );
                            if ( type.empty() )
                                throw std::runtime_error( "empty atomic instruction type specification" );
                            if ( spec.empty() )
                                throw std::runtime_error( "empty atomic ordering specification" );

                            MemoryOrder mo;
                            if ( spec == "unordered" )
                                mo = MemoryOrder::Unordered;
                            else if ( spec == "relaxed" )
                                mo = MemoryOrder::Monotonic;
                            else if ( spec == "acquire" )
                                mo = MemoryOrder::Acquire;
                            else if ( spec == "release" )
                                mo = MemoryOrder::Release;
                            else if ( spec == "acq_rel" )
                                mo = MemoryOrder::AcqRel;
                            else if ( spec == "seq_cst" )
                                mo = MemoryOrder::SeqCst;
                            else
                                throw std::runtime_error( "invalid atomic ordering specification: " + spec );

                            if ( type == "all" )
                                config.setAll( mo );
                            else if ( type == "load" )
                                config.load = mo;
                            else if ( type == "store" )
                                config.store = mo;
                            else if ( type == "armw" )
                                config.rmw = mo;
                            else if ( type == "cas" )
                                config.setCas( mo );
                            else if ( type == "casfail" )
                                config.casFail = mo;
                            else if ( type == "casok" )
                                config.casOK = mo;
                            else if ( type == "fence" )
                                config.fence = mo;
                            else
                                throw std::runtime_error( "invalid instruction type specification: " + type );
                        }
                    }

                    ps.emplace_back< Substitute >( config, bufferSize );
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
            ENSURE_LLVM( f, n, " is null" );
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
        auto ford = get( "__lart_weakmem_min_ordering" );
        auto dump = get( "__lart_weakmem_dump" );
        auto debug_fence = get( "__lart_weakmem_debug_fence" );
        _mask = get( "__dios_mask" );

        _moTy = _fence->getFunctionType()->getParamType( 0 );

        util::Map< llvm::Function *, llvm::Function * > cloneMap, debugCloneMap;

        std::vector< llvm::Function * > inteface = { _store, _load, _fence, _cas, _cleanup, _resize,
                                                     fsize, ford, debug_fence };
        if ( flusher )
            inteface.push_back( flusher );
        for ( auto i : inteface ) {
            cloneMap.emplace( i, i );
        }

        auto isvm = []( llvm::Function &fn ) { return fn.getName().startswith( "__vm_" ); };

        for ( auto i : inteface ) {
            cloneCalleesRecursively( i, cloneMap,
                 [&]( auto &fn ) { return !isvm( fn ); },
                 []( auto & ) { return nullptr; } );
        }

        using P = std::pair< llvm::Function *, llvm::Function ** >;
        for ( auto p : { P{ _memmove, &_scmemmove },
                         P{ _memcpy, &_scmemcpy },
                         P{ _memset, &_scmemset } } )
        {
            *p.second = cloneFunctionRecursively( p.first, cloneMap,
                             [&]( auto &fn ) { return !isvm( fn ); },
                             []( auto & ) { return nullptr; } );
        }

        /* Make sure divine.debugfn (DbgCall) functions do not use weakmem --
         * these functions run only in debug mode (trace, sim, draw), and must
         * not call __vm_choose. To ensure they see consistent memory, we
         * insert fence at their start, this is OK, as any changes to memory
         * performed by them are not persisted.
         */
        brick::llvm::enumerateFunctionsForAnno( "divine.debugfn", m, [&]( llvm::Function *fn )
            {
                // NOTE: clone map must not be shared, as we can clone
                // functions behind function pointers here
                cloneCalleesRecursively( fn, debugCloneMap,
                                         [&]( auto &fn ) { return !isvm( fn ); },
                                         []( auto & ) { return nullptr; },
                                         true );
                _bypass.emplace( fn );
                if ( fn != dump ) {
                    llvm::IRBuilder<> irb( fn->begin()->getFirstInsertionPt() );
                    irb.CreateCall( debug_fence, { } );
                }
            } );

        for ( auto p : cloneMap ) {
            _bypass.emplace( p.second );
        }
        for ( auto p : debugCloneMap ) {
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

        if ( _bufferSize > 0 ) {
            makeReturnConstant( fsize, _bufferSize );
        }
        inlineIntoCallers( fsize );

        if ( _minMemOrd != MemoryOrder::NotAtomic ) {
            makeReturnConstant( ford, int( _minMemOrd ) );
        }
        inlineIntoCallers( ford );
    }

  private:

    static void makeReturnConstant( llvm::Function *f, int val ) {
        f->deleteBody();
        auto *bb = llvm::BasicBlock::Create( f->getContext(), "entry", f );
        llvm::IRBuilder<> irb( bb );
        irb.CreateRet( llvm::ConstantInt::getSigned( f->getReturnType(), val ) );
    }

    llvm::Type *intTypeOfSize( int size, llvm::LLVMContext &ctx ) {
        ASSERT_LEQ( size, 64 );
        return llvm::IntegerType::get( ctx, size );
    }

    llvm::Value *getBitwidth( llvm::Type *type, llvm::LLVMContext &ctx, llvm::DataLayout &dl ) {
        return llvm::ConstantInt::get( llvm::IntegerType::getInt32Ty( ctx ),
                                        type->isPointerTy()
                                            ? dl.getPointerSize() * 8
                                            : type->getPrimitiveSizeInBits() );
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
        if ( !rfn ) // we might be just transformimg memmove/memcpy/memset
            return; //so this is not set yet

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

    // for weak memory functions we have to:
    // *  transform all loads and stores to appropriate lart function
    // *  transform all memcpy/memmove (both calls and intrinsics) to call to lart memcpy/memmove
    // *  transform fence instructions to call to lart fence
    // *  for TSO: insert PSO -> TSO fence at the beginning and after call to
    //    any function which is neither TSO, SC, or bypass
    void transformWeak( llvm::Function &f, llvm::DataLayout &dl, unsigned silentID )
    {
        auto &ctx = f.getContext();
        auto *i8ptr = llvm::Type::getInt8PtrTy( ctx );
        auto *i64 = llvm::Type::getInt64Ty( ctx );
        auto *i32 = llvm::Type::getInt32Ty( ctx );

        auto addrCast = [i8ptr]( llvm::Value *op, llvm::IRBuilder<> &builder )
        {
            auto ety = llvm::cast< llvm::PointerType >( op->getType() )->getElementType();
            if ( !ety->isIntegerTy() || ety->getPrimitiveSizeInBits() != 8 ) {
                return builder.CreateBitCast( op, i8ptr );
            }
            return op;
        };
        auto i64Cast = [i64]( llvm::Value *op, llvm::IRBuilder<> &builder ) {
            return builder.CreateIntCast( op, i64, false );
        };

        auto bw = [&, i32]( llvm::Value *op ) {
            return getBitwidth( op->getType(), ctx, dl );
        };

        auto mo = [this]( MemoryOrder mo ) {
            return llvm::ConstantInt::get( _moTy, uint64_t( usememord( mo ) ) );
        };

        // first translate atomics, so that if they are translated to
        // non-atomic equivalents under mask these are later converted to TSO
        std::vector< llvm::AtomicCmpXchgInst * > cass;
        std::vector< llvm::AtomicRMWInst * > ats;
        std::set< llvm::Instruction * > atomicOps;

        for ( auto &i : query::query( f ).flatten() ) {
            if ( reduction::isSilent( i, silentID ) )
                continue;
            llvmcase( i,
                [&]( llvm::AtomicCmpXchgInst *cas ) {
                    cass.push_back( cas );
                },
                [&]( llvm::AtomicRMWInst *at ) {
                    ats.push_back( at );
                } );
        }

        for ( auto cas : cass ) {
            auto casord = (cas->isWeak() ? MemoryOrder::WeakCAS : MemoryOrder( 0 ))
                          | MemoryOrder::AtomicOp;
            auto succord = castmemord( cas->getSuccessOrdering() ) | _config.casOK | casord;
            auto failord = castmemord( cas->getFailureOrdering() ) | _config.casFail | casord;
            /* transform:
             *    something
             *    %valsucc = cmpxchg %ptr, %cmp, %val succord failord
             *    something_else
             *
             * into:
             *    something
             *    %orig = call wm_cas(%ptr, %cmp, %val, bw, succord, failord)
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
            ASSERT_EQ( rtbool, builder.getInt1Ty() );
            auto *raw = builder.CreateCall( _cas, { addrCast( ptr, builder ),
                                                    i64Cast( cmp, builder ),
                                                    i64Cast( val, builder ),
                                                    bw( val ),
                                                    mo( succord ), mo( failord ) } );
            auto *orig = builder.CreateExtractValue( raw, { 0 } );
            auto *req = builder.CreateExtractValue( raw, { 1 } );
            auto *eq = builder.CreateICmpNE( req, llvm::ConstantInt::get( req->getType(), 0 ) );
            if ( orig->getType() != rtval ) {
                if ( rtval->isIntegerTy() )
                    orig = builder.CreateIntCast( orig, rtval, false );
                else
                    orig = builder.CreateBitOrPointerCast( orig, rtval );
            }
            auto *r0 = builder.CreateInsertValue( llvm::UndefValue::get( rt ), orig, { 0 } );
            auto *r = llvm::cast< llvm::Instruction >( builder.CreateInsertValue( r0, eq, { 1 } ) );
            r->removeFromParent();
            llvm::ReplaceInstWithInst( cas, r );
        }
        for ( auto *at : ats ) {
            auto aord = castmemord( castmemord( at->getOrdering() ) | _config.rmw );
            auto *ptr = at->getPointerOperand();
            auto *val = at->getValOperand();
            auto op = at->getOperation();

            llvm::IRBuilder<> irb( at );
            auto *mask = irb.CreateCall( _mask, { irb.getInt32( 1 ) } );
            auto *orig = irb.CreateLoad( ptr, "lart.weakmem.atomicrmw.orig" );
            orig->setAtomic( usememord( aord == llvm::AtomicOrdering::AcquireRelease ? llvm::AtomicOrdering::Acquire : aord, InstType::Load ) );
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
                    at->dump();
                    UNREACHABLE( "weakmem: bad binop in AtomicRMW" );
            }

            auto *store = irb.CreateStore( val, ptr );
            store->setAtomic( usememord( aord == llvm::AtomicOrdering::AcquireRelease
                        ? llvm::AtomicOrdering::Release : aord, InstType::Store ) );
            atomicOps.insert( store );
            irb.CreateCall( _mask, { mask } );

            at->replaceAllUsesWith( orig );
            at->eraseFromParent();
        }

        for ( auto &bb : f )
            ASSERT( bb.getTerminator() );

        // now translate load/store/fence
        std::vector< llvm::LoadInst * > loads;
        std::vector< llvm::StoreInst * > stores;
        std::vector< llvm::FenceInst * > fences;

        for ( auto &i : query::query( f ).flatten() ) {
            if ( reduction::isSilent( i, silentID ) )
                continue;
            llvmcase( i,
                [&]( llvm::LoadInst *load ) {
                    loads.push_back( load );
                },
                [&]( llvm::StoreInst *store ) {
                    stores.push_back( store );
                },
                [&]( llvm::FenceInst *fence ) {
                    fences.push_back( fence );
                } );
        }

        for ( auto load : loads ) {

            auto op = load->getPointerOperand();
            auto opty = llvm::cast< llvm::PointerType >( op->getType() );
            auto ety = opty->getElementType();
            ASSERT( ety->isPointerTy() || ety->getPrimitiveSizeInBits() || (ety->dump(), false) );

            llvm::IRBuilder<> builder( load );

            // first cast address to i8*
            llvm::Value *addr = op;
            if ( !ety->isIntegerTy() || ety->getPrimitiveSizeInBits() != 8 ) {
                auto ty = llvm::PointerType::get( llvm::IntegerType::getInt8Ty( ctx ), opty->getAddressSpace() );
                addr = builder.CreateBitCast( op, ty );
            }
            auto bitwidth = getBitwidth( ety, ctx, dl );
            auto call = builder.CreateCall( _load, { addr, bitwidth,
                        llvm::ConstantInt::get( _moTy, uint64_t( usememord( _config.load | castmemord( load->getOrdering() ), InstType::Load ) ) )
                    } );
            llvm::Value *result = call;

            // weak load and final cast i64 -> target type
            if ( ety->isPointerTy() )
                result = builder.CreateCast( llvm::Instruction::IntToPtr, result, ety );
            else {
                auto size = ety->getPrimitiveSizeInBits();
                if ( size > 64 ) // TODO
                    continue;
                ASSERT_LEQ( 1u, size );
                ASSERT_LEQ( size, 64u );
                if ( size < 64 )
                    result = builder.CreateCast( llvm::Instruction::Trunc, result, intTypeOfSize( size, ctx ) );
                if ( !ety->isIntegerTy() )
                    result = builder.CreateCast( llvm::Instruction::BitCast, result, ety );
            }
            load->replaceAllUsesWith( result );
            load->eraseFromParent();
        }

        for ( auto store : stores ) {

            auto value = store->getValueOperand();
            auto addr = store->getPointerOperand();
            auto vty = value->getType();
            auto aty = llvm::cast< llvm::PointerType >( addr->getType() );

            llvm::IRBuilder<> builder( store );

            // void * is translated to i8*
            auto i8 = llvm::IntegerType::getInt8Ty( ctx );
            auto i8ptr = llvm::PointerType::get( i8, aty->getAddressSpace() );
            if ( aty != i8ptr )
                addr = builder.CreateBitCast( addr, i8ptr );

            auto i64 = llvm::IntegerType::getInt64Ty( ctx );
            if ( vty->isPointerTy() )
                value = builder.CreateCast( llvm::Instruction::PtrToInt, value, i64 );
            else {
                auto size = vty->getPrimitiveSizeInBits();
                // ASSERT( size > 0 );
                if ( size == 0 || size > 64 )
                    continue; // TODO
                ASSERT_LEQ( size, 64u );
                if ( !vty->isIntegerTy() )
                    value = builder.CreateCast( llvm::Instruction::BitCast, value, intTypeOfSize( size, ctx ) );
                if ( size < 64 )
                    value = builder.CreateCast( castOpFrom( vty, i64 ), value, i64 );
            }

            auto bitwidth = getBitwidth( vty, ctx, dl );
            auto storeCall = builder.CreateCall( _store, { addr, value, bitwidth,
                        llvm::ConstantInt::get( _moTy, uint64_t( usememord( _config.store | castmemord( store->getOrdering() ), InstType::Store ) ) )
                    } );
            store->replaceAllUsesWith( storeCall );
            store->eraseFromParent();
        }
        for ( auto fence : fences ) {
            auto callFlush = llvm::CallInst::Create( _fence, { llvm::ConstantInt::get(
                        _moTy, uint64_t( usememord( _config.fence | castmemord( fence->getOrdering() ) ) ) ) } );
            llvm::ReplaceInstWithInst( fence, callFlush );
        }

        for ( auto &bb : f )
            ASSERT( bb.getTerminator() );

        // add cleanups
        cleanup::addAllocaCleanups( cleanup::EhInfo::cpp( *f.getParent() ), f,
            [&]( llvm::AllocaInst *alloca ) {
                return !reduction::isSilent( *alloca, silentID );
            },
            [&]( llvm::Instruction *insPoint, auto &allocas ) {
                if ( allocas.empty() )
                    return;

                std::vector< llvm::Value * > args;
                args.emplace_back( llvm::ConstantInt::get( llvm::Type::getInt32Ty( ctx ), allocas.size() ) );

                std::copy( allocas.begin(), allocas.end(), std::back_inserter( args ) );
                auto c = llvm::CallInst::Create( _cleanup, args, "", insPoint );
                ASSERT( c->getParent()->getTerminator() );
            } );

        for ( auto &bb : f )
            ASSERT( bb.getTerminator() );
    }

    OrderConfig _config;
    int _bufferSize;
    LLVMFunctionSet _bypass;
    llvm::Function *_store = nullptr, *_load = nullptr, *_fence = nullptr, *_cas = nullptr;
    llvm::Function *_memmove = nullptr, *_memcpy = nullptr, *_memset = nullptr;
    llvm::Function *_scmemmove = nullptr, *_scmemcpy = nullptr, *_scmemset = nullptr;
    llvm::Function *_cleanup = nullptr, *_resize = nullptr;
    llvm::Function *_mask = nullptr;
    llvm::Type *_moTy = nullptr;
    MemoryOrder _minMemOrd = MemoryOrder::SeqCst;
};

PassMeta meta() {
    return passMetaC< ScalarMemory, Substitute >( "weakmem",
            "Transform SC code to code with given weak memory order approximation\n"
            "\n"
            "options: [ x86 | tso | std | SPEC ]:BUFFER_SIZE\n"
            "\n"
            "   SPEC: KIND=ORDER[,...]\n"
            "         any number of specifiers of minimal memory ordering guarantee the latter can override earlier ones.\n"
            "\n"
            "   KIND: {all,load,store,cas,casfail,casok,armw,fence}\n"
            "         all - ordering for all operations\n"
            "               if any entry requires minimal ordering higher than set, it will be silently used\n"
            "         load,load,fence - ordering for load, store instructions\n"
            "         fence - ordering for fence instruction, ordering lower than acquire or release implies fence has no effect\n"
            "         armw - ordering for atomic read-modify-write (atomicrmw), must be at least relaxed\n"
            "         cas - ordering for compare-and-swap (cmpxchg) on both success and failure of comparison, must be at least relaxed\n"
            "         casfail - ordering of cmpxchg on comparison failure, must be at least relaxed\n"
            "         casok - ordering of cmpxchg on success, must be at least relaxed, must be at least as strong as casfail\n"
            "\n"
            "   ORDER: {unordered,relaxed,acquire,release,acq_rel,seq_cst}\n"
            "         memory ordering see, C++11 standard of LLVM LangRef for explanation\n"
            "         unordered - not atomicity guarantee\n"
            "\n"
            "   sc  - equivalent to all=seq_cst\n"
            "   x86 - equivalent to all=acq_rel,armw=seq_cst,cas=seq_cst\n"
            "         that is total store order + atomic compound operations are sequentially consistent\n"
            "\n"
            "   tso - equivalent to all=acq_rel\n"
            "         total store order - orders are written to memory in order of execution\n"
            "\n"
            "   std - equivalent to all=unordered\n"
            "         no ordering apart from the guarantees of C++11/LLVM standard\n",

            []( PassVector &mgr, std::string opt ) {
                ScalarMemory::meta().create( mgr, "" );
                Substitute::meta().create( mgr, opt );
            } );
}

}
}
