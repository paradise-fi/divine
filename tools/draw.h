// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>
//             (c) 2013 Vladimír Štill <xstill@fi.muni.cz>

#include <limits.h>
#include <cstdlib>

#include <brick-fs.h>
#include <brick-process.h>
#include <brick-types.h>

#include <divine/algorithm/common.h>
#include <divine/utility/statistics.h>

#ifndef DIVINE_DRAW_H
#define DIVINE_DRAW_H

namespace divine {

template< typename > struct Simple;

template< typename Setup >
struct Draw : algorithm::Algorithm, algorithm::AlgorithmUtils< Setup, brick::types::Unit >,
              visitor::SetupBase, Sequential
{
    typedef Draw< Setup > This;
    typedef typename Setup::Graph Graph;
    typedef typename Graph::Node Node;
    typedef typename Graph::Label Label;
    typedef typename Setup::Store Store;
    typedef typename Store::Vertex Vertex;
    typedef typename Store::Handle Handle;
    typedef This Listener;
    typedef NoStatistics Statistics;

    struct Extension {
        int initial:31;
        bool intrace:1;
        int distance;
        int serial;
    };

    int drawn, maxdist, currentdist, serial;

    std::string dot_nodes, dot_edges, output, render, trace;
    bool labels;
    bool traceLabels;
    bool bfs;

    std::set< std::pair< int, int > > intrace_trans;

    int id() { return 0; }

    Extension &extension( Vertex n ) {
        return n.template extension< Extension >();
    }

    Pool& pool() {
        return this->graph().pool();
    }

    static visitor::ExpansionAction expansion( This &t, Vertex st )
    {
        bool limit = t.extension( st ).distance > t.maxdist;

        t.dotNode( st, limit );

        if ( limit )
            return visitor::ExpansionAction::Ignore;
        else
            return visitor::ExpansionAction::Expand;
    }

    static visitor::TransitionAction transition( This &draw, Vertex f, Vertex t, Label l )
    {
        if ( draw.extension( t ).serial == 0 && !draw.extension( t ).intrace )
                draw.extension( t ).serial = ++draw.serial;

        if ( draw.extension( t ).initial == 1 ) {
            draw.extension( t ).initial ++;
            return visitor::TransitionAction::Expand;
        }

        auto res = visitor::TransitionAction::Follow;

        std::string color;
        if ( draw.intrace_trans.count(
                 std::make_pair( draw.extension( f ).serial,
                                 draw.extension( t ).serial ) ) ) {
            color = "red";
            res = visitor::TransitionAction::Expand;
        }
        draw.dotEdge( f, t, l, color);

        if ( draw.extension( t ).distance == 0 )
           draw.extension( t ).distance = INT_MAX;

        draw.extension( t ).distance =
            std::min( draw.extension( t ).distance, draw.extension( f ).distance + 1 );
        return res;
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

    std::string label( Vertex n ) {
        if ( extension( n ).intrace && traceLabels )
            return this->graph().showNode( n.node() );
        if ( labels )
            return this->graph().showNode( n.node() );
        return "";
    }

    std::string color( const Vertex &n ) {
        if ( extension( n ).initial )
            return "magenta";
        if ( extension( n ).intrace ) {
            if ( this->graph().isGoal( n.node() ) )
                return "orange";
            return "red";
        }
        if ( this->graph().isGoal( n.node() ) )
            return "yellow";
        return "";
    }

    void dotNode( const Vertex &n, bool dashed = false ) {
        std::stringstream str;

        if ( bfs && extension( n ).distance > currentdist ) {
            str << "} { rank = same; ";
            currentdist = extension( n ).distance;
        }

        str << extension( n ).serial << " [";
        if ( !color( n ).empty() )
            str << " fillcolor = " << color( n ) << " style=filled ";
        if ( this->graph().isAccepting( n.node() ) )
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

    void dotEdge( Vertex f, Vertex t, Label a, std::string color = "") {
        std::stringstream str;
        str << extension( f ).serial << " -> " << extension( t ).serial;
        std::string label;

        if ( labels )
            label = escape( this->graph().showTransition(
                                f.node(), t.node(), a ) );

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

    void loadTrace() {
        if ( trace.empty() )
            return;

        std::istringstream split(trace);
        std::vector<int> trans;
        for( std::string each;
             std::getline( split, each, ',' );
             trans.push_back( ::atoi( each.c_str() ) ) ) ;

        Vertex from, to;
        this->graph().initials( [&]( Node, Node n, Label ) {
                if ( trans.front() == 1 )
                    from = this->store().store( n );
                trans.front() --;
            } );

        ASSERT( this->store().valid( from ) );

        for ( int i = 1; size_t( i ) <= trans.size(); ++ i ) {
            extension( from ).intrace = true;

            if ( i == int( trans.size() ) ) /* done */
                break;

            int drop = trans[ i ] - 1;
            this->graph().successors( from, [&]( Node n, Label ) {
                    if ( drop > 0 ) {
                        -- drop;
                        return;
                    }
                    if ( !this->store().valid( to ) )
                        to = this->store().store( n );
                } );

            if ( !this->store().valid( to ) )
                throw std::logic_error(
                    "The trace " + trace + " is invalid, not enough successors "
                    "at step " + brick::string::fmt( i ) + " (" + brick::string::fmt( trans[ i ] ) + " requested)" );
            if ( !extension( to ).serial )
                extension( to ).serial = ++serial;
            extension( to ).distance = 1;
            intrace_trans.insert( std::make_pair( extension( from ).serial,
                                                  extension( to ).serial ) );
            from = to;
            to = Vertex();
        }
    }

    void draw() {

        visitor::BFV< This >
            visitor( *this, this->graph(), this->store() );

        this->graph().initials( [ this, &visitor ]( Node /*f*/, Node t, Label l ) {
                auto v = this->store().store( t );
                this->extension( v ).serial = ++this->serial;
                this->extension( v ).distance = 1;
                this->extension( v ).initial = 1;
                this->extension( v ).intrace = false;
                visitor.queue( Vertex(), v->node(), l );
            } );

        loadTrace();

        visitor.processQueue();

        while ( this->graph().porEliminateLocally( *this ) ) {
            this->graph().porExpand(
                this->store(), [ this, &visitor ]( Vertex f, Node t, Label l ) {
                    visitor.queue( f, t, l );
                } );
            visitor.processQueue();
        }

        if ( bfs )
            dot_nodes = "{" + dot_nodes + "}";
    }

    void graphviz() {
        std::string dot = "digraph { node [ fontname = Courier ]\n" + dot_nodes + "\n" + dot_edges + "\n}";
        if ( output.empty() ) {
            if ( render.empty() )
                render = "dot -Tx11";
            brick::process::PipeThrough p( render );
            p.run( dot );
        } else {
            if ( render.empty() )
                render = "dot -Tpdf";
            brick::process::PipeThrough p( render );
            brick::fs::writeFile( output, p.run( dot ) );
        }
    }

    Draw( Meta m, bool = false )
        : Algorithm( m, sizeof( Extension ) )
    {
        this->init( *this );
        maxdist = m.algorithm.maxDistance;
        if ( maxdist <= 0 )
            throw std::logic_error(
                "The --distance specified (" + brick::string::fmt( maxdist ) +
                ") is too small, must be at least 1." );
        output = m.output.file;
        render = m.output.filterProgram;
        trace = m.input.trace;
        labels = m.algorithm.labels;
        traceLabels = labels || m.algorithm.traceLabels;
        drawn = 0;
        serial = 0;
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
