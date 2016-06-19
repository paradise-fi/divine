// -*- C++ -*- (c) 2016 Vladimír Štill
#pragma once

#include <divine/cc/clang.hpp>
#include <divine/cc/options.hpp>

#include <brick-assert>
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

    void tagWithRuntimeVersionSha( llvm::Module &m ) const;
    std::string getRuntimeVersionSha( llvm::Module &m ) const;

    std::shared_ptr< llvm::LLVMContext > context();

  private:
    Compiler &mastercc();

    enum class Type { Header, Source, All };
    template< typename Src >
    void prepareSources( std::string basedir, Src src, Type type = Type::All,
                         std::function< bool( std::string ) > filter = nullptr );

    void setupFS();
    void setupLibs();
    void compileLibrary( std::string path, std::initializer_list< std::string > flags = { } );

    const std::string includeDir = "/divine/include";
    const std::string srcDir = "/divine/src";

    Options opts;
    std::vector< Compiler > compilers;
    std::vector< std::thread > workers;
    std::unique_ptr< brick::llvm::Linker > linker;
    std::vector< std::string > commonFlags; // set in CPP
    std::string runtimeVersMeta = "divine.compile.runtime.version";
};

}
}
