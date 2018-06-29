// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2018 Petr Ročkai <code@fixp.eu>
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

#include <lart/support/fixphi.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CFG.h>
DIVINE_UNRELAX_WARNINGS

namespace lart
{

    void FixPHI::run( llvm::Module &m )
    {
        for ( auto &f : m )
            for ( auto &bb : f )
                for ( auto &phi : bb.phis() )
                    fix_phi( phi );
    }

    void FixPHI::fix_phi( llvm::PHINode &phi )
    {
        std::vector< llvm::Value * > in;
        for ( auto pred : llvm::predecessors( phi.getParent() ) )
            in.push_back( phi.getIncomingValueForBlock( pred ) );
        while ( phi.getNumOperands() )
            phi.removeIncomingValue( phi.getNumOperands() - 1, false );
        int idx = 0;
        for ( auto pred : llvm::predecessors( phi.getParent() ) )
            phi.addIncoming( in[ idx++ ], pred );
    }

}
