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
#include <divine/vm/context.hpp>

#include <divine/vm/lx-type.hpp>
#include <divine/vm/lx-code.hpp>
#include <divine/vm/lx-slot.hpp>
#include <divine/vm/xg-type.hpp>
#include <divine/vm/xg-code.hpp>

#include <divine/vm/divm.h>

#include <map>
#include <unordered_map>

namespace divine::vm
{

/* A representation of the LLVM program that is (more) suitable for execution.
 * Functions are represented by flat vectors of instructions, so within a basic
 * block, we hit consecutive memory locations and don't need to dereference
 * pointers. Likewise, the entire program is represented as a vector of
 * Function instances (which each contain a pointer to the first instruction
 * belonging to that function).
 *
 * Instruction operands are always slot references, there is no support for
 * immediates (yet?). Constant data is all stashed away in a single heap object
 * which is split into a number of slots of variable size, each containing a
 * single value. Constants are not de-duplicated. This arrangement is
 * unfortunately not very cache-friendly. Within an instruction, operands are
 * held in a SmallVector, so most instructions should be stored continuously,
 * but they are still rather inefficient. A better representation is in the
 * works. */

struct Program
{
    llvm::DataLayout TD;
    using Slot = lx::Slot;

    /* Encoding of a single LLVM instruction. The opcode is the LLVM opcode or
     * a few DiVM-specific specials which arise from llvm 'call' instructions.
     * The subcode is either the LLVM sub-operation (for the icmp, fcmp and
     * atomicrmw multi-purpose opcodes) or an index into the type table (for
     * GEP or alloca) or the offset of the landing pad for invoke instructions.
     * For 'call' instructions, the subcode (if nonzero) indicates the ID of
     * the intrinsic function. */

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

    /* Temporary data used during generation of the RR, tying LLVM data
     * structures to our internal representation of the program (RR).
     * Unfortunately, parts of the code (especially divine::dbg) still rely on
     * those maps at runtime, hence we cannot clear them after the RR is built.
     * Fixing this is a work in progress. */

    std::map< const llvm::Value *, Slot > valuemap;
    std::map< const llvm::Value *, Slot > globalmap;
    std::map< const llvm::Type *, int > typemap;
    std::map< const llvm::Value *, std::string > anonmap;
    std::set< const llvm::Function * > is_debug;
    std::unordered_set< int > is_trap;
    std::set< HeapPointer > metadata_ptr;

    using Context = ctx_const< Program, SmallHeap >;
    Context _ccontext;

    using LXTypes = lx::Types< typename Context::Heap >;

    /* New-style generators for parts of the runtime representation that has
     * been moved over to a newer format (the new format is called LX for LLVM
     * eXecutable, the translation from LLVM to LX is called XG, for eXecutable
     * Generator). */

    xg::Types _types_gen;
    xg::AddressMap _addr;

    /* New-style encoding for type information. This is mainly used for GEP
     * (getelementptr) instructions, since those need to know the size of each
     * type that appears in them, because those sizes may need to be multiplied
     * by runtime values to compute the result. */

    std::unique_ptr< LXTypes > _types;

    CodePointer _bootpoint;
    GlobalPointer _envptr;

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

    /* Get the next instruction that should be executed -- skips debug metadata
     * and other helper instructions. */

    CodePointer nextpc( CodePointer pc )
    {
        if ( !valid( pc ) )
            return pc;
        auto op = instruction( pc ).opcode;
        if ( valid( pc + 1 ) && ( op == lx::OpBB || op == lx::OpDbg ) )
            return nextpc( pc + 1 );
        return pc;
    }

    CodePointer entry( CodePointer pc )/* entry point of the function that 'pc' points into */
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

    bool traps( CodePointer pc ) { return is_trap.count( pc.function() ); }

    Slot allocateSlot( Slot slot, int function = 0, llvm::Value *val = nullptr );
    HeapPointer s2hptr( Slot s, int offset = 0 );

    using Coverage = std::vector< std::vector< llvm::Value * > >;
    std::vector< Coverage > coverage;

    bool lifetimeOverlap( llvm::Value *a, llvm::Value *b );
    void overlaySlot( int fun, Slot &result, llvm::Value *val );

    std::deque< std::function< void() > > _toinit;
    std::set< llvm::Value * > _doneinit;

    CodePointer bootpoint() { return _bootpoint; }
    GlobalPointer envptr() { return _envptr; }

    /* Inefficient (iterates over all functions), and only works if the named
     * function is unique (i.e. only one module defines a function by this
     * name). Only use this in special circumstances. */
    CodePointer functionByName( std::string s );

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

    void pass( llvm::Module * ); /* internal */

    void setupRR( llvm::Module * );
    void computeRR( llvm::Module * ); /* RR = runtime representation */
    void computeStatic( llvm::Module * );

    /* the construction sequence is this:
       1) constructor
       2) setupRR
       3a) (optional) set up additional constant/global data
       3b) computeRR
       4) computeStatic */
    Program( llvm::DataLayout l ) : TD( l ), _types_gen( l )
    {
        _ccontext.program( *this );
        _constants_size = 0;
        _globals_size = 0;
    }
};

}
