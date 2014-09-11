// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>
//             (c) 2013, 2014 Vladimír Štill <xstill@fi.muni.cz>

#include <unistd.h>
#include <vector>
#include <future>

#include <brick-commandline.h>

#include <wibble/string.h>
#include <wibble/sys/fs.h>
#include <wibble/regexp.h>
#include <wibble/sys/pipe.h>
#include <wibble/sys/exec.h>
#include <wibble/sys/process.h>
#include <wibble/raii.h>

#include <divine/dve/compiler.h>
#include <divine/generator/cesmi.h>
#include <divine/utility/strings.h>
#include <divine/utility/die.h>

#include <tools/combine.h>
#include <tools/llvmpaths.h>

#ifndef DIVINE_COMPILE_H
#define DIVINE_COMPILE_H

using namespace wibble;
using namespace brick;
using namespace brick::commandline;
using namespace sys;

namespace divine {

extern stringtable pdclib_list[];
extern stringtable libm_list[];
extern stringtable libunwind_list[];
extern stringtable libcxxabi_list[];
extern stringtable libcxx_list[];

std::string ltl_to_c( int id, std::string ltl );

using namespace wibble;

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
    StringOption *o_out, *o_cmd_clang, *o_cmd_gold, *o_cmd_llvmgold,
                 *o_cmd_ar, *o_precompiled, *o_parallel;
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
        wibble::sys::fs::rmtree( cleanup_tmpdir.basename );
    }

    struct RunQueue {
        Compile *compile;
        std::vector< std::string > commands;
        std::atomic< size_t > current;

        RunQueue( Compile *compile ) : compile( compile ), current( 0 ) { }

        void push( std::string comm ) {
            commands.push_back( comm );
        }

        void run() {
            assert( compile );
            int extra = compile->parallelBuildJobs - 1;
            std::vector< std::future< void > > thrs;
            for ( int i = 0; i < extra; ++i )
                thrs.push_back( std::async( std::launch::async, &RunQueue::_run, this ) );

            _run();
            for ( auto &t : thrs )
                t.get(); // get propagates exceptions
        }

        void _run() {
            int job;
            while ( (job = current.fetch_add( 1 )) < int( commands.size() ) )
                compile->run( commands[ job ] );
        }
    };

    void run( std::string command ) {
        std::cerr << "+ " << command << std::endl;
        int status = system( command.c_str() );
        if ( status != 0 )
            die( wibble::str::fmtf( "Error running an external command, exit code %d.", status ) );
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
#if GEN_DVE
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

        auto clean = wibble::raii::refDeleteIf( !o_keep->boolValue(), tmp_dir, cleanup );

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
    }

    template< typename Src >
    void prepareIncludes( std::string name, Src src ) {
        fs::mkdirIfMissing( name, 0755 );
        chdir( name.c_str() );
        prepareIncludes( src );
        chdir( ".." );
    }

    template< typename Src >
    void prepareIncludes( Src src ) {
        while ( src->n ) {
            fs::mkFilePath( src->n );
            fs::writeFile( src->n, src->c );
            ++src;
        }
    }

    template< typename Src >
    void compileLibrary( std::string name, Src src, std::string flags )
    {
        std::string files;
        auto src_ = src;
        prepareIncludes( name, src );

        chdir( name.c_str() );
        if ( o_precompiled->boolValue() || o_dont_link->boolValue() ) {
            chdir( ".." );
            return; /* we only need the headers from above */
        }

        src = src_;

        RunQueue rq( this );
        while ( src->n ) {
            if ( str::endsWith( src->n, ".cc" ) ||
                 str::endsWith( src->n, ".c" ) ||
                 str::endsWith( src->n, ".cpp" ) ||
                 str::endsWith( src->n, ".cxx" ) ) {
                rq.push( clang() + " -c " + flags + " -I. " + src->n + " -o " + src->n + ".bc" );
                files = files + src->n + ".bc ";
            }
            ++src;
        }

        rq.run();
        run( gold_ar() + " ../" + name + ".a " + files );
        chdir( ".." );
    }

    void parseModelines( const std::string &file, std::vector< std::string > &modelineOpts ) {
        std::string line;
        std::ifstream fs{ file };
        enum { None, Block, BlockCf } state = None;

        static const std::string lineKey = "// divine:";
        static const std::string blockKey = "/* divine:";
        static const std::string lineCfKey = "// divine-cflags:";
        static const std::string blockCfKey = "/* divine-cflags:";
        wibble::Splitter splitter( "[\t ][ \t]*", REG_EXTENDED );

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
#if GEN_LLVM
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
            std::cout << "INFO: modelines: " << wibble::str::fmt( modelineOpts ) << std::endl;
            auto e = cmd_compile->parseExtraOptions( modelineOpts );
            if ( e.size() )
                std::cout << "WARNING: unknown modeline options: " << wibble::str::fmt( e ) << std::endl;
        }

        std::string cflags = concat( o_cflags );

        // create temporary directory to compile in
        char tmp_dir_template[] = "_divine-compile.XXXXXX";
        FilePath tmp_dir;
        tmp_dir.abspath = process::getcwd();
        tmp_dir.basename = wibble::sys::fs::mkdtemp( tmp_dir_template );

        // prepare cleanup
        auto clean = wibble::raii::refDeleteIf( !o_keep->boolValue(), tmp_dir, cleanup );

        // enter tmp directory
        chdir( tmp_dir.basename.c_str() );

        // copy content of library files from memory to the directory
        prepareIncludes( llvm_h_list );
        fs::mkFilePath( "bits/pthreadtypes.h" );
        fs::writeFile( "bits/pthreadtypes.h" , "#include <pthread.h>" );
        fs::writeFile( "assert.h", "#include <divine.h>\n" ); /* override PDClib's assert.h */
        fs::mkFilePath( "divine/problem.def" );
        fs::writeFile( "divine/problem.def", src_llvm::llvm_problem_def_str );
        if ( !o_dont_link->boolValue() )
            prepareIncludes( llvm_list );

        // compile libraries
        std::string flags = "-D__divine__ -emit-llvm -nobuiltininc -nostdinc -Xclang -nostdsysteminc -nostdinc++ -g ";
        compileLibrary( "libpdc", pdclib_list, flags + " -D_PDCLIB_BUILD -I.." );
        compileLibrary( "libm", libm_list, flags + " -I../libpdc -I." );

        prepareIncludes( "libcxx", libcxx_list ); // cyclic dependency cxxabi <-> cxx
        compileLibrary( "libcxxabi", libcxxabi_list, flags + " -I../libpdc -I../libm "
                        "-I.. -Iinclude -I../libcxx/std -I../libcxx -std=c++11 -fstrict-aliasing" );
        compileLibrary( "libcxx", libcxx_list, flags + " -I../libpdc -I../libm "
                        "-I../libcxxabi/include -I.. -Istd -I. -fstrict-aliasing -std=c++11 -fstrict-aliasing" );

        flags += " -Ilibcxxabi/include -Ilibpdc -Ilibcxx/std -Ilibcxx -Ilibm ";

        if ( !o_precompiled->boolValue() && !o_dont_link->boolValue() ) {
            run( clang() + " -c -I. " + flags + " glue.cpp -o glue.bc" );
            run( clang() + " -c -I. " + flags + " stubs.cpp -o stubs.bc" );
            run( clang() + " -c -I. " + flags + " entry.cpp -o entry.bc" );
            run( clang() + " -c -I. " + flags + " pthread.cpp -o pthread.bc" );
            run( clang() + " -c -I. -Ilibcxxabi " + flags // needs part of private cxxabi headers
                    + " cxa_exception_divine.cpp -o cxa_exception_divine.bc" );
            run( gold_ar() + " libdivine.a entry.bc stubs.bc glue.bc pthread.bc cxa_exception_divine.bc" );
        }

        if ( o_libs_only->boolValue() ) {
            fs::renameIfExists( "libdivine.a", tmp_dir.abspath + "/libdivine.a" );
            fs::renameIfExists( "libpdc.a", tmp_dir.abspath + "/libpdc.a" );
            fs::renameIfExists( "libcxxabi.a", tmp_dir.abspath + "/libcxxabi.a" );
            fs::renameIfExists( "libcxx.a", tmp_dir.abspath + "/libcxx.a" );
            fs::renameIfExists( "libunwind.a", tmp_dir.abspath + "/libunwind.a" );
            chdir( tmp_dir.abspath.c_str() );
            return;
        }

        if ( !o_dont_link->boolValue() ) {
            fs::writeFile( "requires.c", /* whatever is needed in intrinsic lowering */
                           "extern void *memset, *memcpy, *memmove, *_divine_start;\n"
                           "void __divine_requires() {\n"
                           "    (void) memset; (void) memcpy; (void) memmove; (void) _divine_start;\n"
                           "}" );
            run( clang() + " -c " + flags + " -ffreestanding requires.c -o requires.bc" );
         }

        // compile input file(s)
        std::string basename, compilename;
        std::string all_unlinked;

        flags += cflags;

        if ( files.size() >= 2 && o_dont_link->boolValue() && !out.empty() )
            die( "Cannot specify both -o and --dont-link with multiple files." );

        for ( auto file : files ) {

            basename = str::basename( file ).substr( 0, str::basename( file ).rfind( '.' ) );
            compilename = basename + ".bc";

            if ( out.empty() && !o_dont_link->boolValue() ) {
                // If -o and --dont-link are not specified, then choose the name of the first file in the list
                // for the target name.
                out = compilename;
            }

            if ( !str::endsWith( file, ".bc" ) ) {
                run( clang() + " -c -I. " + flags + " " + wibble::str::appendpath( "../", file )
                     + " -o " + compilename );

                if ( o_dont_link->boolValue() ) {
                    fs::renameIfExists( compilename,
                                        out.empty() ? ( wibble::str::appendpath( "../", compilename ) ) :
                                                      ( wibble::str::appendpath( "../", out ) ) );
                } else {
                    all_unlinked += str::joinpath( tmp_dir.basename, compilename ) + " ";
                }
            } else {
                if ( !o_dont_link->boolValue() ) {
                    all_unlinked += file + " ";
                }
            }

        }

        chdir( tmp_dir.abspath.c_str() );

        if ( !o_dont_link->boolValue() ) {
            run( gold() +
                " -plugin-opt emit-llvm " +
                " -o " + out + " " +
                all_unlinked +
                ( o_precompiled->boolValue() ?
                    ( " -L" + o_precompiled->stringValue() ) :
                    ( " -L./" + tmp_dir.basename ) ) + " " +
                tmp_dir.basename + "/requires.bc " +
                "-ldivine -lcxxabi -lcxx -lcxxabi -lpdc -ldivine" );
        }

#else
        die( "LLVM is disabled" );
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
        if ( !o_libs_only->boolValue() && access( input.c_str(), R_OK ) )
            die( "FATAL: cannot open input file " + input + " for reading" );
        if ( str::endsWith( input, ".dve" ) )
            compileDve( input, o_definitions->values() );
        else if ( str::endsWith( input, ".m" ) )
            compileMurphi( input );
        else if ( o_cesmi->boolValue() )
            compileCESMI( input, concat( o_cflags ) );
        else if ( o_llvm->boolValue() )
            compileLLVM( input, o_out->stringValue() );
        else {
            std::cerr << "Do not know how to compile this file type." << std::endl
                      << "Did you mean to run me with --llvm or --cesmi?" << std::endl;
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
