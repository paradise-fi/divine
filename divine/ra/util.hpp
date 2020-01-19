/*
 * (c) 2020 Lukáš Korenčik  <xkorenc1@fi.muni.cz>
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

#include <memory>

#include <bricks/brick-llvm>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS


namespace divine::ra::util {

std::unique_ptr< llvm::Module > load_bc( const std::string &str, llvm::LLVMContext *ctx )
{
    using namespace llvm::object;

    std::unique_ptr< llvm::MemoryBuffer > input =
        std::move( llvm::MemoryBuffer::getFile( str ).get() );
    auto bc_input = IRObjectFile::findBitcodeInMemBuffer( input->getMemBufferRef() );

    if ( !bc_input )
        UNREACHABLE( "Could not load bitcode file" );
    auto module = llvm::parseBitcodeFile( bc_input.get(), *ctx );

    if ( !module )
        UNREACHABLE( "Error parsing input model; probably not a valid bc file." );

    return std::move( module.get() );
}

} // namespace divine::ra::util
