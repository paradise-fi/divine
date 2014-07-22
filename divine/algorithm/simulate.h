// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>
// original version was created by Jan Havlíček <xhavlic4@fi.muni.cz>

#include <wibble/test.h>
#include <divine/algorithm/common.h>
#include <wibble/regexp.h>
#include <wibble/strongenumflags.h>
#include <cstdint>

#include <wibble/union.h>

#include <random>

#ifndef DIVINE_ALGORITHM_SIMULATE
#define DIVINE_ALGORITHM_SIMULATE

namespace divine {
namespace algorithm {

struct DfsNext { };
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
struct NoOp { };

using Command = wibble::Union<
                  DfsNext, Next, Random, Back,
                  FollowCE, FollowCEToEnd, Reset,
                  PrintTrace, PrintCE,
                  ListSuccs, ToggleAutoListing,
                  NoOp, Exit >;

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
using Options = wibble::StrongEnumFlags< Option >;

struct ProcessLoop {

    ProcessLoop() :
        _splitter( "[ \t]*,[ \t]*", REG_EXTENDED ),
        part( _splitter.end() ),
        _in( std::cin ),
        _out( std::cout ),
        _err( std::cerr )
    { }

    void help( bool error = false ) const {
        (error ? _err : _out)
            << "HELP:" << std::endl
            << "  1,2,3 use numbers to select a sucessor" << std::endl
            << "  s     list successors" << std::endl
            << "  S     toggle automatic listing of successors" << std::endl
            << "  b     back to the previous state" << std::endl
            << "  t     show current trace" << std::endl
            << "  c     show remaining part of counterexample" << std::endl
            << "  n     go to the next unvisited successor or go up if there is none (DFS)" << std::endl
            << "  r     go to a random successor" << std::endl
            << "  f [N] follow counterexample [for N steps]" << std::endl
            << "  F     follow counterexample to the end (to the first lasso accepting vertex for cycle)" << std::endl
            << "  i     go back to before-initial state (reset)" << std::endl
            << "  q     exit" << std::endl
            << "multiple commands can be separated by a comma" << std::endl;
    }

    Command command() {
        return command( getPart() );
    }

    Command command( std::string part ) const {

        if ( !_in.good() ) return Exit();
        if ( part.empty() ) return NoOp();
        if ( part == "q" ) return Exit();
        if ( part == "s" ) return ListSuccs();
        if ( part == "S" ) return ToggleAutoListing();
        if ( part == "b" ) return Back();
        if ( part == "t" ) return PrintTrace();
        if ( part == "c" ) return PrintCE();
        if ( part == "n" ) return DfsNext();
        if ( part == "r" ) return Random();
        if ( part == "F" ) return FollowCEToEnd();
        if ( part == "i" ) return Reset();
        if ( part[ 0 ] == 'f' ) {
            auto x = parse( part.substr( 1 ) );
            return FollowCE( x.second ? x.first : 1 );
        } else {
            int i;
            bool ok;
            std::tie( i, ok ) = parse( part );
            if ( !ok ) {
                help( true );
                return NoOp();
            }
            return Next( i );
        }
        help();
        return NoOp();
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
    wibble::Splitter _splitter;
    wibble::Splitter::const_iterator part;

    std::istream &_in;
    std::ostream &_out;
    std::ostream &_err;

    std::string last;

    bool getLine() {
        assert( part == _splitter.end() );
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

    static std::pair< int, bool > parse( std::string s ) {
        try {
            int i = std::stoi( s );
            return std::make_pair( i, true );
        } catch ( std::invalid_argument & ) {
            return std::make_pair( 0, false );
        }
    }
};

template< typename Setup >
struct Simulate : Algorithm, AlgorithmUtils< Setup, wibble::Unit >, Sequential
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
        assert( trace.empty() );
        clearSuccs();
        this->graph().initials( [ this ] ( Node, Node n, Label l ) {
            this->addSucc( n, l );
        });
    }

    // go to i-th successor
    void goDown( int i ) {
        assert_leq( 0, i );
        assert_leq( i, int( succs.size() - 1 ) );

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
        assert( !trace.empty() );

        extension( trace.back() ).intrace = false;
        trace.pop_back();

        if ( trace.empty() ) {
            generateInitials();
        } else {
            generateSuccessors( trace.back() );
        }
    }

    // one step of DFS, go down to the first unvisited state, otherwise go up
    bool stepDFS() {
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
        assert_leq( succ, i );
        assert( v.valid() );
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
        assert_leq( succ, i );
        assert( v.valid() );
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
        assert( extension( next ).ceNext );
        Vertex head;
        int firstSeen = 0;
        do {
            head = next;
            assert( (extension( head ).*what)() );

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
            assert_leq( 1, int( iniTrail.size() ) );
            from = getInitial( iniTrail[ 0 ] );
            assert( from.valid() );
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
        assert( !iniTrail.empty() );
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
            assert( extension( trace.back() ).inCEinit || extension( trace.back() ).inCElasso );
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

    CmdResponse runCommand( const Command cmd ) {

        if ( cmd.is< DfsNext >() ) {
            if ( !stepDFS() )
                loop.error( "Cannot go back" );
        } else if ( cmd.is< Next >() ) {
            int n = cmd.get< Next >().next;
            if ( n > 0 && n <= int( succs.size() ) )
                goDown( n - 1 );
            else if ( succs.empty() )
                loop.error( "Current state has no successor" );
            else
                loop.error( "Enter number between 1 and " + std::to_string( succs.size() ) );
        } else if ( cmd.is< Random >() ) {
            if ( succs.empty() )
                loop.error( "Current state has no successor" );
            else if ( succs.size() == 1 )
                goDown( 0 );
            else
                goDown( randFromTo( 0, succs.size() ) );
        } else if ( cmd.is< FollowCE >() ) {
            for ( int i = 0; i < cmd.get< FollowCE >().limit; ++i ) {
                if ( !followCE() ) {
                    loop.error( "There is no further CE continuation" );
                    break;
                }
            }
        } else if ( cmd.is< FollowCEToEnd >() ) {
            if ( !followCEToEnd() )
                loop.error( "Cannot follow CE from current vertex or no CE at all" );
        } else if ( cmd.is< Reset >() ) {
            trace.clear();
            generateInitials();
        } else if ( cmd.is< Back >() ) {
            if ( !trace.empty() )
                goBack();
            else
                loop.error( "Cannot go back" );
        } else if ( cmd.is< PrintTrace >() ) {
            for ( const auto &n : trace )
                showNode( n );
        } else if ( cmd.is< PrintCE >() ) {
            printCE();
        } else if ( cmd.is< ListSuccs >() ) {
            printSuccessors();
        } else if ( cmd.is< ToggleAutoListing >() ) {
            options ^= Option::AutoSuccs;
            loop.show( std::string( "Automatic listing of successors " ) +
                    (options.has( Option::AutoSuccs ) ? "enabled" : "disabled" ) );
        } else if ( cmd.is< Exit >() ) {
            loop.show( "" );
            return CmdResponse::Exit;
        } else if ( cmd.is< NoOp >() ) {
            return CmdResponse::Ignore;
        } else {
            assert_unreachable( "Unhandled case" );
        }
        return CmdResponse::Continue;
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
