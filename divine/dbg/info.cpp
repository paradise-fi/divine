// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2017 Petr Ročkai <code@fixp.eu>
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

#include <divine/dbg/info.hpp>

namespace divine::dbg
{

std::pair< llvm::StringRef, int > fileline( const llvm::Instruction &insn )
{
    auto loc = insn.getDebugLoc().get();
    if ( loc && loc->getNumOperands() )
        return std::make_pair( loc->getFilename(),
                               loc->getLine() );
    auto prog = insn.getParent()->getParent()->getSubprogram();
    if ( prog )
        return std::make_pair( prog->getFilename(),
                               prog->getScopeLine() );
    return std::make_pair( "", 0 );
}

std::pair< llvm::StringRef, int > Info::fileline( vm::CodePointer pc )
{
    auto npc = _program.nextpc( pc );
    auto op = find( nullptr, npc ).first;
    return dbg::fileline( *op );
}

std::string location( const llvm::Instruction &insn )
{
    return location( fileline( insn ) );
}

std::string location( std::pair< llvm::StringRef, int > fl )
{
    if ( fl.second )
        return fl.first.str() + ":" + brick::string::fmt( fl.second );
    return "(unknown location)";
}

bool Info::in_component( vm::CodePointer pc, Components comp )
{
    auto file = fileline( pc ).first;
    using brick::string::startsWith;

    if ( comp & Component::LibC )
    {
        if ( startsWith( file, "/dios/libc/" ) ||
             startsWith( file, "/dios/arch/" ) ||
             startsWith( file, "/dios/include/sys/" ) )
            return true;
        if ( startsWith( file, "/dios/include/" ) && file.substr( 14 ).find( '/' ) == std::string::npos )
            return true;
    }

    if ( comp & Component::LibCxx )
        if ( startsWith( file, "/dios/libcxx" ) )
            return true;

    if ( comp & Component::LibRst )
        if ( startsWith( file, "/dios/rst/" ) ||
             startsWith( file, "/dios/include/rst/" ) )
            return true;

    if ( comp & Component::DiOS )
        if ( startsWith( file, "/dios/sys/" ) || startsWith( file, "/dios/vfs/" ) )
            return true;

    if ( comp & Component::Program && !startsWith( file, "/dios/" ) )
        return true;

    return false;
}

}
