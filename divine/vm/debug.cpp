// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/debug.hpp>

namespace divine {
namespace vm {

std::pair< std::string, int > fileline( const llvm::Instruction &insn )
{
    auto loc = insn.getDebugLoc().get();
    if ( loc && loc->getNumOperands() )
        return std::make_pair( loc->getFilename().str(),
                               loc->getLine() );
    auto prog = llvm::getDISubprogram( insn.getParent()->getParent() );
    if ( prog )
        return std::make_pair( prog->getFilename().str(),
                               prog->getScopeLine() );
    return std::make_pair( "", 0 );
}

std::string location( const llvm::Instruction &insn )
{
    auto fl = fileline( insn );
    if ( fl.second )
        return fl.first + ":" + brick::string::fmt( fl.second );
    return "(unknown location)";
}

}
}
