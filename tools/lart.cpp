#if LLVM_MAJOR >= 3 && LLVM_MINOR >= 7
#include <lart/interference/pass.h>
#include <lart/aa/pass.h>
#include <lart/abstract/pass.h>
#endif
#include <lart/weakmem/pass.h>
#include <lart/reduction/passes.h>
#include <lart/svcomp/passes.h>
#include <lart/support/composite.h>

#include <iostream>
#include <cassert>
#include <stdexcept>

#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>

#include <llvm/Bitcode/ReaderWriter.h>

#include <llvm/IR/Verifier.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Pass.h>
#include <llvm/IR/Module.h>

using namespace llvm;
using namespace lart;

typedef std::vector< std::pair< std::string, std::string > > PassOpts;

PassOpts parse( char **pass )
{
    PassOpts r;
    while ( *pass ) {
        std::string p( *pass ), name( p, 0, p.find( ':' ) ), opt;

        if ( p.find( ':' ) != std::string::npos )
            opt = std::string( p, p.find( ':' ) + 1, std::string::npos );

        std::cerr << "setting up pass: " << name << ", options = " << opt << std::endl;
        r.push_back( std::make_pair( name, opt ) );
        ++ pass;
    }
    return r;
}

void insertPasses( std::vector< PassMeta > &out, std::vector< PassMeta > &&toadd ) {
    std::copy( toadd.begin(), toadd.end(), std::back_inserter( out ) );
}

std::vector< PassMeta > passes() {
    std::vector< PassMeta > out;
    out.push_back( weakmem::meta() );
    insertPasses( out, reduction::passes() );
    insertPasses( out, svcomp::passes() );
    return out;
}

void addPass( ModulePassManager &mgr, std::string n, std::string opt )
{
    for ( auto pass : passes() ) {
        if ( pass.select( mgr, n, opt ) )
            return;
    }

#if LLVM_MAJOR >= 3 && LLVM_MINOR >= 7
    if ( n == "aa" ) {
        aa::Pass::Type t;

        if ( opt == "andersen" )
            t = aa::Pass::Andersen;
        else
            throw std::runtime_error( "unknown alias-analysis type: " + opt );

        mgr.addPass( aa::Pass( t ) );
    }

    if ( n == "abstract" ) {
        abstract::Pass::Type t;

        if ( opt == "interval" )
            t = abstract::Pass::Interval;
        else if ( opt == "trivial" )
            t = abstract::Pass::Trivial;
        else
            throw std::runtime_error( "unknown abstraction type: " + opt );

        mgr.addPass( abstract::Pass( t ) );
    }

    if ( n == "interference" )
        mgr.addPass( interference::Pass() );
#else
#warning "LART features disabled: aa, abstract, interference (requires LLVM 3.7)"
#endif

}

void process( Module *m, PassOpts opt )
{
    ModulePassManager manager;

    for ( auto i : opt )
        addPass( manager, i.first, i.second );

#if LLVM_MAJOR == 3 && LLVM_MINOR <= 5
    manager.run( m );
#else
    manager.run( *m );
#endif
}

int main( int argc, char **argv )
{
    if ( argc < 3 ) {
        std::cerr << "usage: lart in.bc out.bc [pass:options] ..." << std::endl
                  << std::endl
                  << "pass aa (alias analysis), options:" << std::endl
                  << "    andersen: andersen-style, flow- and context-insensitive" << std::endl
                  << std::endl
                  << "example: lart in.bc out.bc aa:andersen" << std::endl << std::endl;
        for ( auto pass : passes() ) {
            pass.description( std::cerr );
            std::cerr << std::endl;
        }
        return 1;
    }

    const char *from = argv[1], *to = argv[2];
    assert( from );
    assert( to );

    LLVMContext *ctx = new LLVMContext();

    std::unique_ptr< MemoryBuffer > input;
    input = std::move( MemoryBuffer::getFile( from ).get() );
    assert( input );

    Module *module;
#if LLVM_MAJOR == 3 && LLVM_MINOR <= 5
    auto inputData = input.get();
#else
    auto inputData = input->getMemBufferRef();
#endif
    auto parsed = parseBitcodeFile( inputData, *ctx );
    if ( !parsed )
        throw std::runtime_error( "Error parsing input model; probably not a valid bitcode file." );
#if LLVM_MAJOR == 3 && LLVM_MINOR < 7
    module = parsed.get();
#else
    module = parsed.get().get();
#endif

#if LLVM_MAJOR == 3 && LLVM_MINOR <= 5
    std::string serr;
#else
    std::error_code serr;
#endif
    ::llvm::raw_fd_ostream outs( to, serr, ::llvm::sys::fs::F_None );

    PassOpts passes = parse( argv + 3 );
    process( module, passes );
    llvm::verifyModule( *module );

    WriteBitcodeToFile( module, outs );
}
