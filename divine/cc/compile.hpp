// -*- C++ -*- (c) 2016 Vladimír Štill
#pragma once

#include <divine/cc/clang.hpp>
#include <divine/cc/options.hpp>

#include <brick-assert>
#include <brick-string>
#include <thread>

namespace brick { namespace llvm { struct Linker; } } // avoid header dependency

namespace divine {
namespace cc {

// get generate source which defines symbol with name 'name' in namespaces 'ns'
// which contains char array literal 'value'
std::string stringifyToCode( std::vector< std::string > ns, std::string name, std::string value );

struct Compile
{
    explicit Compile( Options opts = Options() );
    ~Compile();

    void compileAndLink( std::string path, std::vector< std::string > flags = {} );
    std::unique_ptr< llvm::Module > compile( std::string path,
                        std::vector< std::string > flags = { } );

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
