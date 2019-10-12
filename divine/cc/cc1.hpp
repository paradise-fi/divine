// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-
#pragma once

/*
 * (c) 2016 Vladimír Štill
 * (c) 2018-2019 Zuzana Baranová
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

#include <divine/cc/vfs.hpp>
#include <divine/cc/filetype.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Tooling/Tooling.h> // ClangTool
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-except>
#include <string_view>

namespace divine::cc
{
    struct CompileError : brq::error
    {
        using brq::error::error;
    };

    void initTargets();

    struct Diagnostics
    {
        Diagnostics()
            : printer( llvm::errs(), new clang::DiagnosticOptions() ),
              engine(
                llvm::IntrusiveRefCntPtr< clang::DiagnosticIDs >( new clang::DiagnosticIDs() ),
                new clang::DiagnosticOptions(), &printer, false )
        {}

        clang::TextDiagnosticPrinter printer;
        clang::DiagnosticsEngine engine;
    };

    /*
     A compiler, capable of compiling a single file into LLVM module

     The compiler is not thread safe, and all modules are bound to its context (so
     they will not be usable when the compiler is destructed. However, as each
     compiler has its own context, multiple compilers in different threads will
     work
     */
    struct CC1
    {
        explicit CC1( std::shared_ptr< llvm::LLVMContext > ctx = nullptr );
        ~CC1();

        void mapVirtualFile( std::string path, std::string_view contents );
        void mapVirtualFile( std::string path, std::string contents );

        // T represents a string
        template< typename T >
        void mapVirtualFiles( std::initializer_list< std::pair< std::string, const T & > > files )
        {
            for ( auto &x : files )
                mapVirtualFile( x.first, x.second );
        }

        std::vector< std::string > filesMappedUnder( std::string path );
        void allowIncludePath( std::string path );

        std::unique_ptr< llvm::Module > compile( std::string filename,
                                    FileType type, std::vector< std::string > args );

        auto compile( std::string filename, std::vector< std::string > args = { } )
        {
            return compile( filename, typeFromFile( filename ), args );
        }

        std::string preprocess( std::string filename,
                                FileType type, std::vector< std::string > args );

        std::string preprocess( std::string filename, std::vector< std::string > args = { } )
        {
            return preprocess( filename, typeFromFile( filename ), args );
        }

        static std::string serializeModule( llvm::Module &m );

        std::shared_ptr< llvm::LLVMContext > context() { return ctx; }

        bool fileExists( llvm::StringRef file );
        std::unique_ptr< llvm::MemoryBuffer > getFileBuffer( llvm::StringRef file, int64_t size = -1 );

      private:
        template< typename CodeGenAction >
        std::unique_ptr< CodeGenAction > cc1( std::string filename,
                                    FileType type, std::vector< std::string > args,
                                    llvm::IntrusiveRefCntPtr< clang::vfs::FileSystem > vfs = nullptr );

        llvm::IntrusiveRefCntPtr< VFS > divineVFS;
        llvm::IntrusiveRefCntPtr< clang::vfs::OverlayFileSystem > overlayFS;
        std::shared_ptr< llvm::LLVMContext > ctx;
    };
}
