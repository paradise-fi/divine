// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <wibble/commandline/parser.h>
#include <wibble/string.h>
#include <wibble/sys/fs.h>
#include <wibble/regexp.h>
#include <wibble/sys/pipe.h>
#include <wibble/sys/exec.h>
#include "dvecompile.h"
#include <unistd.h>

#ifndef DIVINE_COMPILE_H
#define DIVINE_COMPILE_H


using namespace wibble;
using namespace commandline;
using namespace sys;

namespace divine {

extern const char *generator_cesmi_client_h_str;
extern const char *toolkit_pool_h_str;
extern const char *toolkit_blob_h_str;
extern const char *compile_defines_str;
extern const char *llvm_usr_pthread_h_str;
extern const char *llvm_usr_pthread_cpp_str;
extern const char *llvm_usr_h_str;

using namespace wibble;

struct Compile {
    commandline::Engine *cmd_compile;
    commandline::StandardParserWithMandatoryCommand &opts;

    BoolOption *o_cesmi, *o_llvm;
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

    static void run( std::string command ) {
        int status = system( command.c_str() );
#ifdef POSIX
        if ( status != -1 && WEXITSTATUS( status ) != 0 )
            die( "Error running external command: " + command );
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
#ifdef O_LEGACY
        dve_compiler compiler;
        compiler.read( in.c_str() );
        compiler.analyse();

        std::string outfile = str::basename( in ) + ".cpp";
        std::ofstream out( outfile.c_str() );
        compiler.setOutput( out );
        compiler.print_generator();

        gplusplus( outfile, str::basename( in ) + ".so" );
#else
        die( "FATAL: The DVE compiler requires LEGACY code." );
#endif
    }

    void compileMurphi( std::string in );

    void compileCESMI( std::string in ) {
	std::string name( str::basename(in), 0, str::basename(in).rfind( '.' ) );
	std::string cesmi_aux = name + "_cesmi_aux";
	std::string comp;
	if ( str::endsWith( in , ".c") ) {
	    comp = "gcc";
	    cesmi_aux = cesmi_aux + ".c";
	}
	else {
	    comp = "g++";
	    cesmi_aux = cesmi_aux + ".cpp";
	}

	// CESMI interface with system_ prefix     model.c -> model.o, model.so
        runCompiler( comp , str::basename( in ), name + ".o" , "-g -c -O2 -fPIC");

	//  system_ model.c -> model.noprop.so
	run ( "sed 's/_system_/_/' " + str::basename( in ) + "| sed 's/system_setup/setup/' >" + cesmi_aux );
	runCompiler( comp , cesmi_aux, name + ".so", "-g -O2 -fPIC -shared" );
	run ( "rm -f " + cesmi_aux );
    }

    void compileLLVM( std::string in, std::string cflags ) {
        std::string name( str::basename(in), 0, str::basename(in).rfind( '.' ) );
        char tmp_dir_template[] = "__tmpXXXXXX";
        std::string tmp_dir = mkdtemp(tmp_dir_template);
        std::string usr_h = tmp_dir + "/usr.h";
        std::string pthread_h = tmp_dir + "/pthread.h";
        std::string pthread_cpp = tmp_dir + "/pthread.cpp";
        std::string pthread_bc = tmp_dir + "/pthread.bc";
        std::string unlinked = tmp_dir + "/_" + name + ".bc";
        std::string linked = name + ".bc";

        fs::writeFile(usr_h, llvm_usr_h_str);
        fs::writeFile(pthread_h, llvm_usr_pthread_h_str);
        fs::writeFile(pthread_cpp, llvm_usr_pthread_cpp_str);

        std::string flags = "-c -emit-llvm -g -I'" + tmp_dir + "' " + cflags;
        runCompiler("clang", pthread_cpp, pthread_bc, flags);
        runCompiler("clang", in, unlinked, flags);
        run("llvm-link " + unlinked + " " + pthread_bc + " -o " + linked);

        run ( "rm -rf " + tmp_dir );
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
            if (o_cesmi->boolValue())
                compileCESMI( input );
            else if (o_llvm->boolValue())
                compileLLVM( input, o_cflags->stringValue() );
            else {
                std::cerr << "Do not know wheter to process input file as of CESMI type ";
                std::cerr <<  "(use --cesmi or -c) or generate output file in LLVM bytecode format ";
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

        o_cflags = cmd_compile->add< StringOption >(
            "cflags", 'f', "cflags", "",
            "set flags for C/C++ compiler" );

    }

};

}

#endif
