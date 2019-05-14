// -*- C++ -*- (c) 2016-2017 Vladimír Štill
#pragma once

DIVINE_RELAX_WARNINGS
#include <clang/Driver/Compilation.h>
#include <clang/Driver/Driver.h>
DIVINE_UNRELAX_WARNINGS
#include "llvm/ADT/ArrayRef.h"
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>

#include <divine/cc/cc1.hpp>
#include <divine/cc/options.hpp>
#include <divine/rt/paths.hpp>

#include <brick-assert>
#include <brick-llvm-link>
#include <brick-fs>
#include <thread>
#include <stdexcept>

namespace brick { namespace llvm { struct Linker; struct ArchiveReader; } } // avoid header dependency

namespace divine {
namespace cc {

// get generated source which defines symbol with name 'name' in namespaces 'ns'
// which contains char array literal 'value'
std::string stringifyToCode( std::vector< std::string > ns, std::string name, std::string value );

struct Command
{
    Command( std::string name )
        : name( name ){}

    void addArg( std::string a )
    {
        args.push_back( a );
    }
    std::string name;
    std::vector< std::string > args;
};

struct DiagnosticsWrapper
{
    Diagnostics diag;
};

struct ClangDriver : DiagnosticsWrapper, clang::driver::Driver
{
    ClangDriver()
      : clang::driver::Driver( "divcc", LLVM_HOST_TRIPLE, diag.engine )
    {}
};

struct Driver
{
    using ModulePtr = std::unique_ptr< llvm::Module >;

    explicit Driver( std::shared_ptr< llvm::LLVMContext > ctx ) : Driver( Options(), ctx ) {}
    explicit Driver( Options opts = Options(),
                      std::shared_ptr< llvm::LLVMContext > ctx = nullptr );
    Driver( Driver&& ) = default;
    Driver& operator=( Driver&& ) = default;
    virtual ~Driver();

    void linkLibs( std::vector< std::string > libs, std::vector< std::string > searchPaths = {} );
    void linkLib( std::string lib, std::vector< std::string > searchPaths = {} );
    void link( ModulePtr mod );
    void linkArchive( std::unique_ptr< llvm::MemoryBuffer > buf, std::shared_ptr< llvm::LLVMContext > context );
    void linkEntireArchive( std::string arch );

    ModulePtr compile( std::string path, std::vector< std::string > flags = {} );
    ModulePtr compile( std::string path, FileType type, std::vector< std::string > flags = {} );

    virtual void build( ParsedOpts po );

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

    void setupFS( const std::vector< std::pair< std::string, std::string > > &files )
    {
        for( auto [f,c] : files )
            compiler.mapVirtualFile( f, c );
    }

    std::vector< Command > getJobs( llvm::ArrayRef< const char * > args );
    brick::llvm::ArchiveReader getLib( std::string lib, std::vector< std::string > searchPaths = {} );
    ModulePtr load_object( std::string name );

  protected:
    Options opts;
    CC1 compiler;
    std::unique_ptr< brick::llvm::Linker > linker;
  public:
    std::vector< std::string > commonFlags;
};

}
}
