#include <lart/support/lowerselect.h>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/ADT/Statistic.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/query.h>

using namespace llvm;

char lart::LowerSelectPass::id = 0;

// This pass converts SelectInst instructions into conditional branch and PHI
// instructions.
bool lart::LowerSelectPass::runOnFunction( Function &fn )
{
    auto selects = query::query( fn ).flatten()
        .map( query::refToPtr )
        .map( query::llvmdyncast< llvm::SelectInst > )
        .filter( query::notnull )
        .freeze();

    for ( auto sel : selects )
        lower( sel );
    return !selects.empty();
}

void lart::LowerSelectPass::lower( SelectInst *si )
{
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
