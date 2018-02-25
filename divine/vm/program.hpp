// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2012-2016 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <brick-types>
#include <brick-data>
#include <brick-assert>
#include <brick-mem>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Function.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/CallSite.h>
#include <llvm/CodeGen/IntrinsicLowering.h>
#include <llvm/IR/IntrinsicInst.h>
DIVINE_UNRELAX_WARNINGS

#include <divine/vm/pointer.hpp>
#include <divine/vm/heap.hpp>
#include <divine/vm/context.hpp>
#include <divine/cc/clang.hpp>

#include <divine/vm/lx-type.hpp>
#include <divine/vm/lx-code.hpp>
#include <divine/vm/lx-slot.hpp>
#include <divine/vm/xg-type.hpp>
#include <divine/vm/xg-code.hpp>

#include <divine/vm/divm.h>

#include <map>
#include <unordered_map>

#undef alloca

namespace divine::vm
{

/*
 * A representation of the LLVM program that is suitable for execution.
 */
struct Program
{
    llvm::Module *module;
    llvm::DataLayout TD;
    using Slot = lx::Slot;

    struct Instruction
    {
        uint32_t opcode:16;
        uint32_t subcode:16;
        Slot result() const { ASSERT( values.size() ); return values[0]; }
        Slot operand( int i ) const
        {
            int idx = (i >= 0) ? (i + 1) : (i + values.size());
            ASSERT_LT( idx, values.size() );
            return values[ idx ];
        }
        Slot value( int i ) const
        {
            int idx = (i >= 0) ? i : (i + values.size());
            ASSERT_LT( idx, values.size() );
            return values[ idx ];
        }

        int argcount() const { return values.size() - 1; }
        bool has_result() const { return values.size() > 0; }

        Instruction() : opcode( 0 ), subcode( 0 ) {}
        Instruction( const Instruction & ) = delete;
        Instruction( Instruction && ) noexcept = default;

        friend std::ostream &operator<<( std::ostream &o, const Program::Instruction &i )
        {
            for ( auto v : i.values )
                o << v << " ";
            return o;
        }

    private:
        brick::data::SmallVector< Slot, 4 > values;
        friend struct Program;
    };

    struct Function
    {
        int framesize;
        int argcount:31;
        bool vararg:1;
        Slot personality;
        std::vector< Instruction > instructions;

        Instruction &instruction( CodePointer pc )
        {
            ASSERT_LT( pc.instruction(), instructions.size() );
            return instructions[ pc.instruction() ];
        }

        Function() : framesize( PointerBytes * 2 ) {}
        Function( const Function & ) = delete;
        Function( Function && ) noexcept = default;
    };

    std::vector< Function > functions;
    std::vector< Slot > _globals;
    int _globals_size, _constants_size;

    int framealign;
    bool codepointers;

    std::map< const llvm::Value *, Slot > valuemap;
    std::map< const llvm::Value *, Slot > globalmap;
    std::map< const llvm::Type *, int > typemap;
    std::map< const llvm::Value *, std::string > anonmap;
    std::set< const llvm::Function * > is_debug;

    using Context = ConstContext< Program, SmallHeap >;
    Context _ccontext;

    using LXTypes = lx::Types< typename Context::Heap >;

    std::unique_ptr< LXTypes > _types;
    xg::Types _types_gen;
    xg::AddressMap _addr;

    auto &heap() { return _ccontext.heap(); }

    template< typename H >
    std::pair< HeapPointer, HeapPointer > exportHeap( H &target );

    template< typename Container >
    static void makeFit( Container &c, int index )
    {
        c.resize( std::max( index + 1, int( c.size() ) ) );
    }

    Instruction &instruction( CodePointer pc )
    {
        return function( pc ).instruction( pc );
    }

    Function &function( CodePointer pc )
    {
        ASSERT_LT( pc.function(), functions.size() );
        ASSERT( pc.function() > 0 );
        return functions[ pc.function() ];
    }

    Function &function( llvm::Function *F )
    {
        return functions[ _addr.code( F ).function() ];
    }

    GenericPointer addr( llvm::Value *v )
    {
        return _addr.addr( v );
    }

    CodePointer nextpc( CodePointer pc )
    {
        if ( !valid( pc ) )
            return pc;
        auto op = instruction( pc ).opcode;
        if ( valid( pc + 1 ) && ( op == lx::OpBB || op == lx::OpDbg ) )
            return nextpc( pc + 1 );
        return pc;
    }

    CodePointer entry( CodePointer pc )
    {
        auto &f = function( pc );
        auto rv = CodePointer( pc.function(), brick::bitlevel::align( f.argcount + f.vararg, 4 ) );
        ASSERT_EQ( instruction( rv ).opcode, lx::OpBB );
        return rv;
    }

    CodePointer advance( CodePointer pc ) { return nextpc( pc + 1 ); }
    bool valid( CodePointer pc )
    {
        if ( pc.function() >= functions.size() )
            return false;
        return pc.instruction() < function( pc ).instructions.size();
    }

    Slot allocateSlot( Slot slot, int function = 0, llvm::Value *val = nullptr );
    HeapPointer s2hptr( Slot s, int offset = 0 );

    using Coverage = std::vector< std::vector< llvm::Value * > >;
    std::vector< Coverage > coverage;

    bool lifetimeOverlap( llvm::Value *a, llvm::Value *b );
    void overlaySlot( int fun, Slot &result, llvm::Value *val );

    std::deque< std::function< void() > > _toinit;
    std::set< llvm::Value * > _doneinit;

    CodePointer functionByName( std::string s );
    GenericPointer globalByName( std::string s );

    bool isCodePointer( llvm::Value *val );
    bool isCodePointerConst( llvm::Value *val );
    CodePointer getCodePointer( llvm::Value *val );

    void initConstant( Slot slot, llvm::Value * );

    template< typename IorCE >
    int initSubcode( IorCE *I )
    {
        return xg::subcode( _types_gen, _addr, I );
    }

    template< typename T >
    auto initConstant( Slot s, T t ) -> decltype( std::declval< typename T::Raw >(), void() )
    {
        // std::cerr << "initial constant value " << t << " at " << s2hptr( s ) << std::endl;
        _ccontext.heap().write( s2hptr( s ), t );
    }


    struct Position
    {
        CodePointer pc;
        llvm::BasicBlock::iterator I;
        Position( CodePointer pc, llvm::BasicBlock::iterator I ) : pc( pc ), I( I ) {}
    };

    template< typename Insn > void insertIndices( Position p );
    Position insert( Position );
    Position lower( Position ); // convert intrinsic into normal insns
    void hypercall( Position );
    Slot initSlot( llvm::Value *val, Slot::Location loc );
    Slot insert( int function, llvm::Value *val, bool noinit = false );
    int insert( llvm::Type *t );

    void pass(); /* internal */

    void setupRR();
    void computeRR(); /* RR = runtime representation */
    void computeStatic();

    /* the construction sequence is this:
       1) constructor
       2) setupRR
       3a) (optional) set up additional constant/global data
       3b) computeRR
       4) computeStatic */
    Program( llvm::Module *m ) : module( m ), TD( m ), _ccontext( *this ), _types_gen( m )
    {
        _constants_size = 0;
        _globals_size = 0;
    }
};

}

#ifdef BRICK_UNITTEST_REG
namespace divine::t_vm
{

namespace {

auto testContext() {
    static std::shared_ptr< llvm::LLVMContext > ctx( new llvm::LLVMContext );
    return ctx;
}

auto mod2prog( std::unique_ptr< llvm::Module > m )
{
    auto p = std::make_shared< vm::Program >( m.release() );
    p->setupRR();
    p->computeRR();
    p->computeStatic();
    return p;
}

template< typename Build >
auto ir2prog( Build build, std::string funcname, llvm::FunctionType *ft = nullptr )
{
    if ( !ft )
        ft = llvm::FunctionType::get( llvm::Type::getInt32Ty( *testContext() ), false );
    auto m = std::make_unique< llvm::Module >( "test.ll", *testContext() );
    auto f = llvm::cast< llvm::Function >( m->getOrInsertFunction( funcname, ft ) );
    auto bb = llvm::BasicBlock::Create( *testContext(), "_entry", f );
    llvm::IRBuilder<> irb( bb );
    build( irb, f );
    return mod2prog( std::move( m ) );
}

auto c2prog( std::string s )
{
    divine::cc::Compiler c( testContext() );
    c.mapVirtualFile( "main.c", s );
    return mod2prog( c.compileModule( "main.c" ) );
}

}

struct Program
{
    llvm::LLVMContext &ctx;
    Program() : ctx( llvm::getGlobalContext() ) {}

    TEST( empty )
    {
        auto m = std::make_unique< llvm::Module >( "test", ctx );
        vm::Program p( m.get() );
    }

    TEST( simple )
    {
        auto m = c2prog( "int main() { return 0; }" );
    }
};

}
#endif
