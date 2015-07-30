// -*- C++ -*- (c) 2012-2014 Petr Ročkai <me@mornfall.net>

#include <stdexcept>

#include <divine/llvm/program.h>

#include <llvm/IR/Type.h>
#include <llvm/IR/GlobalVariable.h>

#include <llvm/CodeGen/IntrinsicLowering.h>
#include <llvm/Support/CallSite.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Module.h>
#include <llvm/ADT/StringMap.h>

using namespace divine::llvm;
using ::llvm::isa;
using ::llvm::dyn_cast;
using ::llvm::cast;

bool ProgramInfo::isCodePointerConst( ::llvm::Value *val )
{
    return ( isa< ::llvm::Function >( val ) ||
             isa< ::llvm::BlockAddress >( val ) ||
             isa< ::llvm::BasicBlock >( val ) );
}

bool ProgramInfo::isCodePointer( ::llvm::Value *val )
{
    if ( auto ty = dyn_cast< ::llvm::PointerType >( val->getType() ) )
        return ty->getElementType()->isFunctionTy();

    return isCodePointerConst( val );
}

PC ProgramInfo::getCodePointer( ::llvm::Value *val )
{
    if ( auto B = dyn_cast< ::llvm::BasicBlock >( val ) ) {
        ASSERT( blockmap.count( B ) );
        return blockmap[ B ];
    } else if ( auto B = dyn_cast< ::llvm::BlockAddress >( val ) ) {
        ASSERT( blockmap.count( B->getBasicBlock() ) );
        return blockmap[ B->getBasicBlock() ];
    } else if ( auto F = dyn_cast< ::llvm::Function >( val ) ) {
        if ( !functionmap.count( F ) && builtin( F ) == NotBuiltin )
            throw std::logic_error(
                "ProgramInfo::insert: " +
                std::string( "Unresolved symbol (function): " ) + F->getName().str() );

        if ( builtin( F ) )
            return PC( 0, 0 );

        return PC( functionmap[ F ], 0 );
    }
    return PC();
}

namespace {

int intrinsic_id( ::llvm::Value *v ) {
    auto insn = dyn_cast< ::llvm::Instruction >( v );
    if ( !insn || insn->getOpcode() != ::llvm::Instruction::Call )
        return ::llvm::Intrinsic::not_intrinsic;
    llvm::CallSite CS( insn );
    auto f = CS.getCalledFunction();
    if ( !f )
        return ::llvm::Intrinsic::not_intrinsic;
    return f->getIntrinsicID();
}

}

void ProgramInfo::initValue( ::llvm::Value *val, ProgramInfo::Value &result )
{
    if ( val->getType()->isVoidTy() ) {
        result.width = 0;
        result.type = Value::Void;
    } else if ( isCodePointer( val ) ) {
        result.width = val->getType()->isPointerTy() ? TD.getTypeAllocSize( val->getType() ) : sizeof( PC );
        result.type = Value::CodePointer;
    } else if ( val->getType()->isPointerTy() ) {
        result.width = TD.getTypeAllocSize( val->getType() );
        result.type = Value::Pointer;
    } else {
        result.width = TD.getTypeAllocSize( val->getType() );
        if ( val->getType()->isIntegerTy() )
            result.type = Value::Integer;
        else if ( val->getType()->isFloatTy() || val->getType()->isDoubleTy() )
            result.type = Value::Float;
        else
            result.type = Value::Aggregate;
    }

    if ( auto CDS = dyn_cast< ::llvm::ConstantDataSequential >( val ) ) {
        result.type = Value::Aggregate;
        result.width = CDS->getNumElements() * CDS->getElementByteSize();
    }

    if ( isa< ::llvm::AllocaInst >( val ) ||
         intrinsic_id( val ) == ::llvm::Intrinsic::stacksave )
        result.type = ProgramInfo::Value::Alloca;
}

bool ProgramInfo::lifetimeOverlap( ::llvm::Value *v_a, ::llvm::Value *v_b ) {
    auto a = dyn_cast< ::llvm::Instruction >( v_a );
    auto b = dyn_cast< ::llvm::Instruction >( v_b );
    if ( !a || !b )
        return true;

    auto id = a->getMetadata( "lart.id" );
    auto list = b->getMetadata( "lart.interference" );

    if ( !list || !id )
        return true;
    for ( int i = 0; i < list->getNumOperands(); ++i ) {
        auto item = dyn_cast< ::llvm::MDNode >( list->getOperand( i ) );
        if ( item == id )
            return true;
    }
    return false;
}

bool ProgramInfo::overlayValue( int fun, Value &result, ::llvm::Value *val )
{
    if ( !result.width ) {
        result.offset = 0;
        return true;
    }

    int iter = 0;
    auto &f = functions[ fun ];
    makeFit( coverage, fun );
    auto &c = coverage[ fun ];

    auto insn = dyn_cast< ::llvm::Instruction >( val );
    if ( !insn || !insn->getMetadata( "lart.interference" ) )
        goto alloc;

    ASSERT( !f.instructions.empty() );
    c.resize( f.datasize );

    for ( result.offset = 0; result.offset < f.datasize; ) {
        iter ++;
        if ( result.offset + result.width > f.datasize )
            goto alloc;

        bool good = true;
        for ( int i = 0; good && i < result.width; ++i )
            for ( auto v : c[ result.offset + i ] )
                if ( lifetimeOverlap( val, v ) ) {
                    result.offset = valuemap[ v ].offset + valuemap[ v ].width;
                    good = false;
                    break;
                }

        if ( good )
            goto out;
    }
alloc:
    result.offset = f.datasize;
    f.datasize += result.width;
    c.resize( f.datasize );
out:
    c[ result.offset ].push_back( val );
    return true;
}

ProgramInfo::Value ProgramInfo::insert( int function, ::llvm::Value *val )
{
    if ( valuemap.find( val ) != valuemap.end() )
        return valuemap.find( val )->second;

    makeFit( functions, function );

    Value result; /* not seen yet, needs to be allocated */
    initValue( val, result );
    bool constglobal = true;

    auto infopair = std::make_pair(
        val->getType(), val->getValueName() ? val->getValueName()->getKey() : "" );

    if ( result.width % framealign )
        return Value(); /* ignore for now, later pass will assign this one */

    if ( isCodePointerConst( val ) ) {
        makeConstant( result, getCodePointer( val ) );
        doneInit.insert( val );
    } else if ( auto GA = dyn_cast< ::llvm::GlobalAlias >( val ) )
        result = insert( function, const_cast< ::llvm::GlobalValue * >( GA->resolveAliasedGlobal() ) );
    else if ( auto C = dyn_cast< ::llvm::Constant >( val ) ) {
        result.global = true;
        if ( auto G = dyn_cast< ::llvm::GlobalVariable >( val ) ) {
            Value pointee;
            if ( !G->hasInitializer() )
                throw std::logic_error(
                    "ProgramInfo::insert: " +
                    std::string( "Unresolved symbol (global variable): " ) +
                    G->getValueName()->getKey().str() );
            ASSERT( G->hasInitializer() );
            initValue( G->getInitializer(), pointee );
            if ( (pointee.constant = G->isConstant()) )
                makeLLVMConstant( pointee, G->getInitializer() );
            else {
                allocateValue( 0, pointee );
                globalvars.emplace_back( globals.size(), pointee );
            }
            globals.push_back( pointee );
            Pointer p( false, globals.size() - 1, 0 );
            makeConstant( result, p );
            doneInit.insert( val );

            valuemap.insert( std::make_pair( val, result ) );
            if ( !G->isConstant() ) {
                storeConstant( pointee, G->getInitializer(), true );
                constglobal = false;
            }
        } else makeLLVMConstant( result, C );
    } else allocateValue( function, result, val );

    if ( function && !result.global && !result.constant ) {
        if ( result.width )
            this->functions[ function ].values.push_back( result );
    } else {
        makeFit( constglobal ? constinfo : globalinfo, globals.size() );
        (constglobal ? constinfo : globalinfo )[ globals.size() ] = infopair;
        globals.push_back( result );
    }

    valuemap.insert( std::make_pair( val, result ) );
    return result;
}

/* This is silly. */
ProgramInfo::Position ProgramInfo::lower( Position p )
{
    auto BB = p.I->getParent();
    bool first = p.I == BB->begin();
    auto insert = p.I;

    if ( !first ) --insert;
    IL->LowerIntrinsicCall( cast< ::llvm::CallInst >( p.I ) );
    if ( first )
        insert = BB->begin();
    else
        insert ++;

    return Position( p.pc, insert );
}

Builtin ProgramInfo::builtin( ::llvm::Function *f )
{
    std::string name = f->getName().str();
    if ( name == "__divine_interrupt_mask" )
        return BuiltinMask;
    if ( name == "__divine_interrupt_unmask" )
        return BuiltinUnmask;
    if ( name == "__divine_interrupt" )
        return BuiltinInterrupt;
    if ( name == "__divine_get_tid" )
        return BuiltinGetTID;
    if ( name == "__divine_new_thread" )
        return BuiltinNewThread;
    if ( name == "__divine_choice" )
        return BuiltinChoice;
    if ( name == "__divine_assert" )
        return BuiltinAssert;
    if ( name == "__divine_problem" )
        return BuiltinProblem;
    if ( name == "__divine_ap" )
        return BuiltinAp;
    if ( name == "__divine_malloc" )
        return BuiltinMalloc;
    if ( name == "__divine_heap_object_size" )
        return BuiltinHeapObjectSize;
    if ( name == "__divine_free" )
        return BuiltinFree;
    if ( name == "__divine_va_start" )
        return BuiltinVaStart;
    if ( name == "__divine_unwind" )
        return BuiltinUnwind;
    if ( name == "__divine_landingpad" )
        return BuiltinLandingPad;
    if ( name == "__divine_memcpy" )
        return BuiltinMemcpy;
    if ( name == "__divine_is_private" )
        return BuiltinIsPrivate;

    if ( f->getIntrinsicID() != ::llvm::Intrinsic::not_intrinsic )
        return BuiltinIntrinsic; /* not our builtin */

    return NotBuiltin;
}

void ProgramInfo::builtin( Position p )
{
    ProgramInfo::Instruction &insn = instruction( p.pc );
    ::llvm::CallSite CS( p.I );
    ::llvm::Function *F = CS.getCalledFunction();
    insn.builtin = builtin( F );
    if ( insn.builtin == NotBuiltin )
        throw std::logic_error(
            std::string( "ProgramInfo::builtin: " ) +
            "Can't call an undefined function: " + F->getName().str() );
}

template< typename Insn >
void ProgramInfo::insertIndices( Position p )
{
    Insn *I = cast< Insn >( p.I );
    ProgramInfo::Instruction &insn = instruction( p.pc );

    int shift = insn.values.size();
    insn.values.resize( shift + I->getNumIndices() );

    for ( unsigned i = 0; i < I->getNumIndices(); ++i ) {
        Value v;
        v.width = sizeof( unsigned );
        makeConstant( v, I->getIndices()[ i ] );
        insn.values[ shift + i ] = v;
    }
}

ProgramInfo::Position ProgramInfo::insert( Position p )
{
    makeFit( function( p.pc ).instructions, p.pc.instruction );
    ProgramInfo::Instruction &insn = instruction( p.pc );

    insn.opcode = p.I->getOpcode();
    insn.op = &*p.I;

    if ( dyn_cast< ::llvm::CallInst >( p.I ) ||
         dyn_cast< ::llvm::InvokeInst >( p.I ) )
    {
        ::llvm::CallSite CS( p.I );
        ::llvm::Function *F = CS.getCalledFunction();
        if ( F ) { // you can actually invoke a label
            switch ( F->getIntrinsicID() ) {
                case ::llvm::Intrinsic::not_intrinsic:
                    if ( builtin( F ) ) {
                        builtin( p );
                        break;
                    }
                case ::llvm::Intrinsic::eh_typeid_for:
                case ::llvm::Intrinsic::trap:
                case ::llvm::Intrinsic::vastart:
                case ::llvm::Intrinsic::vacopy:
                case ::llvm::Intrinsic::vaend:
                case ::llvm::Intrinsic::stacksave:
                case ::llvm::Intrinsic::stackrestore:
                case ::llvm::Intrinsic::umul_with_overflow:
                case ::llvm::Intrinsic::smul_with_overflow:
                case ::llvm::Intrinsic::uadd_with_overflow:
                case ::llvm::Intrinsic::sadd_with_overflow:
                case ::llvm::Intrinsic::usub_with_overflow:
                case ::llvm::Intrinsic::ssub_with_overflow:
                    break;
                case ::llvm::Intrinsic::dbg_declare:
                case ::llvm::Intrinsic::dbg_value: p.I++; return p;
                default: return lower( p );
            }
        }
    }

    if ( !codepointers )
    {
        insn.values.resize( 1 + p.I->getNumOperands() );
        for ( int i = 0; i < int( p.I->getNumOperands() ); ++i )
            insn.values[ i + 1 ] = insert( p.pc.function, p.I->getOperand( i ) );

        if ( isa< ::llvm::ExtractValueInst >( p.I ) )
            insertIndices< ::llvm::ExtractValueInst >( p );

        if ( isa< ::llvm::InsertValueInst >( p.I ) )
            insertIndices< ::llvm::InsertValueInst >( p );

        if ( auto LPI = dyn_cast< ::llvm::LandingPadInst >( p.I ) ) {
            for ( int i = 0; i < int( LPI->getNumClauses() ); ++i ) {
                if ( LPI->isFilter( i ) )
                    continue;
                Pointer ptr = constant< Pointer >( insn.operand( i + 1 ) );
                if ( !function( p.pc ).typeID( ptr ) )
                    function( p.pc ).typeIDs.push_back( ptr );
                ASSERT( function( p.pc ).typeID( ptr ) );
            }
        }

        pcmap.insert( std::make_pair( p.I, p.pc ) );
        insn.result() = insert( p.pc.function, &*p.I );
    }

    ++ p.I; /* next please */

    if ( ! ++ p.pc.instruction )
        throw std::logic_error(
            "ProgramInfo::insert() in " + p.I->getParent()->getParent()->getName().str() +
            "\nToo many instructions in a basic block, capacity exceeded" );

    return p;
}

void ProgramInfo::build()
{
    /* null pointers are heap = 0, segment = 0, where segment is an index into
     * the "globals" vector */
    Value nullpage;
    nullpage.offset = 0;
    nullpage.width = 0;
    nullpage.global = true;
    nullpage.constant = false;
    globals.push_back( nullpage );

    for ( auto &f : *module )
        if ( f.getName() == "memset" || f.getName() == "memmove" || f.getName() == "memcpy" )
            f.setLinkage( ::llvm::GlobalValue::ExternalLinkage );

    framealign = 1;

    codepointers = true;
    pass();
    codepointers = false;

    for ( auto var = module->global_begin(); var != module->global_end(); ++ var )
        insert( 0, &*var );

    framealign = 4;
    pass();

    framealign = 1;
    pass();

    while ( !toInit.empty() ) {
        auto x = toInit.front();
        toInit.pop_front();
        storeConstant( std::get< 0 >( x ), std::get< 1 >( x ), std::get< 2 >( x ) );
    }

    coverage.clear();
}

void ProgramInfo::pass()
{
    PC pc( 1, 0 );
    int _framealign = framealign;

    for ( auto function = module->begin(); function != module->end(); ++ function )
    {
        if ( function->isDeclaration() )
            continue; /* skip */

        auto name = function->getName();

        if ( !++ pc.function )
            throw std::logic_error(
                "ProgramInfo::build in " + name.str() +
                "\nToo many functions, capacity exceeded" );

        if ( function->begin() == function->end() )
            throw std::logic_error(
                "ProgramInfo::build in " + name.str() +
                "\nCan't deal with empty functions" );

        makeFit( functions, pc.function );
        functionmap[ function ] = pc.function;
        pc.instruction = 0;

        if ( !codepointers ) {
            framealign = 1; /* force all args to go in in the first pass */

            auto &pi_function = this->functions[ pc.function ];
            pi_function.argcount = 0;

            for ( auto arg = function->arg_begin(); arg != function->arg_end(); ++ arg ) {
                insert( pc.function, &*arg );
                ++ pi_function.argcount;
            }

            if ( ( pi_function.vararg = function->isVarArg() ) ) {
                Value vaptr;
                vaptr.width = TD.getPointerSize();
                vaptr.type = Value::Pointer;
                allocateValue( pc.function, vaptr );
                pi_function.values.push_back( vaptr );
            }

            framealign = _framealign;

            this->function( pc ).datasize =
                align( this->function( pc ).datasize, framealign );
        }

        for ( auto block = function->begin(); block != function->end(); ++ block )
        {
            blockmap[ &*block ] = pc;

            if ( block->begin() == block->end() )
                throw std::logic_error(
                    std::string( "ProgramInfo::build: " ) +
                    "Can't deal with an empty BasicBlock in function " + name.str() );

            ProgramInfo::Position p( pc, block->begin() );
            while ( p.I != block->end() )
                p = insert( p );

            pc = p.pc;
        }
    }

    globalsize = align( globalsize, 4 );
}

