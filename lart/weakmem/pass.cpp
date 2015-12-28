// -*- C++ -*- (c) 2015 Petr Rockai <me@mornfall.net>
//             (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

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
#include <brick-string>
#include <unordered_set>
#include <string>
#include <iostream>

#include <lart/weakmem/pass.h>
#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/cleanup.h>
#include <lart/userspace/weakmem.h>

namespace lart {
namespace weakmem {

using LLVMFunctionSet = std::unordered_set< llvm::Function * >;

/*
 * Replace non-scalar load and store instructions with series of scalar-based
 * ones, along with requisite aggregate value (de)composition.
 */
struct ScalarMemory : lart::Pass
{

    static PassMeta meta() {
        return passMeta< ScalarMemory >( "ScalarMemory", "breaks down loads and stores larger than 64 bits" );
    }

    static char ID;
    const unsigned _wordsize = 8;

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) {
        for ( auto &f : m )
            transform( f );
        return llvm::PreservedAnalyses::none();
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


MemoryOrder operator|( MemoryOrder a, MemoryOrder b ) {
    using uint = typename std::underlying_type< MemoryOrder >::type;
    return MemoryOrder( uint( a ) | uint( b ) );
}

struct Substitute : lart::Pass {

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
        switch ( mo ) {
            case MemoryOrder::Unordered: return llvm::AtomicOrdering::Unordered;
            case MemoryOrder::Monotonic: return llvm::AtomicOrdering::Monotonic;
            case MemoryOrder::Acquire: return llvm::AtomicOrdering::Acquire;
            case MemoryOrder::Release: return llvm::AtomicOrdering::Release;
            case MemoryOrder::AcqRel: return llvm::AtomicOrdering::AcquireRelease;
            case MemoryOrder::SeqCst: return llvm::AtomicOrdering::SequentiallyConsistent;
        }
    }

    static MemoryOrder castmemord( llvm::AtomicOrdering mo ) {
        switch ( mo ) {
            case llvm::AtomicOrdering::NotAtomic: // its the same as Unordered for us
            case llvm::AtomicOrdering::Unordered: return MemoryOrder::Unordered;
            case llvm::AtomicOrdering::Monotonic: return MemoryOrder::Monotonic;
            case llvm::AtomicOrdering::Acquire: return MemoryOrder::Acquire;
            case llvm::AtomicOrdering::Release: return MemoryOrder::Release;
            case llvm::AtomicOrdering::AcquireRelease: return MemoryOrder::AcqRel;
            case llvm::AtomicOrdering::SequentiallyConsistent: return MemoryOrder::SeqCst;
        }
    }

    virtual ~Substitute() {}

    static PassMeta meta() {
        return passMetaC( "Substitute",
                "Substitute loads and stores (and other memory manipulations) with appropriate "
                "weak memory model versions.",
                []( llvm::ModulePassManager &mgr, std::string opt ) {
                    OrderConfig config;

                    auto c = opt.find( ':' );
                    int bufferSize = -1;
                    if ( c != std::string::npos ) {
                        bufferSize = std::stoi( opt.substr( c + 1 ) );
                        opt = opt.substr( 0, c );
                    }

                    std::cout << "opt = " << opt << ", bufferSize = " << bufferSize << std::endl;

                    if ( opt == "sc" ) {
                        config.setAll( MemoryOrder::SeqCst );
                    } else if ( opt == "x86" ) {
                        config.setAll( MemoryOrder::AcqRel );
                        config.setCas( MemoryOrder::SeqCst );
                        config.rmw = MemoryOrder::SeqCst;
                    } else if ( opt == "tso" ) {
                        config.setAll( MemoryOrder::AcqRel );
                    } else if ( opt == "std" ) {
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

                    return mgr.addPass( Substitute( config, bufferSize ) );
                } );
    }

    void transformFree( llvm::Function *free ) {
        ASSERT( free );
        auto &ctx = free->getParent()->getContext();
        auto calls = query::query( free->users() )
                    .filter( query::is< llvm::CallInst > || query::is< llvm::InvokeInst > )
                    .filter( [&]( llvm::User *i ) {
                        return _bypass.count( llvm::cast< llvm::Instruction >( i )->getParent()->getParent() ) == 0;
                    } )
                    .map( []( llvm::User *i ) { return llvm::CallSite( i ); } )
                    .freeze();

        for ( auto &cs : calls ) {
            std::vector< llvm::Value * > args;
            args.emplace_back( llvm::ConstantInt::get( llvm::Type::getInt32Ty( ctx ), 1 ) );
            args.emplace_back( cs.getArgOperand( 0 ) );
            llvm::CallInst::Create( _cleanup, args, "", cs.getInstruction() );
        }
    }

    using lart::Pass::run;
    llvm::PreservedAnalyses run( llvm::Module &m ) {
        if ( _bufferSize > 0 ) {
            auto i32 = llvm::IntegerType::getInt32Ty( m.getContext() );
            auto bufSize = llvm::ConstantInt::getSigned( i32, _bufferSize );
            auto glo = llvm::cast< llvm::GlobalVariable >( m.getOrInsertGlobal( "__lart_weakmem_buffer_size", i32 ) );
            glo->setInitializer( bufSize );
        }

        llvm::DataLayout dl( &m );

        _mask = m.getFunction( "__divine_interrupt_mask" );
        _unmask = m.getFunction( "__divine_interrupt_unmask" );
        ASSERT( _mask ); ASSERT( _unmask );

        _store = m.getFunction( "__lart_weakmem_store" );
        _load = m.getFunction( "__lart_weakmem_load" );
        _flush = m.getFunction( "__lart_weakmem_fence" );
        _cleanup = m.getFunction( "__lart_weakmem_cleanup" );
        ASSERT( _store ); ASSERT( _load ); ASSERT( _flush ); ASSERT( _cleanup );

        _moTy = _flush->getFunctionType()->getParamType( 0 );

        transformMemManip( {
                std::make_tuple( &_memmove, &_wmemmove, m.getFunction( "__lart_weakmem_memmove" ) ),
                std::make_tuple( &_memcpy, &_wmemcpy,  m.getFunction( "__lart_weakmem_memcpy" ) ),
                std::make_tuple( &_memset, &_wmemset, m.getFunction( "__lart_weakmem_memset" ) ) } );

        processAnnos( m );

        transformFree( m.getFunction( "__divine_free" ) );

        for ( auto &f : m )
            if ( _bypass.count( &f ) )
                transformBypass( f );
            else
                transformWeak( f, dl );
        return llvm::PreservedAnalyses::none();
    }

  private:

    void transformMemManip( std::initializer_list< std::tuple< llvm::Function **, llvm::Function **, llvm::Function * > > what )
    {
        for ( auto p : what ) {
            ASSERT( std::get< 2 >( p ) );
            _bypass.insert( *std::get< 0 >( p ) = std::get< 2 >( p ) );

            *std::get< 1 >( p ) = memFn( std::get< 2 >( p ), "_weak" );
        }
    }

    llvm::Function *memFn( llvm::Function *fn, std::string nameSuff ) {
        llvm::ValueToValueMapTy vmap;
        auto dup = llvm::CloneFunction( fn, vmap, false );
        dup->setName( fn->getName().str() + nameSuff );
        fn->getParent()->getFunctionList().push_back( dup );
        return dup;
    }

    std::string parseTag( std::string str ) {
        return str.substr( tagNamespace.size() );
    }

    template< typename Yield >
    void forAnnos( llvm::Module &m, Yield yield ) {
        auto annos = m.getNamedGlobal( "llvm.global.annotations" );
        ASSERT( annos );
        auto a = llvm::cast< llvm::ConstantArray >( annos->getOperand(0) );
        for ( int i = 0; i < int( a->getNumOperands() ); i++ ) {
            auto e = llvm::cast< llvm::ConstantStruct >( a->getOperand(i) );
            if ( auto fn = llvm::dyn_cast< llvm::Function >( e->getOperand(0)->getOperand(0) ) ) {
                std::string anno = llvm::cast< llvm::ConstantDataArray >(
                            llvm::cast< llvm::GlobalVariable >( e->getOperand(1)->getOperand(0) )->getOperand(0)
                        )->getAsCString();
                if ( anno.find( tagNamespace ) != std::string::npos )
                    yield( fn, parseTag( anno ) );
            }
        }
    }

    void propagateBypass( llvm::Function *fn, LLVMFunctionSet &seen ) {
        for ( auto &bb : *fn )
            for ( auto &i : bb ) {
                llvm::Function *called = nullptr;
                if ( auto call = llvm::dyn_cast< llvm::CallInst >( &i ) )
                    called = llvm::dyn_cast< llvm::Function >( call->getCalledValue()->stripPointerCasts() );
                else if ( auto invoke = llvm::dyn_cast< llvm::InvokeInst >( &i ) )
                    called = llvm::dyn_cast< llvm::Function >( invoke->getCalledValue()->stripPointerCasts() );

                if ( called && !called->isDeclaration() && !seen.count( called ) ) {
                    if ( _bypass.insert( called ).second )
                        std::cout << "INFO: propagating bypass flag to " << called->getName().str() << std::endl;
                    propagateBypass( called, seen );
                }
            }
    }

    void processAnnos( llvm::Module &m ) {
        forAnnos( m, [&]( llvm::Function *fn, std::string anno ) {
                if ( anno == "bypass" )
                    _bypass.insert( fn );
            } );
        LLVMFunctionSet seen;
        forAnnos( m, [&]( llvm::Function *fn, std::string anno ) {
                if ( anno == "propagate" )
                    propagateBypass( fn, seen );
            } );
    }

    llvm::Type *intTypeOfSize( int size, llvm::LLVMContext &ctx ) {
        switch ( size ) {
            case 64: return llvm::IntegerType::getInt64Ty( ctx );
            case 32: return llvm::IntegerType::getInt32Ty( ctx );
            case 16: return llvm::IntegerType::getInt16Ty( ctx );
            case 8: return llvm::IntegerType::getInt8Ty( ctx );
            case 1: return llvm::IntegerType::getInt1Ty( ctx );
        }
        UNREACHABLE( "unhandled case" );
    }

    llvm::Value *getBitwidth( llvm::Type *type, llvm::LLVMContext &ctx, llvm::DataLayout &dl ) {
        return llvm::ConstantInt::get( llvm::IntegerType::getInt32Ty( ctx ),
                                        type->isPointerTy()
                                            ? dl.getPointerSize() * 8
                                            : type->getPrimitiveSizeInBits() );
    }

    void transform( llvm::Function &f, llvm::DataLayout &dl ) {
        if ( f.empty() )
            return;
        switch ( functionType( &f ) ) {
            case Type::Bypass:
                transformBypass( f );
                return;
            case Type::SC:
                transformSC( f );
                return;
            case Type::TSO:
            case Type::PSO:
                transformWeak( f, dl );
                return;
        }
        ASSERT_UNREACHABLE( "Unhandled case" );
    }

    // in bypass functions we replace all memory transfer intrinsics with lart
    // memmove
    void transformBypass( llvm::Function &fn ) {
        transformMemTrans< llvm::MemSetInst >( fn, _memset );
        transformMemTrans< llvm::MemCpyInst >( fn, _memcpy );
        transformMemTrans< llvm::MemMoveInst >( fn, _memmove );
    }

    void transformMemTrans( llvm::Function &fn ) {
        transformMemTrans< llvm::MemSetInst >( fn, _wmemset );
        transformMemTrans< llvm::MemCpyInst >( fn, _wmemcpy );
        transformMemTrans< llvm::MemMoveInst >( fn, _wmemmove );
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
            mem->replaceAllUsesWith( replace );
            mem->eraseFromParent();
        }
    }

    template< typename Inst >
    bool withLocal( Inst *i ) { return withLocal( i, brick::types::Preferred() ); }

    template< typename Inst >
    auto withLocal( Inst *i, brick::types::Preferred ) -> decltype( bool( i->getPointerOperand() ) )
    {
        return local( i->getPointerOperand() );
    }

    template< typename Inst >
    bool withLocal( Inst *i, brick::types::NotPreferred ) { return false; }

    bool local( llvm::Value *i ) {
        return llvm::isa< llvm::AllocaInst >( i ) && !llvm::PointerMayBeCaptured( i, false, true );
    };

    // for weak memory functions we have to:
    // *  transform all loads and stores to appropriate lart function
    // *  transform all memcpy/memmove (both calls and intrinsics) to call to lart memcpy/memmove
    // *  transform fence instructions to call to lart fence
    // *  for TSO: insert PSO -> TSO fence at the beginning and after call to
    //    any function which is neither TSO, SC, or bypass
    void transformWeak( llvm::Function &f, llvm::DataLayout &dl ) {
        transformWeak( f, dl, functionType( &f ) );
    }

    template< typename Inst >
    bool withLocal( Inst *i ) { return withLocal( i, brick::types::Preferred() ); }

    template< typename Inst >
    auto withLocal( Inst *i, brick::types::Preferred ) -> decltype( bool( i->getPointerOperand() ) )
    {
        return local( i->getPointerOperand() );
    }

    template< typename Inst >
    bool withLocal( Inst *i, brick::types::NotPreferred ) { return false; }

    bool local( llvm::Value *i ) {
        return llvm::isa< llvm::AllocaInst >( i ) && !llvm::PointerMayBeCaptured( i, false, true );
    };

    void transformWeak( llvm::Function &f, llvm::DataLayout &dl, Type type ) {
        auto &ctx = f.getContext();

        // fist translate atomics, so that if they are translated to
        // non-atomic equievalents under mask these are later converted to TSO
        std::vector< llvm::AtomicCmpXchgInst * > cass;
        std::vector< llvm::AtomicRMWInst * > ats;

        for ( auto &i : query::query( f ).flatten() )
            llvmcase( i,
                [&]( llvm::AtomicCmpXchgInst *cas ) {
                    if ( !withLocal( cas ) )
                        cass.push_back( cas );
                },
                [&]( llvm::AtomicRMWInst *at ) {
                    if ( !withLocal( at ) )
                        ats.push_back( at );
                } );

        for ( auto cas : cass ) {
            // we don't need to consider failure ordering for TSO, this is the same case
            // as ordering for load instruction
            auto succord = castmemord( castmemord( cas->getSuccessOrdering() ) | _config.casOK );
            auto failord = castmemord( castmemord( cas->getFailureOrdering() ) | _config.casFail );
            /* transform:
             *    something
             *    %valsucc = cmpxchg %ptr, %cmp, %val succord failord
             *    something_else
             *
             * into:
             *    something
             *    %v = call __divine_interrupt_mask()
             *    %shouldunlock = icmp eq %v, 0
             *    %orig = load %ptr failord
             *    %eq = icmp eq %orig, %cmp
             *    br %eq, %ifeq, %end
             *
             *   ifeq:
             *    fence succord
             *    store %ptr, %val succord
             *    br %end
             *
             *   end:
             *    %r0 = insertvalue undef, %orig, 0
             *    %valsucc = insertvalue %0, %ew, 1
             *    br %shouldunlock, %unmask, %coninue
             *
             *   unmask:
             *    call __divine_interrupt_unmask()
             *    br %continue
             *
             *   continue:
             *    something_else
            */
            auto *rt = cas->getType();
            auto *ptr = cas->getPointerOperand();
            auto *cmp = cas->getCompareOperand();
            auto *val = cas->getNewValOperand();
            auto *bb = cas->getParent();

            auto *end = bb->splitBasicBlock( cas, "lart.weakmem.cmpxchg.end" ); // adds br instead of cas, cas is at beginning of new bb
            auto *ifeq = llvm::BasicBlock::Create( ctx, "lart.weakmem.cmpxchg.ifeq", &f, end );
            auto *cont = end->splitBasicBlock( std::next( llvm::BasicBlock::iterator( cas ) ), "lart.weakmem.cmpxchg.continue" );
            auto *unmask = llvm::BasicBlock::Create( ctx, "lart.weakmem.cmpxchg.unmask", &f, cont );

            llvm::IRBuilder<> irb( bb->getTerminator() );
            auto *mask = irb.CreateCall( _mask, { } );
            auto *shouldunlock = irb.CreateICmpEQ( mask, llvm::ConstantInt::get( mask->getType(), 0 ), "lart.weakmem.cmpxchg.shouldunlock" );
            auto *orig = irb.CreateLoad( ptr, "lart.weakmem.cmpxchg.orig" );
            orig->setAtomic( failord );
            auto *eq = irb.CreateICmpEQ( orig, cmp, "lart.weakmem.cmpxchg.eq" );
            llvm::ReplaceInstWithInst( bb->getTerminator(), llvm::BranchInst::Create( ifeq, end, eq ) );

            irb.SetInsertPoint( ifeq );
            irb.CreateFence( succord );
            irb.CreateStore( val, ptr )->setAtomic( succord );
            irb.CreateBr( end );

            irb.SetInsertPoint( end->getFirstInsertionPt() );
            auto *r0 = irb.CreateInsertValue( llvm::UndefValue::get( rt ), orig, { 0 } );
            auto *r = irb.CreateInsertValue( r0, eq, { 1 } );
            cas->replaceAllUsesWith( r );
            cas->eraseFromParent();
            llvm::ReplaceInstWithInst( end->getTerminator(),
                    llvm::BranchInst::Create( unmask, cont, shouldunlock ) );

            irb.SetInsertPoint( unmask );
            irb.CreateCall( _unmask, { } );
            irb.CreateBr( cont );
        }
        for ( auto *at : ats ) {
            auto aord = castmemord( castmemord( at->getOrdering() ) | _config.rmw );
            auto *ptr = at->getPointerOperand();
            auto *val = at->getValOperand();
            auto op = at->getOperation();

            auto *bb = at->getParent();
            auto *cont = bb->splitBasicBlock( std::next( llvm::BasicBlock::iterator( at ) ), "lart.weakmem.atomicrmw.continue" );
            auto *unmask = llvm::BasicBlock::Create( ctx, "lart.weakmem.atomicrmw.unmask", &f, cont );

            llvm::IRBuilder<> irb( at );
            auto *mask = irb.CreateCall( _mask, { } );
            auto *shouldunlock = irb.CreateICmpEQ( mask, llvm::ConstantInt::get( mask->getType(), 0 ), "lart.weakmem.atomicrmw.shouldunlock" );
            auto *orig = irb.CreateLoad( ptr, "lart.weakmem.atomicrmw.orig" );
            orig->setAtomic( aord == llvm::AtomicOrdering::AcquireRelease ? llvm::AtomicOrdering::Acquire : aord );

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
                    ASSERT_UNREACHABLE( "weakmem: bad binop in AtomicRMW" );
            }

            irb.CreateStore( val, ptr )->setAtomic(
                    aord == llvm::AtomicOrdering::AcquireRelease ? llvm::AtomicOrdering::Release : aord );
            llvm::ReplaceInstWithInst( bb->getTerminator(),
                    llvm::BranchInst::Create( unmask, cont, shouldunlock ) );

            irb.SetInsertPoint( unmask );
            irb.CreateCall( _unmask, { } );
            irb.CreateBr( cont );

            at->replaceAllUsesWith( orig );
            at->eraseFromParent();
        }

        // now translate load/store/fence
        std::vector< llvm::LoadInst * > loads;
        std::vector< llvm::StoreInst * > stores;
        std::vector< llvm::FenceInst * > fences;

        for ( auto &i : query::query( f ).flatten() )
            llvmcase( i,
                [&]( llvm::LoadInst *load ) {
                    if ( !withLocal( load ) )
                        loads.push_back( load );
                },
                [&]( llvm::StoreInst *store ) {
                    if ( !withLocal( store ) )
                        stores.push_back( store );
                },
                [&]( llvm::FenceInst *fence ) {
                    fences.push_back( fence );
                } );

        for ( auto load : loads ) {

            auto op = load->getPointerOperand();
            auto opty = llvm::cast< llvm::PointerType >( op->getType() );
            auto ety = opty->getElementType();
            ASSERT( ety->isPointerTy() || ety->getPrimitiveSizeInBits() || (ety->dump(), false) );

            llvm::IRBuilder<> builder( load );

            // fist cast address to i8*
            llvm::Value *addr = op;
            if ( !ety->isIntegerTy() || ety->getPrimitiveSizeInBits() != 8 ) {
                auto ty = llvm::PointerType::get( llvm::IntegerType::getInt8Ty( ctx ), opty->getAddressSpace() );
                addr = builder.CreateBitCast( op, ty );
            }
            auto bitwidth = getBitwidth( ety, ctx, dl );
            auto call = builder.CreateCall( _load, { addr, bitwidth,
                        llvm::ConstantInt::get( _moTy, uint64_t( _config.load | castmemord( load->getOrdering() ) ) )
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
                        llvm::ConstantInt::get( _moTy, uint64_t( _config.store | castmemord( store->getOrdering() ) ) )
                    } );
            store->replaceAllUsesWith( storeCall );
            store->eraseFromParent();
        }
        for ( auto fence : fences ) {
            auto callFlush = llvm::CallInst::Create( _flush, {
                    llvm::ConstantInt::get( _moTy, uint64_t( _config.fence | castmemord( fence->getOrdering() ) ) ) } );
            llvm::ReplaceInstWithInst( fence, callFlush );
        }

        transformMemTrans( f );

        // add cleanups
        cleanup::addAllocaCleanups( cleanup::EhInfo::cpp( *f.getParent() ), f,
            [&]( llvm::AllocaInst *alloca ) {
                return !query::query( alloca->users() ).all( [this]( llvm::Value *v ) { return this->withLocal( v ); } );
            },
            [&]( llvm::Instruction *insPoint, auto &allocas ) {
                if ( allocas.empty() )
                    return;

                std::vector< llvm::Value * > args;
                args.emplace_back( llvm::ConstantInt::get( llvm::Type::getInt32Ty( ctx ), allocas.size() ) );
                std::copy( allocas.begin(), allocas.end(), std::back_inserter( args ) );
                llvm::CallInst::Create( _cleanup, args, "", insPoint );
            } );
    }

    LLVMFunctionSet &_default() {
        return setOfType( _defaultType );
    }

    LLVMFunctionSet &setOfType( Type type ) {
        switch ( type ) {
            case Type::SC: return _sc;
            case Type::TSO: return _tso;
            case Type::PSO: return _pso;
            case Type::Bypass: return _bypass;
            default: ASSERT_UNREACHABLE( "unhandled case" );
        }
    }

    LLVMFunctionSet &fnSet( llvm::Function *fn ) {
        return setOfType( functionType( fn ) );
    }

    Type _defaultType;
    int _bufferSize;
    LLVMFunctionSet _bypass;
    llvm::Function *_store = nullptr, *_load = nullptr, *_flush = nullptr;
    llvm::Function *_memmove = nullptr, *_memcpy = nullptr, *_memset = nullptr;
    llvm::Function *_wmemmove = nullptr, *_wmemcpy = nullptr, *_wmemset = nullptr;
    llvm::Function *_cleanup = nullptr;
    llvm::Function *_mask = nullptr, *_unmask = nullptr;
    llvm::Type *_moTy;
    static const std::string tagNamespace;
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
            "   sc  - equeivalent to all=seq_cst\n"
            "   x86 - equivalent to all=acq_rel,armw=seq_cst,cas=seq_cst\n"
            "         that is total store order + atomic compound operations are sequentially consistent\n"
            "\n"
            "   tso - equivalent to all=acq_rel\n"
            "         total store order - orders are written to memory in order of execution\n"
            "\n"
            "   std - equivalent to all=unordered\n"
            "         no ordering apart from the guarantees of C++11/LLVM standard\n",

            []( llvm::ModulePassManager &mgr, std::string opt ) {
                ScalarMemory::meta().create( mgr, "" );
                Substitute::meta().create( mgr, opt );
            } );
}

const std::string Substitute::tagNamespace = "lart.weakmem.";

}
}
