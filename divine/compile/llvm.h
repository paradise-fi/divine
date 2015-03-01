// -*- C++ -*- (c) 2014 Vladimír Štill

#if GEN_LLVM || GEN_LLVM_CSDR || GEN_LLVM_PROB || GEN_LLVM_PTST
#include <initializer_list>

#include <brick-llvm.h>
#include <brick-fs.h>
#include <divine/utility/strings.h>
#include <divine/utility/die.h>

#include <divine/compile/env.h>

#ifndef TOOLS_COMPILE_LLVM_H
#define TOOLS_COMPILE_LLVM_H

namespace divine {

extern stringtable pdclib_list[];
extern stringtable libm_list[];
extern stringtable libunwind_list[];
extern stringtable libcxxabi_list[];
extern stringtable libcxx_list[];

namespace compile {

static constexpr auto precompiledRuntime = "libdivinert.bc";

struct CompileLLVM {

    CompileLLVM( Env &env ) : _env( env ) { }

    void compile() try {
        if ( !_env.usePrecompiled.empty() ) {
            auto pre = ::llvm::ParseIRFile(
                        brick::fs::joinPath( _env.usePrecompiled, precompiledRuntime ),
                        err, ctx );
            if ( !pre )
                die( std::string( "Could not load precompiled library: " ) + err.getMessage().data() );
            linker.load( pre );
        }

        // create temporary directory to compile in
        brick::fs::TempDir tmpdir( "_divine-compile.XXXXXX",
                                   brick::fs::AutoDelete( !_env.keepBuildDir ) );
        brick::fs::ChangeCwd rootDir( tmpdir.path );

        // copy content of library files from memory to the directory
        prepareIncludes( llvm_h_list );
        brick::fs::mkFilePath( "bits/pthreadtypes.h" );
        brick::fs::writeFile( "bits/pthreadtypes.h" , "#include <pthread.h>" );
        brick::fs::writeFile( "assert.h", "#include <divine.h>\n" ); /* override PDClib's assert.h */
        brick::fs::mkFilePath( "divine/problem.def" );
        brick::fs::writeFile( "divine/problem.def", src_llvm::llvm_problem_def_str );
        if ( !_env.dontLink )
            prepareIncludes( llvm_list );

        // compile libraries
        std::string flags = "-D__divine__ -D_POSIX_C_SOURCE=2008098L -D_LITTLE_ENDIAN=1234 -D_BYTE_ORDER=1234 "
                            "-emit-llvm -nobuiltininc -nostdlibinc -nostdinc -Xclang -nostdsysteminc -nostdinc++ -g ";
        compileLibrary( "libpdc", pdclib_list, flags + " -D_PDCLIB_BUILD -I.." );
        compileLibrary( "libm", libm_list, flags + " -I../libpdc -I." );

        prepareIncludes( "libcxx", libcxx_list ); // cyclic dependency cxxabi <-> cxx
        compileLibrary( "libcxxabi", libcxxabi_list, flags + " -I../libpdc -I../libm "
                        "-I.. -Iinclude -I../libcxx/std -I../libcxx -std=c++11 -fstrict-aliasing" );
        compileLibrary( "libcxx", libcxx_list, flags + " -I../libpdc -I../libm "
                        "-I../libcxxabi/include -I.. -Istd -I. -fstrict-aliasing -std=c++11 -fstrict-aliasing" );

        flags += " -Ilibcxxabi/include -Ilibpdc -Ilibcxx/std -Ilibcxx -Ilibm ";

        if ( _env.usePrecompiled.empty() && !_env.dontLink ) {
            auto files = { "glue", "stubs", "entry", "pthread", "cxa_exception_divine" };

            for ( std::string f : files ) {
                _env.compileFile( f + ".cpp", " -c -I. -Ilibcxxabi " + flags + " -o " + f + ".bc" );
                linkFile( f + ".bc" );
            }
        }

        if ( _env.librariesOnly ) {
            brick::llvm::writeModule( linker.get(), brick::fs::joinPath( rootDir.oldcwd, precompiledRuntime ) );
            return;
        }

        flags += _env.flags;

        if ( _env.input.size() >= 2 && _env.dontLink && !_env.output.empty() )
            die( "Cannot specify both -o and --dont-link with multiple files." );

        for ( auto file : _env.input ) {

            auto compilename = brick::fs::replaceExtension(
                    brick::fs::splitFileName( file ).second, ".bc" );

            if ( _env.output.empty() && !_env.dontLink ) {
                // If -o and --dont-link are not specified, then choose the name of the first file in the list
                // for the target name.
                _env.output = compilename;
            }

            if ( brick::fs::takeExtension( file ) != ".bc" ) {
                _env.compileFile( brick::fs::joinPath( "..", file ),
                                  " -c -I. " + flags + " -o " + compilename );

                if ( _env.dontLink ) {
                    brick::fs::renameIfExists( compilename, _env.output.empty()
                            ? ( brick::fs::joinPath( rootDir.oldcwd, compilename ) )
                            : ( brick::fs::joinPath( rootDir.oldcwd, _env.output ) ) );
                } else
                    linkFile( compilename );
            } else if ( !_env.dontLink )
                linkFile( file );
        }

        if ( !_env.dontLink ) {
            // we must enter list of root, symbols not accessible from these will be pruned
            // note: mem* functions must be here since clang may emit llvm instrinsic
            // instead of them and this intrinsic needs to be later lowered to symbol
            linker.prune( { "_divine_start", "main", "memmove", "memset", "memcpy", "llvm.global_ctors" },
                          brick::llvm::Prune::UnusedModules ); // AllUnused );
            brick::llvm::writeModule( linker.get(), brick::fs::joinPath( rootDir.oldcwd, _env.output ) );
        }

    } catch ( brick::fs::SystemException &ex ) {
        die( std::string( "FATAL: " ) + ex.what() );
    } catch ( brick::fs::Exception &ex ) {
        die( std::string( "FATAL: " ) + ex.what() );
    }

    template< typename Src >
    void prepareIncludes( std::string name, Src src ) {
        brick::fs::mkdirIfMissing( name, 0755 );
        brick::fs::ChangeCwd _( name );
        prepareIncludes( src );
    }

    template< typename Src >
    void prepareIncludes( Src src ) {
        while ( src->n ) {
            brick::fs::mkFilePath( src->n );
            brick::fs::writeFile( src->n, src->c );
            ++src;
        }
    }

    template< typename Src >
    void compileLibrary( std::string name, Src src, std::string flags ) {
        std::vector< std::string > files;
        auto src_ = src;
        prepareIncludes( name, src );

        brick::fs::ChangeCwd _( name );
        if ( !_env.usePrecompiled.empty() || _env.dontLink )
            return; /* we only need the headers from above */

        src = src_;

        auto queue = _env.getQueue();
        while ( src->n ) {
            if ( brick::string::endsWith( src->n, ".cc" ) ||
                 brick::string::endsWith( src->n, ".c" ) ||
                 brick::string::endsWith( src->n, ".cpp" ) ||
                 brick::string::endsWith( src->n, ".cxx" ) ) {
                queue.pushCompilation( src->n, " -c " + flags + " -I. -o " + src->n + ".bc" );
                files.emplace_back( std::string( src->n ) + ".bc" );
            }
            ++src;
        }

        queue.run();

        for ( auto f : files )
            linkFile( f );
    }

    void linkFile( std::string file ) {
        std::cout << "linking " << file << std::endl;
        auto mod = ::llvm::ParseIRFile( file, err, ctx );
        if ( !mod )
            die( std::string( "Linker error: " ) + err.getMessage().data() );
        linker.link( mod );
    }

  private:
    Env &_env;
    ::llvm::LLVMContext ctx;
    ::llvm::SMDiagnostic err;
    brick::llvm::Linker linker;
};

}
}

#endif // TOOLS_COMPILE_LLVM_H
#endif // llvm
