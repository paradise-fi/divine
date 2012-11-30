#include <divine/llvm/execution.h>
#include <divine/llvm/program.h>

using namespace divine::llvm;

void ProgramInfo::storeConstant( ProgramInfo::Value &v, ::llvm::Constant *C, char *global )
{
    GlobalContext econtext( *this, TD, global );
    if ( auto CE = dyn_cast< ::llvm::ConstantExpr >( C ) ) {
        ControlContext ccontext;
        Evaluator< GlobalContext, ControlContext > eval( *this, econtext, ccontext );

        Instruction &comp = eval.instruction;
        comp.op = CE;
        comp.opcode = CE->getOpcode();
        comp.values.push_back( v ); /* the result comes first */
        for ( int i = 0; i < CE->getNumOperands(); ++i ) // now the operands
            comp.values.push_back( insert( 0, CE->getOperand( i ) ) );
        eval.run(); /* compute and write out the value */
    } else if ( isa< ::llvm::UndefValue >( C ) )
            ; /* Nothing to do. */
    else if ( auto I = dyn_cast< ::llvm::ConstantInt >( C ) ) {
        const uint8_t *mem = reinterpret_cast< const uint8_t * >( I->getValue().getRawData() );
        std::copy( mem, mem + v.width, econtext.dereference( v ) );
    } else if ( C->getType()->isPointerTy() ) {
        if ( auto F = dyn_cast< ::llvm::Function >( C ) )
            constant< PC >( v ) = PC( functionmap[ F ], 0, 0 );
        else if ( auto B = dyn_cast< ::llvm::BlockAddress >( C ) )
            constant< PC >( v ) = blockmap[ B->getBasicBlock() ];
        else if ( auto B = dyn_cast< ::llvm::BasicBlock >( C ) )
            constant< PC >( v ) = blockmap[ B ];
        else if ( isa< ::llvm::ConstantPointerNull >( C ) )
            constant< Pointer >( v ) = Pointer();
        else assert_unreachable( "unknown constant pointer type" );
    } else if ( isa< ::llvm::ConstantAggregateZero >( C ) )
        ; /* nothing to do, everything is zeroed by default */
    else if ( auto CA = dyn_cast< ::llvm::ConstantArray >( C ) ) {
        Value sub = v; /* inherit offset & global/constant status */
        for ( int i = 0; i < CA->getNumOperands(); ++i ) {
            initValue( C->getOperand( i ), sub );
            storeConstant( sub, cast< ::llvm::Constant >( C->getOperand( i ) ), global );
            sub.offset += sub.width;
        }
    } else if ( auto CDS = dyn_cast< ::llvm::ConstantDataSequential >( C ) ) {
        assert_eq( v.width, CDS->getNumElements() * CDS->getElementByteSize() );
        const char *raw = CDS->getRawDataValues().data();
        std::copy( raw, raw + v.width, econtext.dereference( v ) );
    } else if ( auto CV = dyn_cast< ::llvm::ConstantVector >( C ) )
        assert_unimplemented();
    else if ( auto CS = dyn_cast< ::llvm::ConstantStruct >( C ) )
        assert_unimplemented();
    else assert_unreachable( "unknown constant type" );
}

