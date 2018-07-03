// -*- C++ -*- (c) 2016 Vladimír Štill
#pragma once

#include <divine/cc/vfs.hpp>
#include <divine/cc/filetype.hpp>

DIVINE_RELAX_WARNINGS
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <clang/Tooling/Tooling.h> // ClangTool
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-except>
#include <string_view>

namespace divine {
namespace cc {

struct CompileError : brick::except::Error
{
    using brick::except::Error::Error;
};

struct DivineVFS;

void initTargets();

/*
 A compiler, capable of compiling a single file into LLVM module

 The compiler is not thread safe, and all modules are bound to its context (so
 they will not be usable when the compiler is destructed. However, as each
 compiler has its own context, multiple compilers in different threads will
 work
 */
struct CC1 {

    explicit CC1( std::shared_ptr< llvm::LLVMContext > ctx = nullptr );
    ~CC1();

    void mapVirtualFile( std::string path, std::string_view contents );
    void mapVirtualFile( std::string path, std::string contents );

    template< typename T >
    void mapVirtualFiles( std::initializer_list< std::pair< std::string, const T & > > files ) {
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

    template< typename ... Args >
    std::string compileAndSerializeModule( Args &&... args ) {
        auto mod = compileModule( std::forward< Args >( args )... );
        return serializeModule( mod.get() );
    }

    std::shared_ptr< llvm::LLVMContext > context() { return ctx; }

    bool fileExists( llvm::StringRef file );
    std::unique_ptr< llvm::MemoryBuffer > getFileBuffer( llvm::StringRef file );

  private:
    template< typename CodeGenAction >
    std::unique_ptr< CodeGenAction > cc1( std::string filename,
                                FileType type, std::vector< std::string > args,
                                llvm::IntrusiveRefCntPtr< clang::vfs::FileSystem > vfs = nullptr );

    llvm::IntrusiveRefCntPtr< VFS > divineVFS;
    llvm::IntrusiveRefCntPtr< clang::vfs::OverlayFileSystem > overlayFS;
    std::shared_ptr< llvm::LLVMContext > ctx;
};
} // namespace cc
} // namespace divine
