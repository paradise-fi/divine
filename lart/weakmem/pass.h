// -*- C++ -*- (c) 2015 Petr Rockai <me@mornfall.net>
//             (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <llvm/PassManager.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <brick-string.h>
#include <unordered_set>
#include <string>
#include <iostream>

namespace lart {
namespace weakmem {

/*
 * Replace non-scalar load and store instructions with series of scalar-based
 * ones, along with requisite aggregate value (de)composition.
 */
struct ScalarMemory : llvm::ModulePass
{
    static char ID;
    ScalarMemory() : llvm::ModulePass( ID ) {}
    bool runOnModule( llvm::Module &m ) {
        return false;
    }
};

struct Substitute : llvm::ModulePass
{
    static char ID;

    enum Type { TSO, PSO };

    Substitute( Type t ) : llvm::ModulePass( ID ), _type( t ),
        _store( nullptr ), _load( nullptr ), _flush( nullptr )
    { }

    virtual ~Substitute() {}

    std::unordered_set< std::string > parseTags( std::string str ) {
        std::unordered_set< std::string > out;
        brick::string::Splitter splitter( "[ \t]*,[ \t]*", REG_EXTENDED );
        for ( auto it = splitter.begin( str ); it != splitter.end(); ++it )
            if ( it->find( tagNamespace ) == 0 )
                out.insert( it->substr( tagNamespace.size() ) );
        return out;
    }

    void getAnnotations( llvm::Module &m ) {
        auto annos = m.getNamedGlobal( "llvm.global.annotations" );
        ASSERT( annos );
        auto a = llvm::cast< llvm::ConstantArray >( annos->getOperand(0) );
        for ( int i = 0; i < a->getNumOperands(); i++ ) {
            auto e = llvm::cast< llvm::ConstantStruct >( a->getOperand(i) );
            if ( auto fn = llvm::dyn_cast< llvm::Function >( e->getOperand(0)->getOperand(0) ) ) {
                std::string anno = llvm::cast< llvm::ConstantDataArray >(
                            llvm::cast< llvm::GlobalVariable >( e->getOperand(1)->getOperand(0) )->getOperand(0)
                        )->getAsCString();
                if ( anno.find( tagNamespace ) != std::string::npos ) {
                    auto tags = parseTags( anno );
                    if ( tags.count( "direct" ) )
                        _directAccess.insert( fn );
                    if ( tags.count( "store" ) ) {
                        ASSERT( !_store );
                        _store = fn;
                    }
                    if ( tags.count( "load" ) ) {
                        ASSERT( !_load );
                        _load = fn;
                    }
                    if ( tags.count( "flush" ) ) {
                        ASSERT( !_flush );
                        _flush = fn;
                    }
                }
            }
        }
        ASSERT( _store );
        ASSERT( _load );
        ASSERT( _flush );
    }

    void checkDirect( llvm::Function &f ) {
        for ( auto &bb : f )
            for ( auto &i : bb )
                if ( auto call = llvm::dyn_cast< llvm::CallInst >( &i ) ) {
                    auto fn = call->getCalledFunction();
                    if ( fn && !_directAccess.count( fn ) && !fn->isDeclaration() ) {
                        std::cout << "WARNING: adding function `"
                            << call->getCalledFunction()->getName().str()
                            << "' to the list of functions not using weak memory"
                            << std::endl;
                        _directAccess.insert( call->getCalledFunction() );
                    }
                }
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
        auto &ctx = f.getContext();

        std::vector< llvm::LoadInst * > loads;
        std::vector< llvm::StoreInst * > stores;
        std::vector< llvm::FenceInst * > fences;

        for ( auto &bb : f )
            for ( auto &i : bb ) {
                ASSERT( &i );
                if ( auto load = llvm::dyn_cast< llvm::LoadInst >( &i ) )
                    loads.push_back( load );
                if ( auto store = llvm::dyn_cast< llvm::StoreInst >( &i ) )
                    stores.push_back( store );
                else if ( auto fence = llvm::dyn_cast< llvm::FenceInst >( &i ) )
                    fences.push_back( fence );
            }
        for ( auto load : loads ) {
            auto op = load->getPointerOperand();
            auto opty = llvm::cast< llvm::PointerType >( op->getType() );
            auto ety = opty->getElementType();
            ASSERT( ety->isPointerTy() || ety->getPrimitiveSizeInBits() );

            llvm::IRBuilder<> builder( load );

            // fist cast address to i8*
            llvm::Value *addr = op;
            if ( !ety->isIntegerTy() || ety->getPrimitiveSizeInBits() != 8 ) {
                auto ty = llvm::PointerType::get( llvm::IntegerType::getInt8Ty( ctx ), opty->getAddressSpace() );
                addr = builder.CreateBitCast( op, ty );
            }
            auto bitwidth = getBitwidth( ety, ctx, dl );
            auto call = llvm::CallInst::Create( _load, llvm::ArrayRef< llvm::Value * >( { addr, bitwidth } ) );
            llvm::Instruction *result = call;

            // weak load and final cast i64 -> target type
            if ( ety->isPointerTy() )
                result = llvm::CastInst::Create( llvm::Instruction::IntToPtr, builder.Insert( result ), ety );
            else {
                auto size = ety->getPrimitiveSizeInBits();
                ASSERT( size > 0 );
                ASSERT_LEQ( size, 64 );
                if ( size < 64 )
                    result = llvm::CastInst::Create( llvm::Instruction::Trunc, builder.Insert( result ), intTypeOfSize( size, ctx ) );
                if ( !ety->isIntegerTy() )
                    result = llvm::CastInst::Create( llvm::Instruction::BitCast, builder.Insert( result ), ety );
            }
            llvm::ReplaceInstWithInst( load, result );
        }
        for ( auto store : stores ) {
            auto value = store->getValueOperand();
            auto addr = store->getPointerOperand();
            auto vty = value->getType();
            auto aty = llvm::cast< llvm::PointerType >( addr->getType() );

            llvm::IRBuilder<> builder( store );

            auto i64 = llvm::IntegerType::getInt64Ty( ctx );
            auto i64ptr = llvm::PointerType::get( i64, aty->getAddressSpace() );
            if ( aty != i64ptr )
                addr = builder.CreateBitCast( addr, i64ptr );

            if ( vty->isPointerTy() )
                value = builder.CreateCast( llvm::Instruction::PtrToInt, value, i64 );
            else {
                auto size = vty->getPrimitiveSizeInBits();
                ASSERT( size > 0 );
                ASSERT_LEQ( size, 64 );
                if ( !vty->isIntegerTy() )
                    value = builder.CreateCast( llvm::Instruction::BitCast, value, intTypeOfSize( size, ctx ) );
                if ( size < 64 )
                    value = builder.CreateCast( llvm::Instruction::Trunc, value, i64 );
            }

            auto bitwidth = getBitwidth( vty, ctx, dl );
            auto storeCall = llvm::CallInst::Create( _store, { addr, value, bitwidth } );
            llvm::ReplaceInstWithInst( store, storeCall );
        }
        for ( auto fence : fences ) {
            auto callFlush = llvm::CallInst::Create( _flush );
            llvm::ReplaceInstWithInst( fence, callFlush );
        }
    }

    bool runOnModule( llvm::Module &m ) {
        llvm::DataLayout dl( &m );
        getAnnotations( m );

        for ( auto &f : m )
            if ( _directAccess.count( &f ) )
                checkDirect( f );
        for ( auto &f : m )
            if ( !_directAccess.count( &f ) )
                transform( f, dl );
        return true;
    }

  private:
    Type _type;
    std::unordered_set< llvm::Function * > _directAccess;
    llvm::Function *_store, *_load, *_flush;
    static const std::string tagNamespace;
};

}
}
