// -*- C++ -*- (c) 2016 Vladimír Štill
#pragma once

#include <divine/cc/clang.hpp>
#include <divine/cc/options.hpp>

#include <brick-assert>
#include <brick-string>
#include <thread>
#include <stdexcept>

namespace brick { namespace llvm { struct Linker; } } // avoid header dependency

namespace divine {
namespace cc {

// get generate source which defines symbol with name 'name' in namespaces 'ns'
// which contains char array literal 'value'
std::string stringifyToCode( std::vector< std::string > ns, std::string name, std::string value );

struct Compile
{
    using ModulePtr = std::unique_ptr< llvm::Module >;

    explicit Compile( Options opts = Options() );
    ~Compile();

    void compileAndLink( std::string path, std::vector< std::string > flags = {} );
    void compileAndLink( std::string path, Compiler::FileType type, std::vector< std::string > flags = {} );

    ModulePtr compile( std::string path, std::vector< std::string > flags = { } );
    ModulePtr compile( std::string path, Compiler::FileType type, std::vector< std::string > flags = {} );

    // Run the compiler based on invocation arguments (including files). The
    // arguments should not contain -o or linker arguments.
    //
    // The callback (if present) is invoked for every compiled module, with
    // pointer to the module, the name of the file from which the module was
    // created and a boolean flag set to true if an option which prevents
    // linking (such as -c or -S) was specified among rawCCOpts.
    //
    // If the callback returns a module, this module is linked to the
    // composite, which can be later obtained using getLinked(). Otherwise, the
    // module is skipped and the callback can transfer its ownership, or delete
    // it. Normally, the implementation would handle non-liking modules and
    // return without modification modules which should be linked.
    void runCC( std::vector< std::string > rawCCOpts,
                std::function< ModulePtr( ModulePtr &&, std::string, bool ) > moduleCallback = nullptr );

    llvm::Module *getLinked();
    void writeToFile( std::string filename );
    void writeToFile( std::string filename, llvm::Module *module );
    std::string serialize();

    void addDirectory( std::string path );
    void addFlags( std::vector< std::string > flags );

    void prune( std::vector< std::string > r );

    std::shared_ptr< llvm::LLVMContext > context();

    template< typename RT >
    void setupFS( RT each )
    {
        each( [&]( auto path, auto c )
              {
                  if ( brick::string::endsWith( path, ".bc" ) )
                      setupLib( path, c );
                  else
                      mastercc().mapVirtualFile( path, c );
              } );
    }

  private:
    Compiler &mastercc();

    void setupLib( std::string name, const std::string &content );
    void compileLibrary( std::string path, std::vector< std::string > flags = { } );

    Options opts;
    std::vector< Compiler > compilers;
    std::vector< std::thread > workers;
    std::unique_ptr< brick::llvm::Linker > linker;
    std::vector< std::string > commonFlags; // set in CPP
    std::string runtimeVersMeta = "divine.compile.runtime.version";
};

}
}
