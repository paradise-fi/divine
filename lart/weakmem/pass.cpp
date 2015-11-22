// -*- C++ -*- (c) 2015 Petr Rockai <me@mornfall.net>
//             (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

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
#include <brick-string.h>
#include <unordered_set>
#include <string>
#include <iostream>

#include <lart/weakmem/pass.h>
#include <lart/support/pass.h>
#include <lart/support/meta.h>
#include <lart/support/cleanup.h>

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

struct Substitute : lart::Pass
{
    enum Type { Bypass, SC, TSO, PSO };

    Substitute( Type t, int bufferSize )
        : _defaultType( t ), _bufferSize( bufferSize ),
          _storeTso( nullptr ), _storePso( nullptr ),
          _loadTso( nullptr ), _loadPso( nullptr ),
          _flush( nullptr ),
          _memmove(), _memcpy(), _memset()
    {
        ASSERT_NEQ( _defaultType, Type::Bypass );
    }

    virtual ~Substitute() {}

    static PassMeta meta() {
        return passMetaC( "Substitute",
                "Substitute loads and stores (and other memory manipulations) with appropriate "
                "weak memory model versions.",
                []( llvm::ModulePassManager &mgr, std::string opt ) {
                    Type t = TSO;

                    auto c = opt.find( ':' );
                    int bufferSize = -1;
                    if ( c != std::string::npos ) {
                        bufferSize = std::stoi( opt.substr( c + 1 ) );
                        opt = opt.substr( 0, c );
                    }

                    std::cout << "opt = " << opt << ", bufferSize = " << bufferSize << std::endl;

                    if ( opt == "tso" )
                        t = weakmem::Substitute::TSO;
                    else if ( opt == "pso" )
                        t = weakmem::Substitute::PSO;
                    else if ( opt == "sc" )
                        t = weakmem::Substitute::SC;
                    else
                        throw std::runtime_error( "unknown weakmem type: " + opt );

                    return mgr.addPass( Substitute( t, bufferSize ) );
                } );
    }

    void transformFree( llvm::Function *free ) {
        ASSERT( free );
        auto &ctx = free->getParent()->getContext();
        auto calls = query::query( free->users() )
                    .filter( query::is< llvm::CallInst > || query::is< llvm::InvokeInst > )
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

        _storeTso = m.getFunction( "__lart_weakmem_store_tso" );
        _storePso = m.getFunction( "__lart_weakmem_store_pso" );
        _loadTso = m.getFunction( "__lart_weakmem_load_tso" );
        _loadPso = m.getFunction( "__lart_weakmem_load_pso" );
        _flush = m.getFunction( "__lart_weakmem_flush" );
        _cleanup = m.getFunction( "__lart_weakmem_cleanup" );

        transformMemManip( {
                { _memmove, m.getFunction( "__lart_weakmem_memmove" ) },
                { _memcpy,  m.getFunction( "__lart_weakmem_memcpy" ) },
                { _memset,  m.getFunction( "__lart_weakmem_memset" ) } } );

        processAnnos( m );
        ASSERT( _memmove[ Type::Bypass ] );
        ASSERT( _flush );
        ASSERT( _cleanup );
        ASSERT( _tso.empty() || (_storeTso && _loadTso) );
        ASSERT( _pso.empty() || (_storePso && _loadPso) );

        transformFree( m.getFunction( "__divine_free" ) );

        for ( auto &f : m )
            transform( f, dl );
        return llvm::PreservedAnalyses::none();
    }

  private:

    void transformMemManip( std::initializer_list< std::pair< llvm::Function **, llvm::Function * > > what )
    {
        for ( auto p : what ) {
            ASSERT( p.second );
            p.first[ Type::Bypass ] = p.second;
            _bypass.insert( p.second );

            p.first[ Type::SC ] = dupFn( p.second, "_sc" );
            _sc.insert( p.first[ Type::SC ] );

            p.first[ Type::TSO ] = dupFn( p.second, "_tso" );
            _tso.insert( p.first[ Type::TSO ] );

            p.first[ Type::PSO ] = dupFn( p.second, "_pso" );
            _pso.insert( p.first[ Type::PSO ] );
        }
    }

    llvm::Function *dupFn( llvm::Function *fn, std::string nameSuff ) {
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

    Type functionType( llvm::Function *fn ) {
        if ( _sc.count( fn ) )
            return Type::SC;
        if ( _tso.count( fn ) )
            return Type::TSO;
        if ( _pso.count( fn ) )
            return Type::PSO;
        if ( _bypass.count( fn ) )
            return Type::Bypass;
        return _defaultType;
    }

    void propagate( llvm::Function *fn, LLVMFunctionSet &seen ) {
        auto type = functionType( fn );
        auto &set = setOfType( type );
        propagate( fn, type, set, seen );
    }

    void propagate( llvm::Function *fn, Type type, LLVMFunctionSet &set, LLVMFunctionSet &seen ) {
        for ( auto &bb : *fn )
            for ( auto &i : bb ) {
                llvm::Function *called = nullptr;
                if ( auto call = llvm::dyn_cast< llvm::CallInst >( &i ) )
                    called = call->getCalledFunction();
                else if ( auto invoke = llvm::dyn_cast< llvm::InvokeInst >( &i ) )
                    called = invoke->getCalledFunction();

                if ( called && !called->isDeclaration() && !seen.count( called ) ) {
                    if ( !set.count( called ) )
                        std::cout << "INFO: propagating flag " << type << " to " << called->getName().str() << std::endl;
                    set.insert( called );
                    propagate( called, type, set, seen );
                }
            }
    }

    void processAnnos( llvm::Module &m ) {
        forAnnos( m, [&]( llvm::Function *fn, std::string anno ) {
                if ( anno == "bypass" )
                    _bypass.insert( fn );
                else if ( anno == "sc" )
                    _sc.insert( fn );
                else if ( anno == "tso" )
                    _tso.insert( fn );
                else if ( anno == "pso" )
                    _pso.insert( fn );
            } );
        std::map< Type, LLVMFunctionSet > seen;
        forAnnos( m, [&]( llvm::Function *fn, std::string anno ) {
                if ( anno == "propagate" )
                    propagate( fn, seen[ functionType( fn ) ] );
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
        ASSERT_UNREACHABLE( "unhandled case" );
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
        transformMemTrans( fn, Type::Bypass );
    }

    void transformMemTrans( llvm::Function &fn, Type type ) {
        transformMemTrans< llvm::MemSetInst >( fn, _memset[ type ] );
        transformMemTrans< llvm::MemCpyInst >( fn, _memcpy[ type ] );
        transformMemTrans< llvm::MemMoveInst >( fn, _memmove[ type ] );
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

        auto repTy = rfn->getFunctionType();
        auto dstTy = repTy->getParamType( 0 );
        auto srcTy = repTy->getParamType( 1 );
        auto lenTy = repTy->getParamType( 2 );

        std::vector< IntrinsicType * > mems;
        for ( auto &bb : fn )
            for ( auto &i : bb )
                if ( auto mem = llvm::dyn_cast< IntrinsicType >( &i ) )
                    mems.push_back( mem );
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
            auto replace = builder.CreateCall( rfn, llvm::ArrayRef< llvm::Value * >( { dst, src, len } ) );
            mem->replaceAllUsesWith( replace );
            mem->eraseFromParent();
        }
    }

    // SC functions are transformed by inserting flush at the beginning,
    // and flush after every function which we don't surely (statically)
    // know is SC
    void transformSC( llvm::Function &fn ) {
        // intial barrier
        auto &bb = fn.getEntryBlock();
        llvm::IRBuilder<> builder( bb.begin() );
        builder.CreateCall( _flush );

        flushCalls( fn, _flush, [&]( llvm::Function *called ) { return functionType( called ) == Type::SC || functionType( called ) == Type::Bypass; } );
    }

    template< typename Filter >
    void flushCalls( llvm::Function &fn, llvm::Function *barrier, Filter filter ) {
        std::vector< llvm::CallSite > calls;
        for ( auto &bb : fn )
            for ( auto &i : bb ) {
                if ( auto call = llvm::dyn_cast< llvm::CallInst >( &i ) )
                    calls.emplace_back( call );
                else if ( auto inv = llvm::dyn_cast< llvm::InvokeInst >( &i ) )
                    calls.emplace_back( inv );
            }
        for ( auto call : calls ) {
            auto called = call.getCalledFunction();
            if ( (called == nullptr || !filter( called ))
                    && !llvm::isa< llvm::IntrinsicInst >( call.getInstruction() ) )
                llvm::CallInst::Create( barrier )->insertAfter( call.getInstruction() );
        }
    }

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

        if ( type == Type::TSO ) {
            // TODO: PSO->TSO barrier
        }

        std::vector< llvm::LoadInst * > loads;
        std::vector< llvm::StoreInst * > stores;
        std::vector< llvm::FenceInst * > fences;
        std::vector< llvm::AtomicCmpXchgInst * > cass;
        std::vector< llvm::AtomicRMWInst * > ats;

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
                },
                [&]( llvm::AtomicCmpXchgInst *cas ) {
                    if ( !withLocal( cas ) )
                        cass.push_back( cas );
                },
                [&]( llvm::AtomicRMWInst *at ) {
                    if ( !withLocal( at ) )
                        ats.push_back( at );
                } );

        for ( auto load : loads ) {
            // SC: no barrier needed it has no effect on load
            // TODO: PSO ordering (release,...)

            auto op = load->getPointerOperand();
            auto opty = llvm::cast< llvm::PointerType >( op->getType() );
            auto ety = opty->getElementType();
            //ASSERT( ety->isPointerTy() || ety->getPrimitiveSizeInBits() );
            if ( !( ety->isPointerTy() || ety->getPrimitiveSizeInBits() ) )
                continue; // TODO

            llvm::IRBuilder<> builder( load );

            // fist cast address to i8*
            llvm::Value *addr = op;
            if ( !ety->isIntegerTy() || ety->getPrimitiveSizeInBits() != 8 ) {
                auto ty = llvm::PointerType::get( llvm::IntegerType::getInt8Ty( ctx ), opty->getAddressSpace() );
                addr = builder.CreateBitCast( op, ty );
            }
            auto bitwidth = getBitwidth( ety, ctx, dl );
            auto call = builder.CreateCall( type == Type::TSO ? _loadTso : _loadPso, llvm::ArrayRef< llvm::Value * >( { addr, bitwidth } ) );
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
            if ( store->getOrdering() == llvm::AtomicOrdering::SequentiallyConsistent ) {
                // note: theoretically there should be weak store followed by
                // memory barrier, however, this is equeivalent to first flusting
                // store buffers AND THEN preforming direct store
                llvm::CallInst::Create( _flush )->insertBefore( store );
                continue;
            }
            // TODO: acquere ordering for PSO

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
            auto storeCall = builder.CreateCall( type == Type::TSO ? _storeTso : _storePso, llvm::ArrayRef< llvm::Value * >( { addr, value, bitwidth } ) );
            store->replaceAllUsesWith( storeCall );
            store->eraseFromParent();
        }
        for ( auto fence : fences ) {
            if ( fence->getOrdering() != llvm::AtomicOrdering::SequentiallyConsistent )
                continue;

            // TODO: PSO fences
            auto callFlush = llvm::CallInst::Create( _flush );
            llvm::ReplaceInstWithInst( fence, callFlush );
        }
        for ( auto cas : cass ) {
            // ASSERT( cas->getOrdering() == llvm::AtomicOrdering::SequentiallyConsistent );
            llvm::CallInst::Create( _flush )->insertBefore( cas );
        }
        for ( auto at : ats ) {
            // ASSERT( at->getOrdering() == llvm::AtomicOrdering::SequentiallyConsistent );
            llvm::CallInst::Create( _flush )->insertBefore( at );
        }
        /* TODO: flush PSO -> TSO
        if ( type == Type::TSO )
            flushCalls( fn, _flush, [&]( llvm::Function *called ) {
                    return _bypass.count( called ) || _sc.count( called ) || _tso.count( called );
                } );
        */

        transformMemTrans( f, type );

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
    LLVMFunctionSet _bypass, _sc, _tso, _pso;
    llvm::Function *_storeTso, *_storePso, *_loadTso, *_loadPso, *_flush;
    llvm::Function *_memmove[4], *_memcpy[4], *_memset[4];
    llvm::Function *_cleanup;
    static const std::string tagNamespace;
};

PassMeta meta() {
    return passMetaC< ScalarMemory, Substitute >( "weakmem",
            "Transform SC code to code with given weak memory order approximation\n\n"
            "options: { tso | pso | sc }:BUFFER_SIZE",
            []( llvm::ModulePassManager &mgr, std::string opt ) {
                ScalarMemory::meta().create( mgr, "" );
                Substitute::meta().create( mgr, opt );
            } );
}

const std::string Substitute::tagNamespace = "lart.weakmem.";

}
}
