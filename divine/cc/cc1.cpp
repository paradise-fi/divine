// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

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

DIVINE_RELAX_WARNINGS
#include <clang/CodeGen/CodeGenAction.h> // EmitLLVMAction
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/CompilerInstance.h> // CompilerInvocation
#include <clang/Frontend/DependencyOutputOptions.h>
#include <clang/Frontend/Utils.h>
#include <llvm/Bitcode/BitcodeWriter.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-assert>
#include <iostream>

#include <divine/cc/cc1.hpp>
#include <lart/divine/vaarg.h>

namespace divine::str
{
    struct stringtable { std::string n; std::string_view c; };
    extern stringtable cc_list[];
}

namespace divine::cc
{
    class GetPreprocessedAction : public clang::PreprocessorFrontendAction
    {
      public:
        std::string output;
      protected:
        void ExecuteAction() override
        {
            clang::CompilerInstance &CI = getCompilerInstance();
            llvm::raw_string_ostream os( output );
            clang::DoPrintPreprocessedInput(CI.getPreprocessor(), &os,
                                CI.getPreprocessorOutputOpts());
        }

        bool hasPCHSupport() const override { return true; }
    };

    template < typename Action >
    auto buildAction( llvm::LLVMContext *ctx )
        -> decltype( std::enable_if_t< std::is_base_of< clang::CodeGenAction, Action >::value,
            std::unique_ptr< Action > >{} )
    {
        return std::make_unique< Action >( ctx );
    }

    template < typename Action >
    auto buildAction( llvm::LLVMContext * )
        -> decltype( std::enable_if_t< !std::is_base_of< clang::CodeGenAction, Action >::value,
            std::unique_ptr< Action > >{} )
    {
        return std::make_unique< Action >();
    }

    CC1::CC1( std::shared_ptr< llvm::LLVMContext > ctx ) :
        divineVFS( new VFS() ),
        overlayFS( new clang::vfs::OverlayFileSystem( clang::vfs::getRealFileSystem() ) ),
        ctx( ctx )
    {
        if ( !ctx )
            this->ctx = std::make_shared< llvm::LLVMContext >();
        // setup VFS
        overlayFS->pushOverlay( divineVFS );
        if ( !ctx )
            ctx.reset( new llvm::LLVMContext );

        for ( auto src = str::cc_list; !src->n.empty(); ++src )
            mapVirtualFile( brick::fs::joinPath( "/builtin/", src->n ), src->c );
    }

    CC1::~CC1() { }

    void CC1::mapVirtualFile( std::string path, std::string contents )
    {
        divineVFS->addFile( path, std::move( contents ) );
    }

    void CC1::mapVirtualFile( std::string path, std::string_view contents )
    {
        divineVFS->addFile( path, llvm::StringRef( contents.data(), contents.size() ) );
    }

    std::vector< std::string > CC1::filesMappedUnder( std::string path )
    {
        return divineVFS->filesMappedUnder( path );
    }

    void CC1::allowIncludePath( std::string path )
    {
        divineVFS->allowPath( path );
    }

    template< typename CodeGenAction >
    std::unique_ptr< CodeGenAction > CC1::cc1( std::string filename,
                                FileType type, std::vector< std::string > args,
                                llvm::IntrusiveRefCntPtr< clang::vfs::FileSystem > vfs )
    {
        if ( !vfs )
            vfs = overlayFS;

        // Build an invocation
        auto invocation = std::make_shared< clang::CompilerInvocation >();
        std::vector< std::string > cc1args = { "-cc1",
                                                "-triple", "x86_64-unknown-none-elf",
                                                "-emit-obj",
                                                "-mrelax-all",
                                                // "-disable-free",
                                                // "-disable-llvm-verifier",
                                                // "-main-file-name", "test.c",
                                                "-mrelocation-model", "static",
                                                "-mthread-model", "posix",
                                                "-mdisable-fp-elim",
                                                "-fmath-errno",
                                                "-masm-verbose",
                                                "-mconstructor-aliases",
                                                "-munwind-tables",
                                                "-fuse-init-array",
                                                "-target-cpu", "x86-64",
                                                "-dwarf-column-info",
                                                // "-coverage-file", "/home/xstill/DiVinE/clangbuild/test.c", // ???
                                                // "-resource-dir", "../lib/clang/3.7.1", // ???
                                                // "-internal-isystem", "/usr/local/include",
                                                // "-internal-isystem", "../lib/clang/3.7.1/include",
                                                // "-internal-externc-isystem", "/include",
                                                // "-internal-externc-isystem", "/usr/include",
                                                // "-fdebug-compilation-dir", "/home/xstill/DiVinE/clangbuild", // ???
                                                "-ferror-limit", "19",
                                                "-fmessage-length", "212",
                                                "-mstackrealign",
                                                "-fobjc-runtime=gcc",
                                                "-fdiagnostics-show-option",
                                                "-fcolor-diagnostics",
                                                // "-o", "test.o",
                                                "-isystem", "/builtin"
                                                };
        bool exceptions = type == FileType::Cpp || type == FileType::CppPreprocessed;

        for ( auto &a : args )
            if ( a == "-fno-exceptions" )
                exceptions = false;

        add( args, cc1args );
        add( args, argsOfType( type ) );
        if ( exceptions )
            add( args, { "-fcxx-exceptions", "-fexceptions" } );
        args.push_back( filename );

        std::vector< const char * > cc1a;

        for ( auto &a : args )
            if ( a != "-fno-exceptions" )
                cc1a.push_back( a.c_str() );

        Diagnostics diag;
        bool succ = clang::CompilerInvocation::CreateFromArgs(
                            *invocation, &cc1a[ 0 ], &*cc1a.end(), diag.engine );
        if ( !succ )
            throw CompileError( "Failed to create a compiler invocation for " + filename );
        invocation->getDependencyOutputOpts() = clang::DependencyOutputOptions();

        // actually run the compiler invocation
        clang::CompilerInstance compiler( std::make_shared< clang::PCHContainerOperations >() );
        auto fmgr = std::make_unique< clang::FileManager >( clang::FileSystemOptions(), vfs );
        compiler.setFileManager( fmgr.get() );
        compiler.setInvocation( invocation );
        ASSERT( compiler.hasInvocation() );
        compiler.createDiagnostics( &diag.printer, false );
        ASSERT( compiler.hasDiagnostics() );
        compiler.createSourceManager( *fmgr.release() );
        compiler.getPreprocessorOutputOpts().ShowCPP = 1; // Allows for printing preprocessor
        auto emit = buildAction< CodeGenAction >( ctx.get() );
        succ = compiler.ExecuteAction( *emit );
        if ( !succ )
            throw CompileError( "Error building " + filename );

        return emit;
    }

    std::string  CC1::preprocess( std::string filename,
                                FileType type, std::vector< std::string > args )
    {
        auto prep = cc1< GetPreprocessedAction >( filename, type, args );
        return prep->output;
    }

    std::unique_ptr< llvm::Module > CC1::compile( std::string filename,
                                FileType type, std::vector< std::string > args )
    {
        // EmitLLVMOnlyAction emits module in memory, does not write it into a file
        auto emit = cc1< clang::EmitLLVMOnlyAction >( filename, type, args );
        auto mod = emit->takeModule();
        lart::divine::VaArgInstr().run( *mod );
        return mod;
    }

    std::string CC1::serializeModule( llvm::Module &m )
    {
        std::string str;
        {
            llvm::raw_string_ostream os( str );
            llvm::WriteBitcodeToFile( m, os );
            os.flush();
        }
        return str;
    }

    bool CC1::fileExists( llvm::StringRef file )
    {
        auto stat = overlayFS->status( file );
        return stat && stat->exists();
    }

    std::unique_ptr< llvm::MemoryBuffer > CC1::getFileBuffer( llvm::StringRef file )
    {
        auto buf = overlayFS->getBufferForFile( file );
        return buf ? std::move( buf.get() ) : nullptr;
    }

} // namespace divine
