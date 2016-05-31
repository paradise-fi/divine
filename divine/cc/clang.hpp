// -*- C++ -*- (c) 2016 Vladimír Štill
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <clang/Tooling/Tooling.h> // ClangTool
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-assert>
#include <brick-except>

namespace divine {
namespace cc {

struct CompileError : brick::except::Error
{
    using brick::except::Error::Error;
};

struct DivineVFS;

/*
 A compiler, cabaple of compiling single file into LLVM module

 The compiler is not thread safe, and all modules are bound to its context (so
 they will not be usable when the compiler is destructed. However, as each
 compiler has its own context, multiple compilers in different threads will
 work
 */
struct Compiler {

    enum class FileType {
        Unknown,
        C, Cpp, CPrepocessed, CppPreprocessed, IR, BC
    };

    static FileType typeFromFile( std::string name );
    static std::vector< std::string > argsOfType( FileType t );

    explicit Compiler( std::shared_ptr< llvm::LLVMContext > ctx = nullptr );
    ~Compiler();

    void mapVirtualFile( std::string path, const char *contents );
    void mapVirtualFile( std::string path, const std::string &contents );

    template< typename T >
    void mapVirtualFiles( std::initializer_list< std::pair< std::string, const T & > > files ) {
        for ( auto &x : files )
            mapVirtualFile( x.first, x.second );
    }

    std::vector< std::string > filesMappedUnder( std::string path );
    void allowIncludePath( std::string path );

    std::unique_ptr< llvm::Module > compileModule( std::string filename,
                                FileType type, std::vector< std::string > args );

    auto compileModule( std::string filename, std::vector< std::string > args = { } )
    {
        return compileModule( filename, typeFromFile( filename ), args );
    }

    static std::string serializeModule( llvm::Module &m );

    template< typename ... Args >
    std::string compileAndSerializeModule( Args &&... args ) {
        auto mod = compileModule( std::forward< Args >( args )... );
        return serializeModule( mod.get() );
    }

    // parse module from string ref
    std::unique_ptr< llvm::Module > materializeModule( llvm::StringRef str );

    std::shared_ptr< llvm::LLVMContext > context() { return ctx; }

  private:
    llvm::IntrusiveRefCntPtr< DivineVFS > divineVFS;
    llvm::IntrusiveRefCntPtr< clang::vfs::OverlayFileSystem > overlayFS;
    std::shared_ptr< llvm::LLVMContext > ctx;
};
} // namespace cc
} // namespace divine
