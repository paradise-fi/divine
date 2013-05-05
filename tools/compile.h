// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <unistd.h>
#include <vector>

#include <wibble/commandline/parser.h>
#include <wibble/string.h>
#include <wibble/sys/fs.h>
#include <wibble/regexp.h>
#include <wibble/sys/pipe.h>
#include <wibble/sys/exec.h>
#include <wibble/sys/process.h>

#include "combine.h"
#include "llvmpaths.h"
#include <divine/dve/compiler.h>
#include <divine/generator/cesmi.h>
#include <divine/utility/strings.h>

#ifndef DIVINE_COMPILE_H
#define DIVINE_COMPILE_H

using namespace wibble;
using namespace commandline;
using namespace sys;

namespace divine {

struct stringtable { const char *n, *c; };
extern stringtable libsupcpp_list[];
extern stringtable pdclib_list[];

std::string ltl_to_c( int id, std::string ltl );

using namespace wibble;

struct Compile {
    commandline::Engine *cmd_compile;
    commandline::StandardParserWithMandatoryCommand &opts;

    BoolOption *o_cesmi, *o_llvm, *o_keep;
    StringOption *o_cflags, *o_out, *o_cmd_clang, *o_cmd_gold, *o_cmd_llvmgold, *o_cmd_ar;
    VectorOption< String > *o_definitions;

    struct FilePath {
        // filepath = joinpath(abspath, basename)
        std::string basename;
        std::string abspath;
    };

    FilePath *cleanup_tmpdir;

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

    void cleanup() // XXX RAII
    {
        if ( cleanup_tmpdir ) {
            chdir( cleanup_tmpdir->abspath.c_str() );
            wibble::sys::fs::rmtree( cleanup_tmpdir->basename );
        }
    }

    void run( std::string command ) {
        std::cerr << "+ " << command << std::endl;
        int status = system( command.c_str() );
#ifdef POSIX
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
        if ( status != -1 && WEXITSTATUS( status ) != 0 ) {
            cleanup();
            die( "Error running an external command." );
        }
#pragma GCC diagnostic pop
#endif
    }

    void runCompiler( std::string comp, std::string in, std::string out, std::string flags = "" ) {
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

    void gplusplus( std::string in, std::string out, std::string flags = "" ) {
	runCompiler ("g++", in, out, "-g -O2 -fPIC -shared " + flags);
    }

    std::string clang() {
        if ( o_cmd_clang->boolValue() )
            return o_cmd_clang->stringValue();
        return ::_cmd_clang;
    }

    std::string gold_plugin() {
        if ( o_cmd_llvmgold->boolValue() )
            return " --plugin " + o_cmd_llvmgold->stringValue();
        else if ( strlen( ::_cmd_llvmgold ) )
            return std::string( " --plugin " ) + ::_cmd_llvmgold;
        return "";
    }

    std::string gold_ar() {
        if ( o_cmd_ar->boolValue() )
            return o_cmd_ar->stringValue() + " r " + gold_plugin();
        else
            return std::string( ::_cmd_ar ) + " r " + gold_plugin();;
    }

    std::string gold() {
        if ( o_cmd_gold->boolValue() )
            return o_cmd_gold->stringValue() + gold_plugin();
        else
            return ::_cmd_gold + gold_plugin();;
    }

    void compileDve( std::string in, std::vector< std::string > definitions ) {
#if defined O_DVE
        dve::compiler::DveCompiler compiler;
        compiler.read( in.c_str(), definitions );
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

        if ( !o_keep->boolValue() )
            cleanup_tmpdir = &tmp_dir;

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

        std::string impl = tmp_dir.basename + "/cesmi-ltl",
                    aggr = tmp_dir.basename + "/ltl-aggregate.cpp";
        wibble::sys::fs::writeFile( impl + ".cpp", cesmi_usr_ltl_cpp_str );
        wibble::sys::fs::writeFile( impl + ".h", cesmi_usr_ltl_h_str );
        extras += " " + impl + ".cpp";

        std::ofstream aggr_s( aggr.c_str() );
        aggr_s << "#include <stdlib.h>" << std::endl;
        aggr_s << "#include <cesmi.h>" << std::endl;
        aggr_s << ltlincludes << std::endl;
        aggr_s << "extern \"C\" int buchi_accepting( int property, int id ) {\n";
        propswitch( aggr_s, ltlcount, "buchi_accepting", "( id )" );
        aggr_s << "}" << std::endl;
        aggr_s << "extern \"C\" int buchi_next( int property, int from, int transition, cesmi_setup *setup, cesmi_node n ) {\n";
        propswitch( aggr_s, ltlcount, "buchi_next", "( from, transition, setup, n )" );
        aggr_s << "}" << std::endl;
        aggr_s << "extern \"C\" int buchi_initial( int property ) {\n";
        propswitch( aggr_s, ltlcount, "buchi_initial", "" );
        aggr_s << "}" << std::endl;
        aggr_s << "int buchi_property_count = " << ltlcount << ";" << std::endl;

        aggr_s << "extern \"C\" char *buchi_formula( int property ) {\n";
        propswitch( aggr_s, ltlcount, "buchi_formula", "" );
        aggr_s << "}" << std::endl;

        extras += " " + aggr;
        aggr_s.close();

        std::string flags = "-Wall -shared -g -O2 -fPIC " + cflags;
        run( "gcc " + flags + " -I" + tmp_dir.basename + " -o " + in_basename +
             generator::cesmi_ext + " " + in + extras );
        cleanup();
    }

    template< typename Src >
    void compileLibrary( std::string name, Src src, std::string flags )
    {
        std::string files;
        fs::mkdirIfMissing( name, 0755 );
        chdir( name.c_str() );

        auto src_ = src;
        while ( src->n ) {
            fs::mkFilePath( src->n );
            fs::writeFile( src->n, src->c );
            ++src;
        }

        src = src_;
        while ( src->n ) {
            if ( str::endsWith( src->n, ".cc" ) ||
                 str::endsWith( src->n, ".c" ) ||
                 str::endsWith( src->n, ".cpp" ) ||
                 str::endsWith( src->n, ".cxx" ) ) {
                run( "clang -c " + flags + " -I. " + src->n + " -o " + src->n + ".bc" );
                files = files + src->n + ".bc ";
            }
            ++src;
        }

        run( gold_ar() + " ../" + name + ".a " + files );
        chdir( ".." );
    }

    void compileLLVM( std::string first_file, std::string cflags, std::string out ) {
#if O_LLVM
        // create temporary directory to compile in
        char tmp_dir_template[] = "_divine-compile.XXXXXX";
        FilePath tmp_dir;
        tmp_dir.abspath = process::getcwd();
        tmp_dir.basename = wibble::sys::fs::mkdtemp( tmp_dir_template );

        // prepare cleanup
        if ( !o_keep->boolValue() )
            cleanup_tmpdir = &tmp_dir;

        // enter tmp directory
        chdir( tmp_dir.basename.c_str() );

        // copy content of library files from memory to the directory
        fs::writeFile( "divine.h", llvm_usr_h_str );
        fs::writeFile( "pthread.h", llvm_usr_pthread_h_str );
        fs::writeFile( "pthread.cpp", llvm_usr_pthread_cpp_str );
        fs::writeFile( "cstdlib", llvm_usr_cstdlib_h_str );
        fs::writeFile( "cstdlib.cpp", llvm_usr_cstdlib_cpp_str );
        fs::writeFile( "assert.h", "#include <divine.h>\n" ); /* override PDClib's assert.h */

        // compile libraries
        std::string flags = "-emit-llvm -nobuiltininc -g ";
        compileLibrary( "libpdc", pdclib_list, flags + " -D_PDCLIB_BUILD -I.." );
        compileLibrary( "libsupc++", libsupcpp_list, flags + " -I../libpdc -I.." );

        flags += " -Ilibsupc++ -Ilibpdc ";

        run( "clang -c -I. " + flags + " cstdlib.cpp -o cstdlib.bc" );
        run( "clang -c -I. " + flags + " pthread.cpp -o pthread.bc" );
        run( gold_ar() + " libdivine.a cstdlib.bc pthread.bc" );

        fs::writeFile( "requires.c", /* whatever is needed in intrinsic lowering */
                       "extern void *memset;\n"
                       "void __divine_requires() {\n"
                       "    (void)memset;\n"
                       "}" );
        run( "clang -c " + flags + " -ffreestanding requires.c -o requires.bc" );

        // compile input file(s)
        std::string basename;
        std::string file = first_file, all_unlinked;

        flags += cflags;

        do {
            if ( file.empty() )
                file = opts.next();

            basename = str::basename( file ).substr( 0, str::basename( file ).rfind( '.' ) );

            if ( out.empty() )
                out = basename + ".bc";

            all_unlinked += str::joinpath( tmp_dir.basename, basename + ".bc" );
            run( "clang -c -I. " + flags + " ../" + file + " -o " + basename + ".bc" );

            file.clear();
        } while ( opts.hasNext() );

        chdir( tmp_dir.abspath.c_str() );

        run( gold() +
             " -plugin-opt emit-llvm " +
             " -o " + out + " " +
             all_unlinked +
             " -L./" + tmp_dir.basename + " " +
             tmp_dir.basename + "/requires.bc " +
             "-lsupc++ -lpdc -ldivine" );

        cleanup();
#else
        die( "LLVM is disabled" );
#endif
    }

    void main() {
        std::string input = opts.next();
        if ( access( input.c_str(), R_OK ) )
            die( "FATAL: cannot open input file " + input + " for reading" );
        if ( str::endsWith( input, ".dve" ) )
            compileDve( input, o_definitions->values() );
        else if ( str::endsWith( input, ".m" ) )
            compileMurphi( input );
        else if ( ( str::endsWith( input, ".c" ) || str::endsWith( input, ".cpp" ) ||
                    str::endsWith( input, ".cc" ) || str::endsWith( input, ".C" )) )
            if ( o_cesmi->boolValue() )
                compileCESMI( input, o_cflags->stringValue() );
            else if ( o_llvm->boolValue() )
                compileLLVM( input, o_cflags->stringValue(), o_out->stringValue() );
            else {
                std::cerr << "Do not know whether to process input file as of CESMI type ";
                std::cerr << "(use --cesmi or -c) or generate output file in LLVM bitcode/assembly file format ";
                std::cerr << "(use --llvm/-l or --llvm-assembly)." << std::endl;
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
            "compile input C/C++ program into LLVM bitecode");

        o_keep = cmd_compile->add< BoolOption >(
            "keep-build-directory", '\0', "keep-build-directory", "",
            "do not erase intermediate files after a compile" );

        o_cflags = cmd_compile->add< StringOption >(
            "cflags", 'f', "cflags", "",
            "set flags for C/C++ compiler" );

        o_out = cmd_compile->add< StringOption >(
            "output-file", 'o', "output-file", "",
            "specify the output file name "
            "(only works with --llvm/-l)");

        o_cmd_clang = cmd_compile->add< StringOption >(
            "cmd-clang", 0, "cmd-clang", "",
            std::string( "how to run clang [default: " ) + _cmd_clang + "]" );

        o_cmd_ar = cmd_compile->add< StringOption >(
            "cmd-ar", 0, "cmd-ar", "",
            std::string( "how to run ar [default: " ) + _cmd_ar + "]" );

        o_cmd_gold = cmd_compile->add< StringOption >(
            "cmd-gold", 0, "cmd-gold", "",
            std::string( "how to run GNU gold [default: " ) + _cmd_gold + "]" );

        o_cmd_llvmgold = cmd_compile->add< StringOption >(
            "cmd-llvmgold", 0, "cmd-llvmgold", "",
            std::string( "path to LLVMgold.so [default: " ) + _cmd_llvmgold );

        o_definitions = cmd_compile->add< VectorOption< String > >(
            "definition", 'D', "definition", "",
            "add definition for generator (can be specified multiple times)" );
    }

};

}

#endif
