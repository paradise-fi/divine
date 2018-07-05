// -*- C++ -*- (c) 2016-2017 Vladimír Štill
#pragma once

#include <divine/cc/cc1.hpp>
#include <divine/cc/options.hpp>

#include <brick-assert>
#include <thread>
#include <stdexcept>

namespace brick { namespace llvm { struct Linker; struct ArchiveReader; } } // avoid header dependency

namespace divine {
namespace cc {

// get generated source which defines symbol with name 'name' in namespaces 'ns'
// which contains char array literal 'value'
std::string stringifyToCode( std::vector< std::string > ns, std::string name, std::string value );

struct Driver
{
    using ModulePtr = std::unique_ptr< llvm::Module >;

    explicit Driver( std::shared_ptr< llvm::LLVMContext > ctx ) : Driver( Options(), ctx ) {}
    explicit Driver( Options opts = Options(),
                      std::shared_ptr< llvm::LLVMContext > ctx = nullptr );
    ~Driver();

    void linkLibs( std::vector< std::string > libs, std::vector< std::string > searchPaths = {} );
    void linkLib( std::string lib, std::vector< std::string > searchPaths = {} );
    void link( ModulePtr mod );
    void linkArchive( std::unique_ptr< llvm::MemoryBuffer > buf, std::shared_ptr< llvm::LLVMContext > context );
    void linkEntireArchive( std::string arch );
    void linkEssentials();

    static const std::vector< std::string > defaultDIVINELibs;

    ModulePtr compile( std::string path, std::vector< std::string > flags = { } );
    ModulePtr compile( std::string path, FileType type, std::vector< std::string > flags = {} );

    // Run the compiler based on invocation arguments (including files). The
    // arguments should not contain -o or linker arguments.
    //
    // The callback (if present) is invoked for every compiled module, with
    // pointer to the module, and the name of the file from which the module
    // was created.
    //
    // If the callback returns a module, this module is linked to the
    // composite, which can be later obtained using takeLinked(). Otherwise, the
    // module is skipped and the callback can transfer its ownership, or delete
    // it. Normally, the implementation would handle non-liking modules and
    // return without modification modules which should be linked.
    void runCC( std::vector< std::string > rawCCOpts,
                std::function< ModulePtr( ModulePtr &&, std::string ) > moduleCallback = nullptr );

    std::unique_ptr< llvm::Module > takeLinked();
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
    CC1 compiler;
    std::unique_ptr< brick::llvm::Linker > linker;
    std::vector< std::string > commonFlags; // set in CPP
};

}
}
