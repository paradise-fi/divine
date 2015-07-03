#include <lart/aa/pass.h>
#include <lart/abstract/pass.h>
#include <lart/interference/pass.h>
#include <lart/weakmem/pass.h>
#include <lart/support/composite.h>

#include <iostream>
#include <cassert>
#include <stdexcept>

#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/system_error.h>
#include <llvm/Support/raw_ostream.h>

#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/ADT/OwningPtr.h>

#include <llvm/IR/LLVMContext.h>
#include <llvm/PassManager.h>
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

ModulePass *mkPass( std::string n, std::string opt )
{
    if ( n == "aa" ) {
        aa::Pass::Type t;

        if ( opt == "andersen" )
            t = aa::Pass::Andersen;
        else
            throw std::runtime_error( "unknown alias-analysis type: " + opt );

        return new aa::Pass( t );
    }

    if ( n == "abstract" ) {
        abstract::Pass::Type t;

        if ( opt == "interval" )
            t = abstract::Pass::Interval;
        else if ( opt == "trivial" )
            t = abstract::Pass::Trivial;
        else
            throw std::runtime_error( "unknown abstraction type: " + opt );

        return new abstract::Pass( t );
    }

    if ( n == "interference" )
        return new interference::Pass();

    if ( n == "weakmem" ) {
        auto p = new CompositePass();
        weakmem::Substitute::Type t;

        if ( opt == "tso" )
            t = weakmem::Substitute::TSO;
        if ( opt == "pso" )
            t = weakmem::Substitute::PSO;

        p->append( new weakmem::ScalarMemory );
        p->append( new weakmem::Substitute( t ) );
        return p;
    }

    throw std::runtime_error( "unknown pass type: " + n );
}

void process( Module *m, PassOpts opt )
{
    PassManager manager;

    for ( auto i : opt )
        manager.add( mkPass( i.first, i.second ) );

    manager.run( *m );
}

int main( int argc, char **argv )
{
    if ( argc < 3 ) {
        std::cerr << "usage: lart in.bc out.bc [pass:options] ..." << std::endl
                  << std::endl
                  << "pass aa (alias analysis), options:" << std::endl
                  << "    andersen: andersen-style, flow- and context-insensitive" << std::endl
                  << std::endl
                  << "example: lart in.bc out.bc aa:andersen" << std::endl;
        return 1;
    }

    const char *from = argv[1], *to = argv[2];
    assert( from );
    assert( to );

    LLVMContext *ctx = new LLVMContext();

    OwningPtr< MemoryBuffer > input;
    MemoryBuffer::getFile( from, input );
    assert( input );

    Module *module;
    module = ParseBitcodeFile( &*input, *ctx );
    assert( module );

    std::string outerrs;
    raw_fd_ostream outs( to, outerrs );

    if ( !outerrs.empty() ) {
        std::cerr << "error writing output: " << outerrs << std::endl;
        return 2;
    }

    PassOpts passes = parse( argv + 3 );
    process( module, passes );

    WriteBitcodeToFile( module, outs );
}
