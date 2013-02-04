// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <unistd.h>

#include <wibble/commandline/parser.h>
#include <wibble/string.h>
#include <wibble/sys/fs.h>
#include <wibble/regexp.h>
#include <wibble/sys/pipe.h>
#include <wibble/sys/exec.h>
#include <wibble/sys/process.h>

#include "combine.h"
#include <divine/dve/compiler.h>
#include <divine/generator/cesmi.h>

#ifndef DIVINE_COMPILE_H
#define DIVINE_COMPILE_H


using namespace wibble;
using namespace commandline;
using namespace sys;

namespace divine {

extern const char *cesmi_usr_cesmi_h_str;
extern const char *cesmi_usr_cesmi_cpp_str;
extern const char *cesmi_usr_ltl_cpp_str;
extern const char *cesmi_usr_ltl_h_str;
extern const char *toolkit_pool_h_str;
extern const char *toolkit_blob_h_str;
extern const char *compile_defines_str;
extern const char *llvm_usr_h_str;
extern const char *llvm_usr_pthread_h_str;
extern const char *llvm_usr_pthread_cpp_str;
extern const char *llvm_usr_cstdlib_h_str;
extern const char *llvm_usr_cstdlib_cpp_str;

std::string ltl_to_c( int id, std::string ltl );

using namespace wibble;

struct Compile {
    commandline::Engine *cmd_compile;
    commandline::StandardParserWithMandatoryCommand &opts;

    BoolOption *o_cesmi, *o_llvm, *o_keep;
    StringOption *o_cflags;

    void die_help( std::string bla )
    {
        opts.outputHelp( std::cerr );
        die( bla );
    }

    static void die( std::string bla ) __attribute__((noreturn))
    {
        std::cerr << bla << std::endl;
        exit( 1 );
    }

    static void run( std::string command, void (*trap)(void*) = NULL, void *trap_arg = NULL ) {
        int status = system( command.c_str() );
#ifdef POSIX
        if ( status != -1 && WEXITSTATUS( status ) != 0 ) {
            if ( trap )
                trap( trap_arg );
            die( "Error running external command: " + command );
        }
#endif
    }

    static void runCompiler( std::string comp, std::string in, std::string out, std::string flags = "" ) {
        std::stringstream cmd;
        std::string multiarch =
#if defined(O_USE_GCC_M32)
            "-m32 "
#elif defined(O_USE_GCC_M64)
            "-m64 "
#else
            ""
#endif
            ;
        cmd << comp << " " << multiarch << flags << " -o " << out << " " << in;
        run( cmd.str() );
    }

    static void gplusplus( std::string in, std::string out, std::string flags = "" ) {
	runCompiler ("g++", in, out, flags + "-g -O2 -fPIC -shared");
    }

    void compileDve( std::string in ) {
#if defined O_DVE
        dve::compiler::DveCompiler compiler;
        compiler.read( in.c_str() );
        compiler.analyse();

        std::string outfile = str::basename( in ) + ".cpp";
        std::ofstream out( outfile.c_str() );
        compiler.setOutput( out );
        compiler.print_generator();

        gplusplus( outfile, str::basename( in ) + generator::cesmi_ext );
#else
        die( "FATAL: The DVE compiler requires DVE backend." );
#endif
    }

    void compileMurphi( std::string in );

    struct FilePath {
        // filepath = joinpath(abspath, basename)
        std::string basename;
        std::string abspath;
    };

    static void _cleanup_tmpdir( void* _tmp_dir ) {
        FilePath* tmp_dir = reinterpret_cast< FilePath* >( _tmp_dir );
        chdir( tmp_dir->abspath.c_str() );
        wibble::sys::fs::rmtree( tmp_dir->basename );
    }

    void propswitch( std::ostream &o, int c, std::string fun, std::string args ) {
        o << "    switch ( property ) {" << std::endl;
        for ( int i = 0; i < c; ++i )
            o << "        case " << i << ": return " << fun << "_" << i << args << ";" << std::endl;
        o <<     "        default: abort();" << std::endl;
        o << "    };" << std::endl;
    }

    void compileCESMI( std::string in, std::string cflags ) {
        FilePath tmp_dir;
        tmp_dir.abspath = process::getcwd();
        tmp_dir.basename = wibble::sys::fs::mkdtemp( "_divine-compile.XXXXXX" );
        std::string in_basename( str::basename( in ), 0, str::basename(in).rfind( '.' ) );

        void (*trap)(void*) = o_keep->boolValue() ? nullptr : _cleanup_tmpdir;
        void *trap_arg = reinterpret_cast< void* >( &tmp_dir );

        chdir( tmp_dir.basename.c_str() );
        fs::writeFile( "cesmi.h", cesmi_usr_cesmi_h_str );
        fs::writeFile( "cesmi.cpp", cesmi_usr_cesmi_cpp_str );
        chdir( tmp_dir.abspath.c_str() );

        std::string extras, ltlincludes;
        int ltlcount = 0;
        while ( opts.hasNext() ) {
            std::string extra = opts.next();
            if ( wibble::str::endsWith( extra, ".ltl" ) ) {
                std::string ltlpath = tmp_dir.basename + "/" + wibble::str::basename( extra ) + ".h";
                std::string code = "#include <cesmi.h>\n";
                parse_ltl( wibble::sys::fs::readFile( extra ), [&]( std::string formula ) {
                        code += ltl_to_c( ltlcount++, formula );
                    }, [&]( std::string ) {} );
                wibble::sys::fs::writeFile( ltlpath, code );
                ltlincludes += "#include <" + wibble::str::basename( extra ) + ".h>\n";
            } else
                extras += " " + extra;
        }

        extras += " " + tmp_dir.basename + "/cesmi.cpp";

        if ( ltlcount ) {
            std::string impl = tmp_dir.basename + "/cesmi-ltl",
                        aggr = tmp_dir.basename + "/ltl-aggregate.cpp";
            wibble::sys::fs::writeFile( impl + ".cpp", cesmi_usr_ltl_cpp_str );
            wibble::sys::fs::writeFile( impl + ".h", cesmi_usr_ltl_h_str );
            extras += " " + impl + ".cpp";
            std::ofstream aggr_s( aggr.c_str() );
            aggr_s << "#include <stdlib.h>" << std::endl;
            aggr_s << ltlincludes << std::endl;
            aggr_s << "extern \"C\" int buchi_accepting( int property, int id ) {\n";
            propswitch( aggr_s, ltlcount, "buchi_accepting", "( id )" );
            aggr_s << "}" << std::endl;
            aggr_s << "extern \"C\" int buchi_next( int property, int from, int transition, cesmi_setup *setup, cesmi_node n ) {\n";
            propswitch( aggr_s, ltlcount, "buchi_next", "( from, transition, setup, n )" );
            aggr_s << "}" << std::endl;
            aggr_s << "int buchi_property_count = " << ltlcount << ";" << std::endl;
            extras += " " + aggr;
        }

        std::string flags = "-Wall -shared -g -O2 -fPIC " + cflags;
        run( "gcc " + flags + " -I" + tmp_dir.basename + " -o " + in_basename +
             generator::cesmi_ext + " " + in + extras,
             trap, trap_arg );
        if ( trap ) trap( trap_arg );
    }

    void compileLLVM( std::string in, std::string cflags ) {       
        // create temporary directory to compile in
        char tmp_dir_template[] = "__tmpXXXXXX";
        FilePath tmp_dir;
        tmp_dir.abspath = process::getcwd();
        tmp_dir.basename = wibble::sys::fs::mkdtemp( tmp_dir_template );

        // input
        std::string in_basename ( str::basename(in), 0, str::basename(in).rfind( '.' ) );
          // user-space
        std::string usr_h = "usr.h";
          // pthread
        std::string pthread_h = "pthread.h";
        std::string pthread_cpp = "pthread.cpp";
        std::string pthread_bc = "pthread.bc";
          // cstdlib
        std::string cstdlib_h = "cstdlib"; // note: no extension
        std::string cstdlib_cpp = "cstdlib.cpp";
        std::string cstdlib_bc = "cstdlib.bc";

        // output
        std::string unlinked = str::joinpath( tmp_dir.basename, "_" + in_basename + ".bc" );
        std::string linked = in_basename + ".bc";

        // prepare cleanup
        void (*trap)(void*) = o_keep->boolValue() ? nullptr : _cleanup_tmpdir;
        void *trap_arg = reinterpret_cast< void* >( &tmp_dir );

        // enter tmp directory
        chdir( tmp_dir.basename.c_str() );

        // copy library files from memory to the directory
        fs::writeFile ( usr_h, llvm_usr_h_str );
        fs::writeFile ( pthread_h, llvm_usr_pthread_h_str );
        fs::writeFile ( pthread_cpp, llvm_usr_pthread_cpp_str );
        fs::writeFile ( cstdlib_h, llvm_usr_cstdlib_h_str );
        fs::writeFile ( cstdlib_cpp, llvm_usr_cstdlib_cpp_str );

        // compile
          // libraries
        std::string flags = "-c -emit-llvm -g " + cflags;
        run ( "clang " + flags + " -I. " + cstdlib_cpp + " -o " + cstdlib_bc, trap, trap_arg );
        run ( "clang " + flags + " -I. " + pthread_cpp + " -o " + pthread_bc, trap, trap_arg );
          // leave tmp directory
        chdir( tmp_dir.abspath.c_str() );
          // input file
        run ( "clang " + flags + " -I'" + tmp_dir.basename + "' " + in + " -o " + unlinked, trap, trap_arg );

        // link
        run ( "llvm-link " + unlinked + " " + str::joinpath( tmp_dir.basename, cstdlib_bc ) + " " +
              str::joinpath( tmp_dir.basename, pthread_bc ) + " -o " + linked, trap, trap_arg );

        // cleanup
        if (trap) trap( trap_arg );
    }

    void main() {
        std::string input = opts.next();
        if ( access( input.c_str(), R_OK ) )
            die( "FATAL: cannot open input file " + input + " for reading" );
        if ( str::endsWith( input, ".dve" ) )
            compileDve( input );
        else if ( str::endsWith( input, ".m" ) )
            compileMurphi( input );
        else if ( ( str::endsWith( input, ".c" ) || str::endsWith( input, ".cpp" ) ||
                    str::endsWith( input, ".cc" ) || str::endsWith( input, ".C" )) )
            if ( o_cesmi->boolValue() )
                compileCESMI( input, o_cflags->stringValue() );
            else if ( o_llvm->boolValue() )
                compileLLVM( input, o_cflags->stringValue() );
            else {
                std::cerr << "Do not know whether to process input file as of CESMI type ";
                std::cerr << "(use --cesmi or -c) or generate output file in LLVM bytecode format ";
                std::cerr << "(use --llvm or -l)." << std::endl;
            }
        else {
            std::cerr << "Do not know how to compile this file type." << std::endl;
        }
    }

    Compile( commandline::StandardParserWithMandatoryCommand &_opts )
        : opts( _opts)
    {
        cmd_compile = _opts.addEngine( "compile",
                                       "<input>",
                                       "Compile input models into DiVinE binary interface.");

        o_cesmi = cmd_compile->add< BoolOption >(
            "cesmi", 'c', "cesmi", "",
            "process input .cpp or .cc files as of CESMI type" );

        o_llvm = cmd_compile->add< BoolOption >(
            "llvm", 'l', "llvm", "",
            "generate output file in LLVM bytecode format" );

        o_keep = cmd_compile->add< BoolOption >(
            "keep-build-directory", '\0', "keep-build-directory", "",
            "do not erase intermediate files after a compile" );

        o_cflags = cmd_compile->add< StringOption >(
            "cflags", 'f', "cflags", "",
            "set flags for C/C++ compiler" );

    }

};

}

#endif
