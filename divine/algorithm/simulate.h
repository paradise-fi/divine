// -*- C++ -*- (c) 2013-2014 Vladimír Štill <xstill@fi.muni.cz>
// original version was created by Jan Havlíček <xhavlic4@fi.muni.cz>

#include <cstdint>
#include <cstdlib>
#include <csignal>
#include <cctype>
#include <random>
#include <initializer_list>
#include <iomanip>
#include <map>
#include <list>
#include <vector>

#include <brick-types.h>

#include <wibble/regexp.h>
#include <wibble/raii.h>
#include <divine/algorithm/common.h>

#ifndef DIVINE_ALGORITHM_SIMULATE
#define DIVINE_ALGORITHM_SIMULATE

namespace divine {
namespace algorithm {

struct DfsNext { };
struct RunDfs {
    long limit;
    RunDfs( long limit = 0 ) : limit( limit ) { }
};
struct Next {
    int next;
    Next( int next ) : next( next ) { }
};
struct Random { };
struct FollowCE {
    int limit;
    FollowCE( int limit ) : limit( limit ) { }
};
struct FollowCEToEnd { };
struct Back { };
struct Reset { };
struct PrintTrace { };
struct PrintCE { };
struct ListSuccs { };
struct ToggleAutoListing { };

struct Exit { };
struct Help { };
struct NoOp { };

using Command = brick::types::Union<
                  DfsNext, Next, Random, Back, RunDfs,
                  FollowCE, FollowCEToEnd, Reset,
                  PrintTrace, PrintCE,
                  ListSuccs, ToggleAutoListing,
                  NoOp, Help, Exit >;

enum class Option : uint32_t {
    TraceOpts = 0xffff,
    JumpTraceEnd = 0x0001,
    InteractiveTrace = 0x0002,
    CE = 0x0004,
    Lasso = 0x0008,

    DisplayOpts = 0xff0000,
    Interactive = 0x010000,
    AutoSuccs   = 0x200000, // all successors are automatically printed whenever the interactive prompt is shown
    PrintEdges  = 0x040000, // edge labels are printed for each successor
};
using Options = brick::types::StrongEnumFlags< Option >;


struct ApiError : std::logic_error
{
    ApiError(const std::string& error) throw ()
        : std::logic_error( error + " while constructing command parser" )
    { }

    ~ApiError() throw () { }
};

/* describes command, parse should return command representation, it will be
 * given string containing command parameters */
struct CommandDesc {

    CommandDesc() = default;
    CommandDesc( std::string longOpt, char shortOpt, std::string params,
            std::string description, std::function< Command( std::string ) > parse ) :
        parse( std::move( parse ) ), longOpt( longOpt ), shortOpt( shortOpt ),
        params( params ), description( description )
    {
        if ( !this->parse )
            throw ApiError( "Parse function is not callable" );
        if ( longOpt.empty() && shortOpt == 0 )
            throw ApiError( "Missing both short and long command" );
    }

    CommandDesc( std::string longOpt, char shortOpt, std::string params,
            std::string description, Command comm ) :
        CommandDesc( longOpt, shortOpt, params, description,
                [comm]( std::string ) { return comm; } )
    { }

    std::function< Command( std::string ) > parse;
    std::string longOpt;
    char        shortOpt;
    std::string params;
    std::string description;
};

/* describes commands which do not start with short or long option, parse will
 * be given raw line (without line terminator), it should return parsed command
 * or value of Command which is explicitly convertible to false */
struct SpecCommandDesc {
    SpecCommandDesc() = default;
    SpecCommandDesc( std::string usage, std::string description,
            std::function< Command( std::string ) > parse ) :
        usage( usage ), description( description ), parse( std::move( parse ) )
    {
        if ( !this->parse )
            throw ApiError( "Parse function is not callable" );
        if ( usage.empty() )
            throw ApiError( "usage must be given for SpecCommandDesc" );
    }

    std::function< Command( std::string ) > parse;
    std::string usage;
    std::string description;
};

/* Command processing engine, use add or addSpec to add commands
 *
 * Command matching (function parse) is performed as follows:
 * 1. if command matches any special option (in order of their addition)
 *    return of this special option is returned
 * 2. if command length is 1 it is matched by short options
 * 3. if command length is greater then 1 it is matches against long options
 *    3a. if that does not succeed it is partially matched against long options
 *        (that is we try to find unique option such that given command is
 *        a prefix of its long option)
 * 4. if nothing succeeds then help is printed and Command() is returned
 */
struct CommandEngine {
    CommandEngine( std::string description ) : _descrition( description ) { }

    CommandEngine &add( CommandDesc desc ) {
        if ( _shortOpts.count( desc.shortOpt ) )
            throw ApiError( "Short option clash for option '" + std::string( 1, desc.shortOpt ) + "'" );
        if ( _longOpts.count( desc.longOpt ) )
            throw ApiError( "Long option clash for option '" + desc.longOpt + "'" );

        _commands.emplace_back( std::move( desc ) );
        auto ptr = &_commands.back();
        _shortOpts[ ptr->shortOpt ] = ptr;
        _longOpts[ ptr->longOpt ] = ptr;
        return *this;
    }
    template< typename... Args >
    CommandEngine &add( Args &&...args ) {
        return add( CommandDesc( std::forward< Args >( args )... ) );
    }

    CommandEngine &addSpec( SpecCommandDesc spec ) {
        _specCommands.push_back( spec );
        return *this;
    }
    template< typename... Args >
    CommandEngine &addSpec( Args &&...args ) {
        _specCommands.emplace_back( std::forward< Args >( args )... );
        return *this;
    }

    std::string help() {
        std::stringstream str;
        help( str );
        return str.str();
    }

    void help( std::ostream &os ) const {
        os << _descrition << std::endl << std::endl;
        int offset = 0;
        for ( auto &cd : _commands )
            offset = std::max( offset, int( _formatOpt( cd ).size() ) );
        for ( auto &sc : _specCommands )
            offset = std::max( offset, int( sc.usage.size() ) );
        offset += 2;
        for ( auto &sc : _specCommands )
            os << sc.usage << std::string( offset - sc.usage.size(), ' ' )
               << sc.description << std::endl;
        for ( auto &cd : _commands )
            os << _formatOpt( cd, offset ) << cd.description << std::endl;
    }

    Command parse( std::string line, std::ostream &err = std::cerr ) {
        if ( line.empty() )
            return Command();
        Command comm;

        for ( auto sci = _specCommands.begin(); !comm && sci != _specCommands.end(); ++sci )
            comm = sci->parse( line );
        if ( comm )
            return comm;

        auto sp = std::find_if( line.begin(), line.end(),
                        []( char c ) { return bool( std::isspace( c ) ); } )
                    - line.begin();
        ASSERT_LEQ( 0, sp );
        ASSERT_LEQ( sp, line.size() );
        auto cmd = line.substr( 0, sp );
        auto param = sp == line.size() ? std::string() : line.substr( sp + 1 );
        CommandDesc *desc = nullptr;
        if ( cmd.size() == 1 ) {
            if ( _shortOpts.count( cmd[0] ) )
                desc = _shortOpts[ cmd[0] ];
            else {
                err << "Unknown short option '" << cmd << "'." << std::endl;
                help( err );
            }
        } else if ( _longOpts.count( cmd ) ) {
                desc = _longOpts[ cmd ];
        } else { // partial matching
            auto lb = _longOpts.lower_bound( cmd );
            std::vector< CommandDesc * > matches;
            for ( ; lb != _longOpts.end() && lb->second->longOpt.substr( 0, cmd.size() ) == cmd; ++lb )
                matches.push_back( lb->second );
            if ( matches.empty() ) {
                err << "Unknown long option '" << cmd << "'." << std::endl;
                help( err );
            } else if ( matches.size() > 1 ) {
                err << "Ambiguous option '" << cmd << "' can mean one of { ";
                for ( auto it = matches.begin(); (it + 1) != matches.end(); ++it )
                    err << (*it)->longOpt << ", ";
                err << matches.back()->longOpt << " }." << std::endl;
            } else
                desc = matches[ 0 ];
        }
        if ( !desc )
            return Command();
        return desc->parse( param );
    }

  private:
    std::string _descrition;
    // we need to store descriptions in container which does not invalidate
    // pointers to them
    std::list< SpecCommandDesc > _specCommands;
    std::list< CommandDesc > _commands;
    std::map< std::string, CommandDesc * > _longOpts;
    std::map< char, CommandDesc * > _shortOpts;

    static std::string _formatOpt( const CommandDesc &cd, int pad = 0 ) {
        std::string str;
        if ( cd.shortOpt != 0 && !cd.longOpt.empty() )
            str = "{ " + cd.longOpt + " | " + std::string( 1, cd.shortOpt ) + " }";
        else if ( !cd.longOpt.empty() )
            str = cd.longOpt;
        else
            str = std::string( 1, cd.shortOpt );
        if ( !cd.params.empty() )
            str += " " + cd.params;
        pad -= str.size();
        return pad > 0 ? str + std::string( pad, ' ' ) : str;
    }
};

template< typename Cmd >
static Command parseParams( std::string s ) {
    try {
        return Cmd( std::stol( s ) );
    } catch ( std::invalid_argument & ) {
        return Command();
    }
}

struct ProcessLoop {

    ProcessLoop() :
        _engine( "DIVINE simulate -- model simulator and interactive model checker" ),
        _splitter( "[ \t]*,[ \t]*", REG_EXTENDED ),
        part( _splitter.end() ),
        _in( std::cin ),
        _out( std::cout ),
        _err( std::cerr )
    {
        _engine
            .add( "succs",                    's', "",    "list successors", ListSuccs() )
            .add( "toggle-successor-listing", 'S', "",    "toggle automatic listing of successors", ToggleAutoListing() )
            .add( "back",                     'b', "",    "back to previous state", Back() )
            .add( "show-trace",               't', "",    "show current trace", PrintTrace() )
            .add( "show-ce",                  'c', "",    "show remaining part of the counterexample", PrintCE() )
            .add( "step-dfs",                 'n', "",    "go to the next unvisited successor or go up if there is none (DFS)", DfsNext() )
            .add( "random-succ",              'r', "",    "go to a random successor", Random() )
            .add( "follow-ce",                'f', "[N]", "follow counterexample [for N steps]", &parseParams< FollowCE > )
            .add( "jump-goal",                'F', "",
                    "follow counterexample to the end (to the first lasso accepting vertex for cycle)", FollowCEToEnd() )
            .add( "run-dfs",                  'D', "[N]",
                    "run DFS [for N steps], can be interrupted by ctrl+c/SIGINT", &parseParams< RunDfs > )
            .add( "initial",                  'i', "",    "go back to before-initial state (reset)", Reset() )
            .add( "help",                     'h', "",    "show help", Help() )
            .add( "quit",                     'q', "",    "exit simulation", Exit() )
            .addSpec( "1,2,3", "use numbers to select a sucessor", []( std::string str ) -> Command {
                        char *rest;
                        long val = std::strtol( str.c_str(), &rest, 0 );
                        if ( rest == str.c_str() )
                            return Command();
                        for ( int i = 0; rest[ i ]; ++i )
                            if ( !std::isspace( rest[ i ] ) )
                                return Command();
                        return Next( val );
                    } );
    }

    void help( bool error = false ) const { _engine.help( error ? _err : _out ); }

    Command command() {
        return command( getPart() );
    }

    Command command( std::string part ) {
        if ( !_in.good() ) return Exit();

        auto comm = _engine.parse( part, _err );
        if ( comm.is< Help >() ) {
            help();
            return NoOp();
        }
        return comm ? comm : NoOp();
    }

    void error( std::string what ) const {
        _err << what << std::endl;
    }

    void show( std::string what ) const {
        _out << what << std::endl;
    }

    wibble::Splitter::const_iterator splitString( std::string str ) {
        return _splitter.begin( str );
    }

    wibble::Splitter::const_iterator splitEnd() { return _splitter.end(); }


  private:
    CommandEngine _engine;

    wibble::Splitter _splitter;
    wibble::Splitter::const_iterator part;

    std::istream &_in;
    std::ostream &_out;
    std::ostream &_err;

    std::string last;

    bool getLine() {
        ASSERT( part == _splitter.end() );
        std::string line;
        std::getline( _in, line );
        part = _splitter.begin( line );
        return !line.empty();
    }

    std::string getPart() {
        if ( part == _splitter.end() ) {
            if ( !getLine() )
                return last;
        }
        last = *part;
        ++part;
        return last;
    }
};

static std::atomic< bool > interrupted;
static void sigintHandler( int ) {
    interrupted = true;
}

template< typename Setup >
struct Simulate : Algorithm, AlgorithmUtils< Setup, brick::types::Unit >, Sequential
{
    typedef Simulate< Setup > This;
    typedef typename Setup::Graph Graph;
    typedef typename Graph::Node Node;
    typedef typename Graph::Label Label;
    typedef typename Setup::Store::Vertex Vertex;


    struct Extension {
        bool seen:1;
        bool intrace:1;

        bool inCEinit:1;
        bool inCElasso:1;

        bool getInCEinit() { return inCEinit; }
        bool getInCElasso() { return inCElasso; }

        uint32_t ceNext:28;
    };

    Options options;

    std::vector< Vertex > trace;
    std::vector< std::pair< Vertex, Label > > succs;

    std::vector< int > iniTrail;
    std::vector< int > cycleTrail;

    ProcessLoop loop;

    std::mt19937 rand{ std::random_device()() };

    std::string inputTrace;
    std::string last;

    int id() { return 0; } // expected by AlgorithmUtils

    Extension &extension( Vertex n ) {
        return n.template extension< Extension >();
    }

    // clear the successor list
    void clearSuccs() {
        succs.clear();
    }

    // add successor to the list
    void addSucc( Node n, Label l ) {
        auto v = this->store().store( n );
        succs.emplace_back( v, l );
    }

    // fill the successor array
    void generateSuccessors( Vertex from ) {
        clearSuccs();
        this->graph().successors( from, [ this ] ( Node n, Label l ) {
            this->addSucc( n, l );
        });
    }

    // fill the successor array by initial states
    void generateInitials() {
        ASSERT( trace.empty() );
        clearSuccs();
        this->graph().initials( [ this ] ( Node, Node n, Label l ) {
            this->addSucc( n, l );
        });
    }

    // go to i-th successor
    void goDown( int i ) {
        ASSERT_LEQ( 0, i );
        ASSERT_LEQ( i, int( succs.size() - 1 ) );

        Vertex from = trace.empty()
                    ? Vertex()
                    : trace.back();

        // store selected successor
        trace.push_back( succs[ i ].first );
        extension( succs[ i ].first ).seen = true;
        extension( succs[ i ].first ).intrace = true;

        if ( options.has( Option::PrintEdges ) ) {
            std::string edge = this->graph().showTransition(
                    from.node(),
                    succs[ i ].first.node(),
                    succs[ i ].second );
            if ( !edge.empty() )
                loop.show( "=> " + edge );
        }

        generateSuccessors( trace.back() );
    }

    // backtrace by one
    void goBack() {
        ASSERT( !trace.empty() );

        extension( trace.back() ).intrace = false;
        trace.pop_back();

        if ( trace.empty() ) {
            generateInitials();
        } else {
            generateSuccessors( trace.back() );
        }
    }

    // one step of DFS, go down to the first unvisited state, otherwise go up
    bool stepDfs() {
        for ( unsigned int i = 0; i < succs.size(); i++ ) {
            if ( !extension( succs[ i ].first ).seen ) {
                goDown( i );
                return true;
            }
        }

        if ( trace.empty() )
            return false;

        goBack();
        return true;
    }

    brick::types::Maybe< long > runDfs( long limit ) {
        bool print = options.has( Option::PrintEdges );
        options.clear( Option::PrintEdges );
        auto oldsig = std::signal( SIGINT, &sigintHandler );
        ASSERT( oldsig != SIG_ERR );
        interrupted = false;
        auto _ = wibble::raii::defer( [&]() {
                if ( print ) options |= Option::PrintEdges;
                std::signal( SIGINT, oldsig );
            } );

        for ( long i = 0; limit <= 0 || i < limit; ++i ) {
            if ( stepDfs() ) {
                if ( interrupted.load( std::memory_order_relaxed ) )
                    return brick::types::Maybe< long >::Just( -i );
            } else
                return brick::types::Maybe< long >::Just( i );
        }
        return brick::types::Maybe< long >::Nothing();
    }

    void markCE() {
        Vertex prev, cur;
        if ( iniTrail.empty() )
            return;

        cur = getInitial( iniTrail[ 0 ] );
        extension( cur ).inCEinit = true;

        for ( auto it = iniTrail.begin() + 1; it != iniTrail.end(); ++it ) {
            prev = cur;
            cur = getSucc( prev, *it );
            extension( prev ).ceNext = *it;
            extension( cur ).inCEinit = true;
        }

        for ( auto it = cycleTrail.begin(); it != cycleTrail.end(); ++it ) {
            prev = cur;
            cur = getSucc( prev, *it );
            extension( prev ).ceNext = *it;
            extension( cur ).inCElasso = true;
        }
    }

    Vertex getSucc( Vertex from, int succ ) {
        int i = 0;
        Vertex v;
        this->graph().successors( from, [&]( Node n, Label ) {
            ++i;
            if ( i == succ )
                v = this->store().store( n );
            else
                this->pool().free( n );
        } );
        ASSERT_LEQ( succ, i );
        ASSERT( v.valid() );
        return v;
    }

    Vertex getInitial( int succ ) {
        int i = 0;
        Vertex v;
        this->graph().initials( [&]( Node, Node n, Label ) {
            ++i;
            if ( i == succ )
                v = this->store().store( n );
            else
                this->pool().free( n );
        } );
        ASSERT_LEQ( succ, i );
        ASSERT( v.valid() );
        return v;
    }

    template< typename Yield >
    void vertexCallback( Yield yield, const Vertex &v ) { yield( v ); }
    void vertexCallback( void (This::*yield)( const Vertex & ), const Vertex &v ) {
        (this->*yield)( v );
    }

    template< typename Yield >
    Vertex followExtension( const Vertex &from, bool (Extension::*what)(), Yield yield ) {
        Vertex next = from;
        ASSERT( extension( next ).ceNext );
        Vertex head;
        int firstSeen = 0;
        do {
            head = next;
            ASSERT( (extension( head ).*what)() );

            vertexCallback( yield, head );
            int target = extension( head ).ceNext;
            if ( target == 0 ) // end of ce
                break;
            next = getSucc( head, target );
          // we continue until beginning of new type or arrival at beginning
          if ( head.handle().asNumber() == from.handle().asNumber() )
              ++firstSeen;
        } while ( (extension( next ).*what)() && firstSeen < 2 );
        return head;
    }

    void printCE() {
        if ( !options.has( Option::CE ) ) {
            loop.error( "No counterexample" );
            return;
        }

        Vertex from;
        if ( trace.empty() ) {
            ASSERT_LEQ( 1, int( iniTrail.size() ) );
            from = getInitial( iniTrail[ 0 ] );
            ASSERT( from.valid() );
        } else {
            from = trace.back();
        }
        if ( extension( from ).inCEinit ) {
            if ( options.has( Option::Lasso ) )
                loop.show( "-- the trace to cycle" );
            else
                loop.show( "-- the trace to goal" );
            auto v = followExtension( from, &Extension::getInCEinit, &This::showNode );
            if ( extension( v ).inCElasso ) {
                loop.show( "-- the cycle" );
                followExtension( v, &Extension::getInCElasso, &This::showNode );
            }
        } else if ( extension( from ).inCElasso ) {
            loop.show( "-- the cycle" );
            followExtension( from, &Extension::getInCElasso, &This::showNode );
        } else {
            loop.error( "Current node is not part of counterexample" );
        }
    }

    // print successors
    void printSuccessors() {
        const int LINE = 40;
        unsigned int id = 0;
        for ( const auto &s : succs ) {
            Node n = s.first.node();
            ++id;

            std::stringstream ss;
            ss << "== # " << id << " ";

            auto str = ss.str();
            int chars = std::max( LINE - int( str.size() ), 0 );
            loop.show( str + std::string( chars, '=' ) );

            // print transition
            if ( options.has( Option::PrintEdges ) && !trace.empty() ) {
                std::string edge = this->graph().showTransition( trace.back().node(), n, s.second );
                if ( !edge.empty() )
                    loop.show( "=> " + edge );
            }

            // print successor
            showNode( s.first );
        }

        if ( id )
            loop.show( std::string( LINE, '=' ) );
        loop.show( std::to_string( succs.size() ) + " successors" );
    }

    void printCurrent() {
        if ( !trace.empty() )
            showNode( trace.back() );
    }

    // called whenever the current state is changed
    void printUpdate() {
        if ( !trace.empty() ) {
            loop.error( "-- you are here (and there are "
                    + std::to_string( succs.size() ) + " successors):" );
            printCurrent();
        } else {
            loop.error( "-- choose an initial state ("
                    + std::to_string( succs.size() ) + " available)" );
        }
    }

    int randFromTo( int from, int toExclusive ) {
        std::uniform_int_distribution< int > dist( from, toExclusive - 1 );
        return dist( rand );
    }

    bool assertCEVertex() {
        if ( !options.has( Option::CE ) )
            return false;
        if ( !trace.empty()
                && !extension( trace.back() ).inCEinit
                && !extension( trace.back() ).inCElasso )
            return false;
        ASSERT( !iniTrail.empty() );
        return true;
    }

    bool followCE() {
        if ( !assertCEVertex() )
            return false;

        if ( trace.empty() ) {
            generateInitials();
            goDown( iniTrail[ 0 ] - 1 );
            return true;
        } else if ( extension( trace.back() ).ceNext ) {
            goDown( extension( trace.back() ).ceNext - 1 );
            return true;
        }
        return false;
    }

    bool followCEToEnd() {
        if ( !assertCEVertex() )
            return false;

        if ( trace.empty() ) {
            generateInitials();
            goDown( iniTrail[ 0 ] - 1 );
        }

        while ( true ) {
            ASSERT( extension( trace.back() ).inCEinit || extension( trace.back() ).inCElasso );
            if ( extension( trace.back() ).ceNext == 0 )
                return true;
            if ( extension( trace.back() ).inCElasso && this->graph().isAccepting( trace.back().node() ) )
                return true;
            goDown( extension( trace.back() ).ceNext - 1 );
        }
    }

    void showNode( const Vertex &v ) {
        std::vector< std::string > nodeinfo;
        if ( extension( v ).seen )
            nodeinfo.emplace_back( "visited" );
        if ( extension( v ).intrace )
            nodeinfo.emplace_back( "in-trace" );
        if ( extension( v ).inCEinit )
            nodeinfo.emplace_back( "in-ce-init" );
        if ( extension( v ).inCElasso )
            nodeinfo.emplace_back( "in-ce-lasso" );
        if ( this->graph().isAccepting( v.node() ) )
            nodeinfo.emplace_back( "accepting" );
        if ( this->graph().isGoal( v.node() ) )
            nodeinfo.emplace_back( "goal" );

        loop.show( "vertex: " + (nodeinfo.empty() ?
                "" : wibble::str::fmt_container( nodeinfo, '{', '}' )) );
        loop.show( this->graph().showNode( v.node() ) );
    }

    enum class CmdResponse { Continue, Ignore, Exit };

    void batch( std::string cmds ) {
        for ( auto p = loop.splitString( cmds ); p != loop.splitEnd(); ++p )
            if ( runCommand( loop.command( *p ) ) == CmdResponse::Continue )
                showNode( trace.back() );
    }

    CmdResponse runCommand() {
        return runCommand( loop.command() );
    }

    CmdResponse runCommand( Command cmd ) try {
        cmd.match(
            [&]( DfsNext ) {
                if ( !stepDfs() )
                    loop.error( "Cannot go back" );
            }, [&]( Next n ) {
                if ( n.next > 0 && n.next <= int( succs.size() ) )
                    goDown( n.next - 1 );
                else if ( succs.empty() )
                    loop.error( "Current state has no successor" );
                else
                    loop.error( "Enter number between 1 and " + std::to_string( succs.size() ) );
            }, [&]( Random ) {
                if ( succs.empty() )
                    loop.error( "Current state has no successor" );
                else if ( succs.size() == 1 )
                    goDown( 0 );
                else
                    goDown( randFromTo( 0, succs.size() ) );
            }, [&]( FollowCE fc ) {
                if ( fc.limit <= 0 )
                    fc.limit = 1;
                for ( int i = 0; i < fc.limit; ++i ) {
                    if ( !followCE() ) {
                        loop.error( "There is no further CE continuation" );
                        break;
                    }
                }
            }, [&]( FollowCEToEnd ) {
                if ( !followCEToEnd() )
                    loop.error( "Cannot follow CE from current vertex or no CE at all" );
            }, [&]( Reset ) {
                trace.clear();
                generateInitials();
            }, [&]( Back ) {
                if ( !trace.empty() )
                    goBack();
                else
                    loop.error( "Cannot go back" );
            }, [&]( RunDfs dfs ) {
                auto r = runDfs( dfs.limit );
                if ( r && r.value() < 0 )
                    loop.error( "Interrupted after " + std::to_string( - r.value() ) + " steps" );
                else if ( r && r.value() > 0 )
                    loop.error( "Model fully explored after " + std::to_string( r.value() ) + " steps" );
            }, [&]( PrintTrace ) {
                for ( const auto &n : trace )
                    showNode( n );
            }, [&]( PrintCE ) {
                printCE();
            }, [&]( ListSuccs ) {
                printSuccessors();
            }, [&]( ToggleAutoListing ) {
                options ^= Option::AutoSuccs;
                loop.show( std::string( "Automatic listing of successors " ) +
                        (options.has( Option::AutoSuccs ) ? "enabled" : "disabled" ) );
            }, [&]( Exit  ) {
                loop.show( "" );
                throw CmdResponse::Exit;
            }, [&]( NoOp ) {
                throw CmdResponse::Ignore;
            } );
        return CmdResponse::Continue;
    } catch ( CmdResponse resp ) {
        return resp;
    }

    void evalLoop() {
        CmdResponse resp = CmdResponse::Continue;
        do {
            if ( resp == CmdResponse::Continue && options.has( Option::AutoSuccs ) )
                printSuccessors();
            if ( options.has( Option::Interactive ) )
                printUpdate();
            std::cerr << "> " << std::flush;
        } while ( (resp = runCommand()) != CmdResponse::Exit );
    }

    void run() {
        if ( inputTrace.empty() ) {
            generateInitials();
            evalLoop();
        } else {
            generateInitials();
            batch( inputTrace );
            if ( options.has( Option::Interactive ) ) {
                inputTrace.clear();
                evalLoop();
            }
        }
    }

    Simulate( Meta m ) : Algorithm( m, sizeof( Extension ) ) {
        this->init( *this );
        options |= Option::PrintEdges;
        inputTrace = m.input.trace;
        if ( m.algorithm.interactive )
            options |= Option::Interactive;

        if ( m.result.propertyHolds == meta::Result::R::No ) {
            options |= Option::Interactive;
            options |= Option::CE;
            iniTrail = m.result.iniTrail;
            if ( m.result.ceType == meta::Result::CET::Cycle ) {
                options |= Option::Lasso;
                cycleTrail = m.result.cycleTrail;
            }
            markCE();
        }
    }
};

}
}

#endif // DIVINE_ALGORITHM_SIMULATE
