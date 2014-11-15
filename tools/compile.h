// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>
//             (c) 2013, 2014 Vladimír Štill <xstill@fi.muni.cz>

#include <vector>

#include <brick-commandline.h>
#include <brick-fs.h>
#include <brick-types.h>
#include <brick-string.h>

#include <divine/dve/compiler.h>
#include <divine/generator/cesmi.h>
#include <divine/utility/die.h>
#include <divine/utility/strings.h>
#include <divine/compile/llvm.h>

#include <tools/combine.h>
#include <tools/llvmpaths.h>

#ifndef DIVINE_COMPILE_H
#define DIVINE_COMPILE_H

using namespace brick;
using namespace brick::commandline;

namespace divine {

std::string ltl_to_c( int id, std::string ltl );

static inline std::string concat( commandline::VectorOption< String > *opt ) {
    assert( opt != nullptr );
    if ( opt->values().empty() )
        return "";
    std::stringstream ss;
    for ( auto it = opt->values().begin(); ; ) {
        ss << *it;
        if ( ++it != opt->values().end() )
            ss << " ";
        else
            break;
    }
    return ss.str();
}

struct Compile {
    commandline::Engine *cmd_compile;
    commandline::StandardParserWithMandatoryCommand &opts;

    BoolOption *o_cesmi, *o_llvm, *o_keep, *o_libs_only, *o_dont_link, *o_no_modeline;
    StringOption *o_out, *o_cmd_clang, *o_precompiled, *o_parallel;
    VectorOption< String > *o_definitions, *o_cflags;
    int parallelBuildJobs;

    struct FilePath {
        // filepath = joinpath(abspath, basename)
        std::string basename;
        std::string abspath;
    };

    void die_help( std::string bla )
    {
        opts.outputHelp( std::cerr );
        die( bla );
    }

    static void cleanup( FilePath &cleanup_tmpdir )
    {
        chdir( cleanup_tmpdir.abspath.c_str() );
        brick::fs::rmtree( cleanup_tmpdir.basename );
    }

    void run( std::string command ) {
        std::cerr << "+ " << command << std::endl;
        int status = system( command.c_str() );
        if ( status != 0 )
            die( brick::string::fmtf( "Error running an external command, exit code %d.", status ) );
    }

    void runCompiler( std::string comp, std::string in, std::string out, std::string flags = "" ) {
        std::stringstream cmd;
        cmd << comp << " " << flags << " -o " << out << " " << in;
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

    void compileDve( std::string in, std::vector< std::string > definitions ) {
#if GEN_DVE
        dve::compiler::DveCompiler compiler;
        compiler.read( in.c_str(), definitions );
        compiler.analyse();

        std::string outfile = brick::fs::splitFileName( in ).second + ".cpp";
        std::ofstream out( outfile.c_str() );
        compiler.setOutput( out );
        compiler.print_generator();

        gplusplus( outfile, brick::fs::splitFileName( in ).second + generator::cesmi_ext );
#else
        die( "FATAL: The DVE compiler requires DVE backend." );
        (void)(in);
        (void)(definitions);
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
        brick::fs::TempDir tmpdir( "_divine-compile.XXXXXX",
                                   brick::fs::AutoDelete( !o_keep->boolValue() ) );
        brick::fs::ChangeCwd rootDir( tmpdir.path );

        std::string in_filename = brick::fs::splitFileName( in ).second;
        std::string in_basename( in_filename, 0, in_filename.rfind( '.' ) );

        brick::fs::writeFile( "cesmi.h", cesmi_usr_cesmi_h_str );
        brick::fs::writeFile( "cesmi.cpp", cesmi_usr_cesmi_cpp_str );

        std::string extras, ltlincludes;
        int ltlcount = 0;
        while ( opts.hasNext() ) {
            std::string extra = opts.next();
            if ( brick::string::endsWith( extra, ".ltl" ) ) {
                std::string ltlpath = brick::fs::splitFileName( extra ).second + ".h";
                std::string code = "#include <cesmi.h>\n";
                parse_ltl( brick::fs::readFile( extra ), [&]( std::string formula ) {
                        code += ltl_to_c( ltlcount++, formula );
                    }, [&]( std::string ) {} );
                brick::fs::writeFile( ltlpath, code );
                ltlincludes += "#include <" + brick::fs::splitFileName( extra ).second + ".h>\n";
            } else
                extras += " " + extra;
        }

        extras += " cesmi.cpp";

        std::string impl = "cesmi-ltl",
                    aggr = "ltl-aggregate.cpp";
        brick::fs::writeFile( impl + ".cpp", cesmi_usr_ltl_cpp_str );
        brick::fs::writeFile( impl + ".h", cesmi_usr_ltl_h_str );
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
        run( "gcc " + flags + " -I." + " -o ../" + in_filename +
             generator::cesmi_ext + " " + in + extras );
    }

    void parseModelines( const std::string &file, std::vector< std::string > &modelineOpts ) {
        std::string line;
        std::ifstream fs{ file };
        enum { None, Block, BlockCf } state = None;

        static const std::string lineKey = "// divine:";
        static const std::string blockKey = "/* divine:";
        static const std::string lineCfKey = "// divine-cflags:";
        static const std::string blockCfKey = "/* divine-cflags:";
        brick::string::Splitter splitter( "[\t ][ \t]*", REG_EXTENDED );

        auto match = [ &line ]( const std::string &key ) { return line.substr( 0, key.size() ) == key; };
        auto addmods = [&]( std::string m ) {
            for ( auto it = splitter.begin( m ); it != splitter.end(); ++it )
                if ( !it->empty() && *it != "*/" )
                    modelineOpts.emplace_back( *it );
        };
        auto addcfmods = [&]( std::string m ) {
            for ( auto it = splitter.begin( m ); it != splitter.end(); ++it )
                if ( !it->empty() && *it != "*/" )
                    modelineOpts.emplace_back( "--cflags=" + *it );
        };

        for ( int i = 0; (state != None || i < 5) && std::getline( fs, line ); ++i ) {
            if ( state == None ) {
                if ( match( lineKey ) )
                    addmods( line.substr( lineKey.size() ) );
                else if ( match( lineCfKey ) )
                    addcfmods( line.substr( lineCfKey.size() ) );
                else if ( match( blockKey ) ) {
                    if ( line.find( "*/" ) == std::string::npos )
                        state = Block;
                    addmods( line.substr( blockKey.size() ) );
                } else if ( match( blockCfKey ) ) {
                    if ( line.find( "*/" ) == std::string::npos )
                        state = BlockCf;
                    addcfmods( line.substr( blockCfKey.size() ) );
                }
            } else if ( state == Block ) {
                addmods( line );
                if ( line.find( "*/" ) != std::string::npos )
                    state = None;
            } else if ( state == BlockCf ) {
                addcfmods( line );
                if ( line.find( "*/" ) != std::string::npos )
                    state = None;
            }
        }
    }


    void compileLLVM( std::string first_file, std::string out ) {
#if GEN_LLVM || GEN_LLVM_CSDR || GEN_LLVM_PROB || GEN_LLVM_PTST
        std::vector< std::string > files;
        if ( !first_file.empty() )
            files.push_back( first_file );
        while ( opts.hasNext() ) {
            auto f = opts.next();
            if ( !f.empty() )
                files.push_back( f );
        }

        if ( !o_no_modeline->boolValue() ) {
            std::vector< std::string > modelineOpts;
            for ( const auto &f : files )
                parseModelines( f, modelineOpts );
            std::cout << "INFO: modelines: " << brick::string::fmt( modelineOpts ) << std::endl;
            auto e = cmd_compile->parseExtraOptions( modelineOpts );
            if ( e.size() )
                std::cout << "WARNING: unknown modeline options: " << brick::string::fmt( e ) << std::endl;
        }

        std::string cflags = concat( o_cflags );

        struct LLVMEnv : compile::Env {
            LLVMEnv( Compile *comp ) : compile( comp ) { }

            void compileFile( std::string file, std::string flags ) override {
                compile->run( compile->clang() + " " + file + " " + flags );
            }

            Compile *compile;
        } env( this );

        env.usePrecompiled = o_precompiled->stringValue();
        env.librariesOnly = o_libs_only->boolValue();
        env.dontLink = o_dont_link->boolValue();
        env.useThreads = parallelBuildJobs;
        env.keepBuildDir = o_keep->boolValue();
        env.input = files;
        env.output = out;
        env.flags = cflags;

        compile::CompileLLVM llvm( env );
        llvm.compile();
#else
        die( "LLVM is disabled" );
        (void)(first_file);
        (void)(out);
#endif
    }

    int parseParallel( std::string str ) {
        std::stringstream ss( str );
        int j;
        ss >> j;
        if ( j <= 0 )
            die( "Invalid value for --job/-j: '" + str + "'" );
        return j;
    }

    void main() {
        std::string input = opts.next();
        parallelBuildJobs = o_parallel->boolValue()
            ? parseParallel( o_parallel->stringValue() )
            : 1;
        if ( !o_libs_only->boolValue() && !brick::fs::access( input, R_OK ) )
            die( "FATAL: cannot open input file " + input + " for reading" );
        if ( brick::string::endsWith( input, ".dve" ) )
            compileDve( input, o_definitions->values() );
        else if ( brick::string::endsWith( input, ".m" ) )
            compileMurphi( input );
        else if ( o_cesmi->boolValue() )
            compileCESMI( input, concat( o_cflags ) );
        else if ( o_llvm->boolValue() )
            compileLLVM( input, o_out->stringValue() );
        else {
            std::cerr << "Hint: Assuming --llvm. Use --cesmi to build CESMI modules." << std::endl;
            compileLLVM( input, o_out->stringValue() );
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

        o_cflags = cmd_compile->add< VectorOption< String > >(
            "cflags", 'f', "cflags", "",
            "set flags for C/C++ compiler" );

        o_precompiled = cmd_compile->add< StringOption >(
            "precompiled", '\0', "precompiled", "",
            "path to pre-built bitcode libraries" );

        o_libs_only = cmd_compile->add< BoolOption >(
            "libraries-only", '\0', "libraries-only", "",
            "only built runtime libraries (for use with --precompiled)");

        o_out = cmd_compile->add< StringOption >(
            "output-file", 'o', "output-file", "",
            "specify the output file name "
            "(only works with --llvm/-l)");

        o_dont_link = cmd_compile->add< BoolOption >(
            "dont-link", 0, "dont-link", "",
            "compile the source files, but do not link" );

        o_cmd_clang = cmd_compile->add< StringOption >(
            "cmd-clang", 0, "cmd-clang", "",
            std::string( "how to run clang [default: " ) + _cmd_clang + "]" );

        o_definitions = cmd_compile->add< VectorOption< String > >(
            "definition", 'D', "definition", "",
            "add definition for generator (can be specified multiple times)" );

        o_parallel = cmd_compile->add< StringOption >(
            "jobs", 'j', "jobs", "",
            "parallel building (like with make but -j (without parameter) is not supported)" );

        o_no_modeline = cmd_compile->add< BoolOption >(
            "no-modeline", 0, "no-modeline", "",
            "disable DIVINE modeline parsing" );
    }

};

}

#endif
