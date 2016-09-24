// -*- C++ -*- (c) 2015 Vladimír Štill <xstill@fi.muni.cz>

#include <lart/interference/pass.h>
#include <lart/aa/pass.h>
#include <lart/abstract/pass.h>
#include <lart/abstract/passes.h>
#include <lart/weakmem/pass.h>
#include <lart/reduction/passes.h>
#include <lart/svcomp/passes.h>
#include <lart/divine/passes.h>

#include <iostream>

DIVINE_RELAX_WARNINGS
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>

#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/raw_os_ostream.h>

#include <llvm/IR/Verifier.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
DIVINE_UNRELAX_WARNINGS

namespace lart {

struct Driver {

    std::vector< PassMeta > passes() const {
        std::vector< PassMeta > out;
        out.push_back( weakmem::meta() );
        insertPasses( out, reduction::passes() );
        insertPasses( out, svcomp::passes() );
        insertPasses( out, divine::passes() );
        insertPasses( out, abstract::passes() );
        return out;
    }

    static void assertValid( llvm::Module *module ) {
        llvm::raw_os_ostream cerr( std::cerr );
        if ( llvm::verifyModule( *module, &cerr ) ) {
            cerr.flush(); // otherwise nothing will be displayed
            UNREACHABLE( "invalid bitcode" );
        }
    }

    bool setup( std::string p, bool verbose = true ) {
        std::string name( p, 0, p.find( ':' ) ), opt;

        if ( p.find( ':' ) != std::string::npos )
            opt = std::string( p, p.find( ':' ) + 1, std::string::npos );

        if ( verbose )
            std::cerr << "setting up pass: " << name << ", options = " << opt << std::endl;
        return addPass( name, opt );
    }

    void setup( PassMeta pass, std::string opt = "" ) {
        pass.create( manager, opt );
    }

    bool addPass( std::string n, std::string opt )
    {
        for ( auto pass : passes() ) {
            if ( pass.select( manager, n, opt ) )
                return true;
        }

        if ( n == "aa" ) {
            aa::Pass::Type t;

            if ( opt == "andersen" )
                t = aa::Pass::Andersen;
            else
                throw std::runtime_error( "unknown alias-analysis type: " + opt );

            manager.addPass( aa::Pass( t ) );
            return true;
        }

        if ( n == "abstract" ) {
            abstract::Pass::Type t;

            if ( opt == "interval" )
                t = abstract::Pass::Interval;
            else if ( opt == "trivial" )
                t = abstract::Pass::Trivial;
            else
                throw std::runtime_error( "unknown abstraction type: " + opt );

            manager.addPass( abstract::Pass( t ) );
            return true;
        }

        if ( n == "interference" ) {
            manager.addPass( interference::Pass() );
            return true;
        }

        return false;
    }

    void process( llvm::Module *m )
    {
        manager.run( *m );
    }

  private:
    static void insertPasses( std::vector< PassMeta > &out, std::vector< PassMeta > &&toadd ) {
        std::copy( toadd.begin(), toadd.end(), std::back_inserter( out ) );
    }

    llvm::ModulePassManager manager;
};
} // namespace lart

