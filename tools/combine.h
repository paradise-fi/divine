// -*- C++ -*-
#include <iostream>
#include <sstream>
#include <brick-commandline.h>

#include <divine/ltl2ba/main.h>
#include <divine/utility/die.h>
#include <wibble/string.h>
#include <wibble/sys/fs.h>
#include <wibble/regexp.h>
#include <wibble/sys/pipe.h>
#include <wibble/sys/exec.h>

#ifndef DIVINE_COMBINE_H
#define DIVINE_COMBINE_H

extern const char *combine_m4;

#ifdef O_LTL2DSTAR
int main_ltl2dstar(int argc, const char **argv, std::istream& in, std::ostream& out);
#endif

#ifdef O_LTL3BA
#include <external/ltl3ba/ltl3ba.h>
#undef min // SIGH
int main_ltl3ba(int argc, char *argv[], std::ostream& out);
struct BState;
BState* get_buchi_states();
int get_buchi_accept();
std::list<std::string> get_buchi_all_symbols();
#endif

using namespace wibble;
using namespace brick;
using namespace brick::commandline;
using namespace sys;

namespace divine {

#ifdef O_LTL3BA
std::string buchi_to_cpp(BState* bstates, int accept, std::list< std::string > symbols);
#else
std::string graph_to_cpp(const BA_opt_graph_t &g);
#endif

template< typename F, typename G >
static inline void parse_ltl( std::string file, F ltl, G definition )
{
    wibble::Splitter lines( "\n", 0 );
    wibble::ERegexp prop( "^[ \t]*#property ([^\n]+)", 2 );
    wibble::ERegexp def( "^[ \t]*#define ([^\n]+)", 2 );

    std::vector< std::string >::iterator i;
    for ( Splitter::const_iterator ln = lines.begin( file ); ln != lines.end(); ++ln ) {
        if ( prop.match( *ln ) )
            ltl( prop[1] );
        if ( def.match( *ln ) )
            definition( def[1] );
    }
}

struct Combine {
    Engine *cmd_combine;
    IntOption *o_propId;
    BoolOption *o_stdout, *o_quiet, *o_help, *o_version, *o_det;
    StringOption *o_formula;
#ifdef O_LTL2DSTAR
    StringOption *o_condition;
#endif
    commandline::StandardParserWithMandatoryCommand &opts;

    bool have_ltl;
    bool probabilistic;

    std::string input, ltl, defs;
    std::string in_data, ltl_data, ltl_defs, system;
    std::string ext;
    // std::vector< std::string > defs;
    std::vector< std::string > ltl_formulae;

    void die_help( std::string bla )
    {
        opts.outputHelp( std::cerr );
        die( bla );
    }

    Combine( commandline::StandardParserWithMandatoryCommand &_opts )
        : opts( _opts )
    {
        cmd_combine = opts.addEngine(
            "combine",
            "input.[m][prob]dve [preprocessor defs]",
            "combine DVE models with LTL properties" );

        o_formula = cmd_combine->add< StringOption >(
            "formula", 'f', "formula", "",
            "a file with LTL properties" );
        o_propId = cmd_combine->add< IntOption >(
            "property", 'p', "property", "",
            "only process n-th property from the LTL file" );
        o_stdout = cmd_combine->add< BoolOption >(
            "stdout", 'o', "stdout", "",
            "print output to stdout; implies -q" );
        o_quiet = cmd_combine->add< BoolOption >(
            "quiet", 'q', "quiet", "",
            "suppress normal output" );
#ifdef O_LTL2DSTAR
        o_condition = cmd_combine->add< StringOption >(
            "condition", 'c', "condition", "",
            "acceptance condition type: NBA | DRA (default: NBA)" );
#endif
    }

    std::string m4( std::string in )
    {
        PipeThrough p( "m4" + defs );
        return p.run( in );
    }

    std::string cpp( std::string in )
    {
        PipeThrough p( "cpp -E -P" );
        return p.run( in );
    }

    std::string outFile( int i, std::string extension = "" )
    {
        if ( extension.empty() )
            extension = ext;

        std::ostringstream str;
        auto a = input.rfind( '/' ), b = input.rfind( '.' );
        if ( b == std::string::npos )
            throw wibble::exception::Generic( "Suffix expected." );
        if ( a == std::string::npos )
            a = 0;
        else
            a = a + 1;
        std::string base( input, a, b - a );
        str << base;
        if ( i != -1 ) {
            str << ".prop";
            str << i;
        }
        str << extension;
        return str.str();
    }

#ifdef O_LTL2DSTAR
    /// Translates ltl formula to automaton specified by acceptanceConditionType using external translator
    std::string ltl2dstarTranslation( const std::string& acceptanceConditionType, std::string ltl ) {
        // ltl2dstar expects the ltl formula to be in the prefix form
        LTL_parse_t ltlParse( ltl );
        LTL_formul_t ltlFormula;
        if ( !ltlParse.syntax_check( ltlFormula ) ) {
            std::cerr << "Syntax error in LTL formula: " << ltl << std::endl;
            return "";
        }
        ltlFormula = ltlFormula.negace();
        std::string ltlInPrefixNotation = ltlFormula.prefixNotation();

        const std::string ac("--automata=" + acceptanceConditionType);
        const char *argv[] = { "ltl2dstar", "--ltl2nba=spinint:ltl2ba", ac.c_str(), 
          "--output=plugin:DVE_Process", "--plugin=DVE_Process", "-", "-" };
        std::stringstream ltlStream, automatonStream;
        ltlStream << ltlInPrefixNotation;
        main_ltl2dstar( 7, argv, ltlStream, automatonStream ); 
        return automatonStream.str();
    }
#endif

#ifdef O_LTL3BA
    /// Translates ltl formula to Buchi automaton using ltl3ba
    static std::string ltl3baTranslation( std::string ltl ) {
        std::stringstream automatonStream, formulaStream;
        LTL_parse_t ltlParse( ltl );
        LTL_formul_t ltlFormula, ltlFormulaNeg;
        if ( !ltlParse.syntax_check( ltlFormula ) ) {
            std::cerr << "Syntax error in LTL formula: " << ltl << std::endl;
            return "";
        }
        ltlFormulaNeg = ltlFormula.negace();
        formulaStream << ltlFormulaNeg;

        char** argv = new char*[ 5 ];
        argv[ 0 ] = const_cast<char*>("ltl3ba");
        // option to produce more deterinistic automaton
        argv[ 1 ] = const_cast<char*>("-M");
        // enable strong fair (S)imulation reduction of BA
        argv[ 2 ] = const_cast<char*>("-S");
        argv[ 3 ] = const_cast<char*>("-f");
        std::string formula = formulaStream.str();
        argv[ 4 ] = const_cast<char*>(formula.c_str());
        main_ltl3ba(5, argv, automatonStream);
        return automatonStream.str();
    }
#endif

    void announce( int id, std::string descr ) {
        if ( !o_quiet->boolValue() && !o_stdout->boolValue() )
            std::cerr << outFile( id ) << ": " << descr << std::endl;
    }

    void output( int id, std::string data, std::string prop_descr ) {
        announce( id, prop_descr );
        if ( !o_stdout->boolValue() )
            fs::writeFile( outFile( id ), data );
        else
            std::cout << data;
    }

    template< typename F >
    void process_ltl( F each ) {
        parse_ltl( ltl_data, [&]( std::string f ) {
                this->ltl_formulae.push_back( f );
            }, [&]( std::string d ) {
                this->ltl_defs += "\n#define " + d;
            } );

        int id = 1;
        for ( auto i = ltl_formulae.begin(); i != ltl_formulae.end(); ++i, ++id ) {
            if ( o_propId->intValue() && o_propId->intValue() != id )
                continue;

            each( id, *i );
        }
    }

    void ltl_to_dve( int id, std::string ltl ) {
        std::string automaton;
        std::stringstream automaton_str;
#ifdef O_LTL2DSTAR
        if ( !o_condition->boolValue() || o_condition->stringValue() == "NBA" )
#endif
        {
#ifdef O_LTL3BA
            automaton = ltl3baTranslation( ltl );
#else
            divine::output( buchi( ltl, probabilistic ), automaton_str );
            automaton = automaton_str.str();
#endif
        }
#ifdef O_LTL2DSTAR
        // this can also handle NBA, Streett, etc.
        else if ( o_condition->stringValue() == "DRA" ) {
            automaton = ltl2dstarTranslation( "rabin", ltl );

            // something bad happened
            if (automaton.empty())
                return;
        }
#endif
        std::string prop = cpp( ltl_defs + "\n" + automaton );
        std::string dve = cpp( in_data + "\n" + prop + "\n" + system );

        output( id, dve, ltl );
    }

    void parseOptions() {
        try {
            if ( !opts.hasNext() )
                die_help( "FATAL: No input file specified." );
            input = opts.next();

            have_ltl = o_formula->boolValue();
            if ( have_ltl )
                ltl = o_formula->stringValue();

            while ( opts.hasNext() )
                defs += " -D" + opts.next(); // .push_back( opts.next() );

        } catch( commandline::BadOption &e ) {
            die_help( e.what() );
        }

        if ( have_ltl && o_stdout->boolValue() && !o_propId->intValue() )
            die( "FATAL: cannot print to stdout more than single property. Use -p n." );
    }

    int main() {
        parseOptions();

        probabilistic = str::endsWith( input, "probdve" );

        if ( probabilistic )
            ext = ".probdve";
        else if ( str::endsWith( input, "dve" ) )
            ext = ".dve";
        else
            die( "FATAL: Input file extension has to be one of "
                 ".o, .[m]dve or .[m]probdve." );

        if ( !fs::access( input, R_OK ) )
            die( "FATAL: Can't open '" + input + "' for reading." );

        in_data = fs::readFile( input ) + "\n";

        if ( have_ltl ) {
            if ( !fs::access( ltl, R_OK ) )
                die( "FATAL: Can't open '" + ltl + "' for reading." );
            ltl_data = fs::readFile( ltl ) + "\n";
        }

        if ( str::endsWith( ltl, ".mltl" ) ) {
            ltl_data = m4( combine_m4 + ltl_data );
        }

        if ( str::endsWith( input, ".mprobdve" )
             || str::endsWith( input, ".mdve" ) ) {
            in_data = m4( combine_m4 + in_data );
            if ( !have_ltl ) {
                output( -1, in_data, "(no property)" );
                return 0;
            }
        } else {
           if ( !have_ltl )
               die( "FATAL: Nothing to do." );
        }

        combine_dve();

        return 0;
    }

    void combine_dve() {
        auto off = in_data.find( "system async" );
        if ( off != std::string::npos )
            system = "system async property LTL_property;\n";
        else {
            off = in_data.find( "system sync" );
            if ( off != std::string::npos ) {
                system = "system sync property LTL_property;\n";
                std::cerr << "Warning: LTL model checking of synchronous "
                             "systems is not supported yet." << std::endl;
            } else {
                std::cerr << "Failing preprocessed source: " << std::endl;
                std::cerr << in_data;
                die( "FATAL: DVE has to specify either 'system sync' or "
                     "'system async'." );
            }
        }
        in_data = std::string( in_data, 0, off );

        process_ltl( [&]( int i, std::string ltl ) { return ltl_to_dve( i, ltl ); } );
    }

};

}

#endif
