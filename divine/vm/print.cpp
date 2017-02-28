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

#include <divine/vm/print.hpp>

using namespace std::literals;

namespace divine {
namespace vm {
namespace print {

#define HANDLE_INST(num, opc, class) [num] = #opc ## s,
static std::string _opcode[] = {
#include <llvm/IR/Instruction.def>
};
#undef HANDLE_INST

void opcode_tolower()
{
    static bool done = false;
    if ( done )
        return;
    for ( int i = 0; i < int( sizeof( _opcode ) / sizeof( _opcode[0] ) ); ++i )
        std::transform( _opcode[i].begin(), _opcode[i].end(),
                        _opcode[i].begin(), ::tolower );
    done = true;
}

std::string opcode( int op )
{
    if ( op == OpDbg )
        return "dbg";
    opcode_tolower();
    return _opcode[ op ];
}

}
}
}
