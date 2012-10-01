// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <limits.h>
#include <cstdlib>
#include <divine/algorithm/common.h>
#include <divine/graph/visitor.h>
#include <divine/utility/report.h>
#include <tools/combine.h> // for PipeThrough... FIXME
#include <wibble/sys/fs.h>
#include <wibble/list.h>

#ifndef DIVINE_DRAW_H
#define DIVINE_DRAW_H

namespace divine {

template< typename > struct Simple;

template< typename Setup >
struct Draw : algorithm::Algorithm, algorithm::AlgorithmUtils< Setup >, visitor::SetupBase, Sequential
{
    typedef Draw< Setup > This;
    typedef typename Setup::Graph Graph;
    typedef typename Graph::Node Node;
    typedef typename Setup::Store Store;
    typedef typename Graph::Successors Successors;
    typedef This Listener;
    typedef NoStatistics Statistics;

    struct Extension {
        int distance;
        int serial;
    };

    Node initial;
    int drawn, maxdist, serial;

    std::string dot, output, render, trace;
    bool labels;
    bool traceLabels;

    HashSet< Node, algorithm::Hasher > *intrace;
    std::set< std::pair< int, int > > intrace_trans;

    int id() { return 0; }

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    static visitor::ExpansionAction expansion( This &t, Node st )
    {
        bool limit = t.extension( st ).distance > t.maxdist;

        t.dotNode( st, limit );
        t.graph().porExpansion( st );

        if ( limit )
            return visitor::IgnoreState;
        else
            return visitor::ExpandState;
    }

    static visitor::TransitionAction transition( This &draw, Node f, Node t )
    {
        if ( draw.extension( t ).serial == 0 ) {
            if ( !draw.intrace->has( t ) )
                draw.extension( t ).serial = ++draw.serial;
            else
                draw.extension( t ) = draw.extension( draw.intrace->get( t ) );
        }

        if ( !f.valid() )
            return visitor::FollowTransition;

        std::string color;
        if ( draw.intrace_trans.count(
                 std::make_pair( draw.extension( f ).serial,
                                 draw.extension( t ).serial ) ) )
            color = "red";
        draw.dotEdge( f, t, color );

        if ( draw.extension( t ).distance == 0 )
           draw.extension( t ).distance = INT_MAX;

        draw.graph().porTransition( f, t, 0 );
        draw.extension( t ).distance =
            std::min( draw.extension( t ).distance, draw.extension( f ).distance + 1 );
        return visitor::FollowTransition;
    }

    std::string escape( std::string s ) {
        char buf[ s.length() * 2 ];
        int i = 0, j = 0;
        while ( size_t( i ) < s.length() ) {
            char c = s[ i ++ ];
            if ( c == '\\' || c == '\n' || c == '"' )
                buf[ j++ ] = '\\';
            if ( c == '\n' )
                buf[ j++ ] = 'n';
            else
                buf[ j++ ] = c;
        }
        buf[ j ] = 0;
        return buf;
    }

    std::string label( Node n ) {
        if ( intrace->has( n ) && traceLabels )
            return this->graph().showNode( n );
        if ( labels )
            return this->graph().showNode( n );
        return "";
    }

    std::string color( Node n ) {
        if ( n == initial )
            return "magenta";
        if ( intrace->has( n ) )
            return "red";
        return "";
    }

    void dotNode( Node n, bool dashed = false ) {
        stringstream str;
        str << extension( n ).serial << " [";
        if ( !color( n ).empty() )
            str << " fillcolor = " << color( n ) << " style=filled ";
        if ( this->graph().isAccepting( n ) )
            str << "peripheries=2 ";

        if ( label( n ).empty() )
            str << "shape=circle ";
        else
            str << "shape=ellipse ";

        if ( dashed )
            str << "style=dashed ";

        if ( !label( n ).empty() )
            str << "label=\"" << escape( label( n ) ) << "\"";

        str << "]\n";
        dot += str.str();
    }

    void dotEdge( Node f, Node t, std::string color = "" ) {
        stringstream str;
        str << extension( f ).serial << " -> " << extension( t ).serial;
        std::string label;
        if ( labels )
            label = this->graph().showTransition( f, t );

        if ( !color.empty() || !label.empty()) {
            str << " [";
            if (!color.empty()) {
                str << "color = " << color;
                if (!label.empty())
                    str << ",";
            }
            if (!label.empty())
                str << "label = \"" << label << "\"";
            str << "]";
        }
        str << std::endl;
        dot += str.str();
    }

    // TODO: We leak some memory here, roughly linear to the trace size, i.e. not too bad
    void loadTrace() {
        intrace = new HashSet< Node, algorithm::Hasher >( this->store().hasher() );

        if ( trace.empty() )
            return;

        std::istringstream split(trace);
        std::vector<int> trans;
        for( std::string each;
             std::getline( split, each, ',' );
             trans.push_back( ::atoi( each.c_str() ) ) ) ;

        Node from = initial;
        for ( int i = 0; size_t( i ) < trans.size(); ++ i ) {
            if ( intrace->get( from ).valid() )
                from = intrace->get( from );
            else
                intrace->insert( from );
            Successors s = wibble::list::drop( trans[ i ] - 1, this->graph().successors( from ) );
            Node to = intrace->get( s.head() );
            if ( !to.valid() ) {
                to = s.head();
                intrace->insert( to );
            }
            if ( !extension( to ).serial )
                extension( to ).serial = ++serial;
            extension( to ).distance = 1;
            intrace_trans.insert( std::make_pair( extension( from ).serial,
                                                  extension( to ).serial ) );
            from = to;
        }
    }

    void draw() {
        dot += "digraph {";

        initial = this->graph().initial();
        extension( initial ).serial = 1;
        extension( initial ).distance = 1;

        loadTrace();

        visitor::BFV< This >
            visitor( *this, this->graph(), this->store() );

        do {
            this->graph().queueInitials( visitor );
            visitor.processQueue();
        } while ( this->graph().porEliminateLocally( *this ) );

        dot += "}";
    }

    void graphviz() {
        if ( output.empty() ) {
            if ( render.empty() )
                render = "dot -Tx11";
            PipeThrough p( render );
            p.run( dot );
        } else {
            if ( render.empty() )
                render = "dot -Tpdf";
            PipeThrough p( render );
            wibble::sys::fs::writeFile( output, p.run( dot ) );
        }
    }

    Draw( Meta m, bool = false )
        : Algorithm( m, sizeof( Extension ) )
    {
        this->init( this );
        maxdist = m.algorithm.maxDistance;
        output = m.output.file;
        render = m.output.filterProgram;
        trace = m.input.trace;
        labels = m.algorithm.labels;
        traceLabels = labels || m.algorithm.traceLabels;
        drawn = 0;
        serial = 1;
    }

    void run() {
        progress() << "  exploring... \t\t\t\t" << std::flush;
        draw();
        progress() << "   done" << std::endl;

        progress() << "  running graphviz... \t\t\t" << std::flush;
        graphviz();
        progress() << "   done" << std::endl;
    }
};

}

#endif
