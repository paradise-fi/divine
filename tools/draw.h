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
    typedef typename Graph::Label Label;
    typedef typename Setup::Store Store;
    typedef This Listener;
    typedef NoStatistics Statistics;

    struct Extension {
        int distance;
        int serial;
    };

    Node initial;
    int drawn, maxdist, currentdist, serial;

    std::string dot_nodes, dot_edges, output, render, trace;
    bool labels;
    bool traceLabels;
    bool bfs;

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

    static visitor::TransitionAction transition( This &draw, Node f, Node t, Label l )
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
        draw.dotEdge( f, t, l, color);

        if ( draw.extension( t ).distance == 0 )
           draw.extension( t ).distance = INT_MAX;

        draw.graph().porTransition( f, t, 0 );
        draw.extension( t ).distance =
            std::min( draw.extension( t ).distance, draw.extension( f ).distance + 1 );
        return visitor::FollowTransition;
    }

    std::string escape( std::string s ) {
        std::string buf;
        buf.resize( s.length() * 2 );
        int i = 0, j = 0;
        while ( i < int( s.length() ) ) {
            char c = s[ i ++ ];
            if ( c == '\\' || c == '\n' || c == '"' )
                buf[ j++ ] = '\\';
            if ( c == '\n' )
                buf[ j++ ] = 'n';
            else
                buf[ j++ ] = c;
        }
        return std::string( buf, 0, j );
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
        std::stringstream str;

        if ( bfs && extension( n ).distance > currentdist ) {
            str << "} { rank = same; ";
            currentdist = extension( n ).distance;
        }

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
        dot_nodes += str.str();
    }

    void dotEdge( Node f, Node t, Label a, std::string color = "") {
        std::stringstream str;
        str << extension( f ).serial << " -> " << extension( t ).serial;
        std::string label;

        if ( labels )
            label = escape( this->graph().showTransition( f, t, a ) );

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
        dot_edges += str.str();
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

        Node from = initial, to;
        for ( int i = 0; size_t( i ) < trans.size(); ++ i ) {
            if ( intrace->get( from ).valid() )
                from = intrace->get( from );
            else
                intrace->insert( from );

            int drop = trans[ i ] - 1;
            this->graph().successors( from, [&]( Node n, Label ) {
                    if ( drop > 0 ) {
                        -- drop;
                        return;
                    }
                    to = intrace->get( n );
                    if ( !to.valid() ) {
                        to = n;
                        intrace->insert( to );
                    }
                } );

            if ( !extension( to ).serial )
                extension( to ).serial = ++serial;
            extension( to ).distance = 1;
            intrace_trans.insert( std::make_pair( extension( from ).serial,
                                                  extension( to ).serial ) );
            from = to;
            to = Blob();
        }
    }

    void draw() {
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

        if ( bfs )
            dot_nodes = "{" + dot_nodes + "}";
    }

    void graphviz() {
        std::string dot = "digraph { node [ fontname = Courier ]\n" + dot_nodes + "\n" + dot_edges + "\n}";
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
        currentdist = 0;
        bfs = m.algorithm.bfsLayout;
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
