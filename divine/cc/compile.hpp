// -*- C++ -*- (c) 2016-2017 Vladimír Štill
#pragma once

#include <divine/cc/clang.hpp>
#include <divine/cc/options.hpp>

#include <brick-assert>
#include <brick-string>
#include <brick-types>
#include <thread>
#include <stdexcept>

namespace brick { namespace llvm { struct Linker; struct ArchiveReader; } } // avoid header dependency

namespace divine {
namespace cc {

// get generated source which defines symbol with name 'name' in namespaces 'ns'
// which contains char array literal 'value'
std::string stringifyToCode( std::vector< std::string > ns, std::string name, std::string value );

struct Lib {
    using InPlace = brick::types::InPlace< Lib >;

    Lib() = default;
    explicit Lib( std::string name ) : name( std::move( name ) ) { }
    std::string name;
};

struct File {
    using InPlace = brick::types::InPlace< File >;

    File() = default;
    explicit File( std::string name, Compiler::FileType type = Compiler::FileType::Unknown ) :
        name( std::move( name ) ), type( type )
    { }

    std::string name;
    Compiler::FileType type = Compiler::FileType::Unknown;
};

using FileEntry = brick::types::Union< File, Lib >;

struct ParsedOpts {
    std::vector< std::string > opts;
    std::vector< std::string > libSearchPath;
    std::vector< FileEntry > files;
    std::string outputFile;
    std::vector< std::string > allowedPaths;
    bool toObjectOnly = false;
    bool preprocessOnly = false;
};

ParsedOpts parseOpts( std::vector< std::string > rawCCOpts );

struct Compile
{
    using ModulePtr = std::unique_ptr< llvm::Module >;

    explicit Compile( std::shared_ptr< llvm::LLVMContext > ctx ) : Compile( Options(), ctx ) {}
    explicit Compile( Options opts = Options(),
                      std::shared_ptr< llvm::LLVMContext > ctx = nullptr );
    ~Compile();

    void compileAndLink( std::string path, std::vector< std::string > flags = {} );
    void compileAndLink( std::string path, Compiler::FileType type, std::vector< std::string > flags = {} );

    void linkLibs( std::vector< std::string > libs, std::vector< std::string > searchPaths = {} );
    void linkLib( std::string lib, std::vector< std::string > searchPaths = {} );
    void linkModule( ModulePtr mod );
    void linkArchive( std::unique_ptr< llvm::MemoryBuffer > buf, std::shared_ptr< llvm::LLVMContext > context );
    void linkEntireArchive( std::string arch );
    void linkEssentials();

    static const std::vector< std::string > defaultDIVINELibs;

    ModulePtr compile( std::string path, std::vector< std::string > flags = { } );
    ModulePtr compile( std::string path, Compiler::FileType type, std::vector< std::string > flags = {} );

    // Run the compiler based on invocation arguments (including files). The
    // arguments should not contain -o or linker arguments.
    //
    // The callback (if present) is invoked for every compiled module, with
    // pointer to the module, and the name of the file from which the module
    // was created.
    //
    // If the callback returns a module, this module is linked to the
    // composite, which can be later obtained using getLinked(). Otherwise, the
    // module is skipped and the callback can transfer its ownership, or delete
    // it. Normally, the implementation would handle non-liking modules and
    // return without modification modules which should be linked.
    void runCC( std::vector< std::string > rawCCOpts,
                std::function< ModulePtr( ModulePtr &&, std::string ) > moduleCallback = nullptr );

    llvm::Module *getLinked();
    void writeToFile( std::string filename );
    void writeToFile( std::string filename, llvm::Module *module );
    std::string serialize();

    void addDirectory( std::string path );
    void addFlags( std::vector< std::string > flags );

    std::shared_ptr< llvm::LLVMContext > context();

    template< typename RT >
    void setupFS( RT each )
    {
        each( [&]( auto path, auto c ) { compiler.mapVirtualFile( path, c ); } );
    }

  private:
    brick::llvm::ArchiveReader getLib( std::string lib, std::vector< std::string > searchPaths = {} );

    Options opts;
    Compiler compiler;
    std::unique_ptr< brick::llvm::Linker > linker;
    std::vector< std::string > commonFlags; // set in CPP
    std::string runtimeVersMeta = "divine.compile.runtime.version";
};

}
}
