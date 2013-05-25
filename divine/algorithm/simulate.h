#include <divine/algorithm/common.h>
//#include <wibble/regexp.h>

#ifndef DIVINE_ALGORITHM_SIMULATE
#define DIVINE_ALGORITHM_SIMULATE

namespace divine {
namespace algorithm {

template< typename Setup >
struct Simulate : Algorithm, AlgorithmUtils< Setup >, Sequential
{
    typedef Simulate< Setup > This;
    typedef typename Setup::Graph Graph;
    typedef typename Graph::Node Node;
    typedef typename Graph::Label Label;
    typedef typename Setup::Store::Vertex Vertex;

    struct Extension {
        bool seen;
    };

    std::vector< Vertex > trace;
    std::vector< Vertex > succs;


    // when true, edge labels are printed for each successor
    bool printEdges;
    // when true, all successors are automatically printed whenever the interactive prompt is shown
    bool autoSuccs;
    // command to run, the interactive prompt is shown if blank
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
    void addSucc( Node n ) {
        auto v = this->store().store( n );
        succs.push_back( v );
    }

    // fill the successor array
    void generateSuccessors( Vertex from ) {
        clearSuccs();
        this->graph().successors( from, [ this ] ( Node n, Label ) {
            this->addSucc( n );
        });
    }

    // fill the successor array by initial states
    void generateInitials() {
        assert( trace.empty() );
        clearSuccs();
        this->graph().initials( [ this ] ( Node f, Node n, Label ) {
            this->addSucc( n );
        });
    }

    // go to i-th successor
    void goDown( unsigned int i ) {
        assert( i < succs.size() );

        // store selected successor
        trace.push_back( succs[ i ] );
        extension( succs[ i ] ).seen = true;

        generateSuccessors( trace.back() );
    }

    // backtrace by one
    void goBack() {
        assert( !trace.empty() );
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
            if ( !extension( succs[ i ] ).seen ) {
                goDown( i );
                return true;
            }
        }

        if ( trace.empty() )
            return false;

        goBack();
        return true;
    }

    // print successors
    void printSuccessors( std::ostream& o ) {
        const int LINE = 40;
        unsigned int id = 0;
        for ( auto& s : succs ) {
            Node n = s.node();
            ++id;

            std::stringstream ss;
            ss << "== # " << id << " ";
            int chars = ss.str().size();
            o << ss.str();

            ss.str( "" );
            if ( extension( s ).seen )
                ss << " visited";
            if ( this->graph().isAccepting( n ) )
                ss << " accept";
            if ( this->graph().isGoal( n ) )
                ss << " goal";

            if ( !ss.str().empty() ) {
                ss << " ==";
                chars += ss.str().size();
            }
            chars = std::max( LINE - chars, 0 );
            o << std::string( chars, '=' ) << ss.str() << '\n';

            // print transition
            if ( printEdges && !trace.empty() ) {
                std::string edge = this->graph().showTransition( trace.back().node(), n, Label() );
                if ( !edge.empty() )
                    o << "=> " << edge << '\n';
            }

            // print successor
            o << this->graph().showNode( n ) << '\n';
        }

        if ( id )
            o << std::string( LINE, '=' ) << '\n';
        o << succs.size() << " successors" << std::endl;
    }

    void printCurrent( std::ostream& o ) {
        if ( !trace.empty() ) {
            o << this->graph().showNode( trace.back().node() ) << '\n';
        }
    }

    // called whenever the current state is changed
    void printUpdate() {
        if ( !trace.empty() ) {
            std::cerr << "-- you are here (and there are " << succs.size() << " successors):" << std::endl;
            printCurrent( std::cout );
        } else {
            std::cerr << "-- choose an initial state (" << succs.size() << " available)" << std::endl;
        }
    }

    bool runCommand( const std::string& cmd ) {
        if ( cmd.empty() )
            return last.empty() ? true : runCommand( last );

        last = cmd;

        wibble::Splitter splitter( "[ \t]*,[ \t]*", REG_EXTENDED );
        for ( auto part = splitter.begin( cmd ); part != splitter.end(); ++part ) {
            if ( *part == "q" ) {
                return false;
            } else if ( *part == "s" ) {
                printSuccessors( std::cerr );
            } else if ( *part == "b" ) {
                if ( !trace.empty() ) {
                    goBack();
                } else {
                    std::cerr << "Cannot go back" << std::endl;
                }
            } else if ( *part == "t" ) {
                for ( Vertex n : trace ) {
                    std::cerr << this->graph().showNode( n.node() ) << "\n";
                }
            } else if ( *part == "n" ) {
                if ( !stepDFS() )
                    std::cerr << "Cannot go back" << std::endl;
            } else if ( !part->empty() && isdigit( (*part)[0] ) ) {
                int i = std::atoi( part->c_str() );
                if ( i > 0 && i <= int( succs.size() ) ) {
                    goDown( i - 1 );
                } else if ( succs.empty() ) {
                    std::cerr << "Current state has no successor" << std::endl;
                } else {
                    std::cerr << "Enter a number between 1 and " << succs.size() << std::endl;
                }
            } else {
               std::cerr <<
                    "HELP:\n"
                    "  1,2,3 use numbers to select a sucessor\n"
                    "  s     list successors\n"
                    "  b     back to the prevoius state state\n"
                    "  t     show current trace\n"
                    "  n     go to the next unvisited successor or go up if there is none (DFS)\n"
                    "  q     exit\n"
                    "multiple commands can be separated by a comma\n"
                    << std::endl;
            }
            printUpdate();
        }
        return true;
    }

    void runInteractive() {
        generateInitials();
        printUpdate();

        std::string line;
        do {
            if ( autoSuccs ) {
                printSuccessors( std::cerr );
            }
            std::cerr << "> " << std::flush;
            line.clear();
            std::getline( std::cin, line );
        } while ( runCommand( line ) && std::cin.good() );
    }

    void run() {
        if ( inputTrace.empty() ) {
            runInteractive();
        } else {
            generateInitials();
            runCommand( inputTrace );
        }
    }

    Simulate( Meta m ) : Algorithm( m, sizeof( Extension ) ) {
        this->init( *this );
        printEdges = true;
        autoSuccs = false;
        inputTrace = m.input.trace;
    }
};

}
}

#endif // DIVINE_ALGORITHM_SIMULATE
