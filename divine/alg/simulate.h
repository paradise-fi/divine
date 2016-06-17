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
#include <brick-string.h>

#include <divine/alg/common.h>

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

struct Mark { };
struct Unmark { };
struct StopOn {
    enum SC { Marked = 0x1, Accepting = 0x2, Deadlock = 0x4 };
    StopOn( SC v, bool set = true ) : value( v ), set( set ) { };

    SC value;
    bool set;
};

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
                  Mark, Unmark, StopOn,
                  PrintTrace, PrintCE, ListSuccs, ToggleAutoListing,
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


struct ApiError : std::logic_error {
    explicit ApiError( const std::string& error ) noexcept
        : std::logic_error( error + " while constructing command parser" )
    { }

    ~ApiError() noexcept = default;
};

struct ParseError : std::invalid_argument {
    explicit ParseError( const std::string &error ) noexcept
        : std::invalid_argument( error )
    { }

    ~ParseError() noexcept = default;
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

    CommandDesc( std::string longOpt, char shortOpt,
            std::string description, Command comm ) :
        CommandDesc( longOpt, shortOpt, "", description,
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
        parse( std::move( parse ) ), usage( usage ), description( description )
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
        if ( ptr->shortOpt )
            _shortOpts[ ptr->shortOpt ] = ptr;
        if ( !ptr->longOpt.empty() )
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

    Command parse( std::string line, std::ostream &err = std::cerr ) try {
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
        auto cmd = line.substr( 0, sp );
        auto param = sp == long( line.size() ) ? std::string() : line.substr( sp + 1 );
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
    } catch ( ParseError &ex ) {
        err << ex.what() << std::endl;
        return Command();
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
static Command parseLongParam( std::string s ) {
    if ( s.empty() )
        return Cmd( -1 );
    try {
        return Cmd( std::stol( s ) );
    } catch ( std::invalid_argument & ) {
        throw ParseError( "Expected nothing or positive number, given '" + s + "'" );
    }
}

template< StopOn::SC cond >
static Command parseStop( std::string s ) {
    if ( s == "on" || s == "ON" )
        return StopOn( cond, true );
    else if ( s == "off" || s == "OFF" )
        return StopOn( cond, false );
    throw ParseError( "Invalid option '" + s + "' expected 'on' or 'off'" );
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
            .add( "succs",                    's',        "list successors", ListSuccs() )
            .add( "toggle-successor-listing", 'S',        "toggle automatic listing of successors", ToggleAutoListing() )
            .add( "back",                     'b',        "back to previous state", Back() )
            .add( "show-trace",               't',        "show current trace", PrintTrace() )
            .add( "show-ce",                  'c',        "show remaining part of the counterexample", PrintCE() )
            .add( "step-dfs",                 'n',        "go to the next unvisited successor or go up if there is none (DFS)", DfsNext() )
            .add( "random-succ",              'r',        "go to a random successor", Random() )
            .add( "follow-ce",                'f', "[N]", "follow counterexample [for N steps]", &parseLongParam< FollowCE > )
            .add( "jump-goal",                'F',
                    "follow counterexample to the end (to the first lasso accepting vertex for cycle)", FollowCEToEnd() )
            .add( "run-dfs",                  'D', "[N]",
                    "run DFS [for N steps], can be interrupted by ctrl+c/SIGINT", &parseLongParam< RunDfs > )
            .add( "initial",                  'i',        "go back to before-initial state (reset)", Reset() )
            .add( "mark",                     'm',        "mark this state", Mark() )
            .add( "unmark",                   'M',        "unmark this state", Unmark() )
            .add( "stop-on-accepting",        0,   "{ on | off }",
                    "stop on any accepting or goal state", &parseStop< StopOn::Accepting > )
            .add( "stop-on-marked",           0,   "{ on | off }",
                    "stop on any marked state", &parseStop< StopOn::Marked > )
            .add( "stop-on-deadlock",         0,   "{ on | off }",
                    "stop on any deadlock (state without successorts)", &parseStop< StopOn::Deadlock > )

            .add( "help",                     'h',        "show help", Help() )
            .add( "quit",                     'q',        "exit simulation", Exit() )
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

    using Splitter = brick::string::Splitter;

    Splitter::const_iterator splitString( std::string str ) {
        return _splitter.begin( str );
    }

    Splitter::const_iterator splitEnd() { return _splitter.end(); }


  private:
    CommandEngine _engine;

    Splitter _splitter;
    Splitter::const_iterator part;

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

        bool marked:1;

        uint32_t ceNext:27;

        bool getInCEinit() { return inCEinit; }
        bool getInCElasso() { return inCElasso; }
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

    StopOn::SC stopOn;
    brick::data::Bimap< short, std::string > stateFlagMap;
    brick::data::SmallVector< short > stateFlagsAll;

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
        this->graph().successors( LongTerm(), from.node(), [ this ] ( Node n, Label l ) {
            this->addSucc( n, l );
        });
    }

    // fill the successor array by initial states
    void generateInitials() {
        ASSERT( trace.empty() );
        clearSuccs();
        this->graph().initials( LongTerm(), [ this ] ( Node, Node n, Label l ) {
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

    enum class StopReason { Done, EndOfModel, Signal, Deadlock, Accepting, Marked };
    using Stop = std::tuple< StopReason, long >;

    Stop runDfs( long limit ) {
        bool print = options.has( Option::PrintEdges );
        options.clear( Option::PrintEdges );
        auto oldsig = std::signal( SIGINT, &sigintHandler );
        ASSERT( oldsig != SIG_ERR );
        interrupted = false;
        brick::types::Defer _( [&]() {
                if ( print ) this->options |= Option::PrintEdges;
                std::signal( SIGINT, oldsig );
            } );

        for ( long i = 0; limit <= 0 || i < limit; ++i ) {
            if ( stepDfs() ) {
                if ( (stopOn & StopOn::Deadlock) == StopOn::Deadlock
                        && succs.empty() )
                    return Stop{ StopReason::Deadlock, i };
                if ( !trace.empty() ) {
                    if ( (stopOn & StopOn::Accepting) == StopOn::Accepting
                            && this->graph().stateFlags( trace.back().node(),
                                graph::FlagVector{ graph::flags::goal, graph::flags::accepting } ) )
                        return Stop{ StopReason::Accepting, i };
                    if ( (stopOn & StopOn::Marked) == StopOn::Marked
                            && extension( trace.back() ).marked )
                        return Stop{ StopReason::Marked, i };
                }

                if ( interrupted.load( std::memory_order_relaxed ) )
                    return Stop{ StopReason::Signal, i };
            } else
                return Stop{ StopReason::EndOfModel, i };
        }
        return Stop{ StopReason::Done, limit };
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
        this->graph().successors( LongTerm(), from.node(), [&]( Node n, Label ) {
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
        this->graph().initials( LongTerm(), [&]( Node, Node n, Label ) {
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
            if ( extension( trace.back() ).inCElasso
                    && this->graph().stateFlags( trace.back().node(), graph::flags::isAccepting ) )
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
        if ( extension( v ).marked )
            nodeinfo.emplace_back( "marked" );
        if ( this->graph().stateFlags( v.node(), graph::flags::isAccepting ) )
            nodeinfo.emplace_back( "accepting" );
        if ( this->graph().stateFlags( v.node(), graph::flags::isGoal ) )
            nodeinfo.emplace_back( "goal" );
        for ( auto f : this->graph().stateFlags( v.node(), stateFlagsAll ) )
            nodeinfo.emplace_back( stateFlagMap[ f ] );

        loop.show( "vertex: " + (nodeinfo.empty() ?
                                 "" : brick::string::fmt_container( nodeinfo, '{', '}' )) );
        loop.show( this->graph().showNode( v.node() ) );
    }

    enum class CmdResponse { Continue, Ignore, Exit };

    void batch( std::string cmds ) {
        for ( auto p = loop.splitString( cmds ); p != loop.splitEnd(); ++p )
            if ( runCommand( loop.command( *p ) ) == CmdResponse::Continue && !trace.empty() )
                showNode( trace.back() );
    }

    CmdResponse runCommand() {
        return runCommand( loop.command() );
    }

    CmdResponse runCommand( Command cmd ) try {
        cmd.match(
            [&]( DfsNext ) {
                if ( !this->stepDfs() )
                    this->loop.error( "Cannot go back" );
            }, [&]( Next n ) {
                if ( n.next > 0 && n.next <= int( this->succs.size() ) )
                    this->goDown( n.next - 1 );
                else if ( this->succs.empty() )
                    this->loop.error( "Current state has no successor" );
                else
                    this->loop.error( "Enter number between 1 and " + std::to_string( this->succs.size() ) );
            }, [&]( Random ) {
                if ( this->succs.empty() )
                    this->loop.error( "Current state has no successor" );
                else if ( this->succs.size() == 1 )
                    this->goDown( 0 );
                else
                    this->goDown( this->randFromTo( 0, this->succs.size() ) );
            }, [&]( FollowCE fc ) {
                if ( fc.limit <= 0 )
                    fc.limit = 1;
                for ( int i = 0; i < fc.limit; ++i ) {
                    if ( !this->followCE() ) {
                        this->loop.error( "There is no further CE continuation" );
                        break;
                    }
                }
            }, [&]( FollowCEToEnd ) {
                if ( !this->followCEToEnd() )
                    this->loop.error( "Cannot follow CE from current vertex or no CE at all" );
            }, [&]( Reset ) {
                this->trace.clear();
                this->generateInitials();
            }, [&]( Back ) {
                if ( !this->trace.empty() )
                    this->goBack();
                else
                    this->loop.error( "Cannot go back" );
            }, [&]( RunDfs dfs ) {
                auto r = this->runDfs( dfs.limit );
                auto n = std::to_string( std::get< 1 >( r ) );
                switch ( std::get< 0 >( r ) ) {
                    case StopReason::Signal:
                        this->loop.error( "Interrupted after " + n + " steps" );
                        break;
                    case StopReason::EndOfModel:
                        this->loop.error( "Model fully explored after " + n + " steps" );
                        break;
                    case StopReason::Deadlock:
                        this->loop.show( "Reached deadlock after " + n + " steps" );
                        break;
                    case StopReason::Accepting:
                        this->loop.show( "Reached accepting state after " + n + " steps" );
                        break;
                    case StopReason::Marked:
                        this->loop.show( "Reached marked state after " + n + " steps" );
                        break;
                    case StopReason::Done:
                        this->loop.show( "Done" );
                        break;
                    default:
                        ASSERT_UNREACHABLE( "unhandled case" );
                }
            }, [&]( Mark ) {
                if ( !this->trace.empty() )
                    this->extension( this->trace.back() ).marked = true;
            }, [&]( Unmark ) {
                if ( !this->trace.empty() )
                    this->extension( this->trace.back() ).marked = false;
            }, [&]( StopOn st ) {
                if ( st.set )
                    this->stopOn = StopOn::SC( this->stopOn | st.value );
                else
                    this->stopOn = StopOn::SC( this->stopOn & ~st.value );
            }, [&]( PrintTrace ) {
                for ( const auto &n : this->trace )
                    this->showNode( n );
            }, [&]( PrintCE ) {
                this->printCE();
            }, [&]( ListSuccs ) {
                this->printSuccessors();
            }, [&]( ToggleAutoListing ) {
                this->options ^= Option::AutoSuccs;
                this->loop.show( std::string( "Automatic listing of successors " ) +
                        (this->options.has( Option::AutoSuccs ) ? "enabled" : "disabled" ) );
            }, [&]( Exit  ) {
                this->loop.show( "" );
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

    Simulate( Meta m, typename Setup::Generator *g = nullptr )
        : Algorithm( m, sizeof( Extension ) )
    {
        this->init( *this, g );
        options |= Option::PrintEdges;
        inputTrace = m.input.trace;
        stopOn = StopOn::SC( 0 );
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

        stateFlagMap = graph::flags::flagMap( this->graph() );
        for ( auto &pair : stateFlagMap.left() )
            stateFlagsAll.emplace_back( pair.first );
    }
};

}
}

#endif // DIVINE_ALGORITHM_SIMULATE
