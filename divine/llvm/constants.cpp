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
    } else if ( auto I = dyn_cast< ::llvm::ConstantInt >( C ) ) {
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
        else assert_die();
    } else if ( isa< ::llvm::ConstantAggregateZero >( C ) )
        ; /* nothing to do, everything is zeroed by default */
    else assert_die();
}

