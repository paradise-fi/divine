#include <lart/support/lowerselect.h>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/ADT/Statistic.h>
DIVINE_UNRELAX_WARNINGS

using namespace llvm;

// This pass converts SelectInst instructions into conditional branch and PHI
// instructions.
bool lart::LowerSelect::runOnFunction( Function &fn ) {
    bool changed = false;
    for ( auto & bb : fn )
        for ( auto & i : bb ) {
            if ( auto si = dyn_cast<SelectInst>( &i ) ) {
                lower( si );
                changed = true;
                break; // This block is done with.
            }
        }
    return changed;
}

void lart::LowerSelect::lower( SelectInst *si ) {
    BasicBlock * bb = si->getParent();
    // Split this basic block in half right before the select instruction.
    BasicBlock * newCont = bb->splitBasicBlock( si, bb->getName()+".selectcont" );

    // Make the true block, and make it branch to the continue block.
    BasicBlock * newTrue = BasicBlock::Create( bb->getContext(),
                 bb->getName()+".selecttrue", bb->getParent(), newCont );
    BranchInst::Create( newCont, newTrue );

    // Make the unconditional branch in the incoming block be a
    // conditional branch on the select predicate.
    bb->getInstList().erase( bb->getTerminator() );
    BranchInst::Create( newTrue, newCont, si->getCondition(), bb );

    // Create a new PHI node in the cont block with the entries we need.
    std::string name = si->getName(); si->setName("");
    PHINode *pn = PHINode::Create( si->getType(), 2, name, &*newCont->begin() );
    pn->addIncoming( si->getTrueValue(), newTrue );
    pn->addIncoming( si->getFalseValue(), bb );

    // Use the PHI instead of the select.
    si->replaceAllUsesWith( pn );
    newCont->getInstList().erase( si );
}
