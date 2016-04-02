// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2015 Petr Ročkai <code@fixp.eu>
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

#include <divine/vm/program.hpp>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/LLVMContext.h>

namespace divine {
namespace vm {

struct BitCode {
    std::unique_ptr< llvm::MemoryBuffer > _input;
    std::shared_ptr< llvm::Module > _module;
    llvm::LLVMContext *_ctx;
    Program *_program;

    Program &program() { ASSERT( _program ); return *_program; }

    BitCode( std::string file )
    {
        _ctx = new llvm::LLVMContext();
        _input = std::move( llvm::MemoryBuffer::getFile( file ).get() );
        auto parsed = llvm::parseBitcodeFile( _input->getMemBufferRef(), *_ctx );
        if ( !parsed )
            throw std::runtime_error( "Error parsing input model; probably not a valid bitcode file." );
        _module = std::move( parsed.get() );
        _program = new Program( _module.get() );
    }

    BitCode( std::shared_ptr< llvm::Module > m )
        : _ctx( nullptr ), _module( m )
    {
        ASSERT( _module );
        _program = new Program( _module.get() );
    }

    ~BitCode()
    {
        if ( _ctx )
            ASSERT_EQ( _module.use_count(), 1 );
        _module.reset();
        delete _program;
        delete _ctx;
    }
};

}
}
