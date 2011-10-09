// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <limits.h>
#include <cstdlib>
#include <divine/algorithm/common.h>
#include <divine/visitor.h>
#include <divine/report.h>
#include <tools/combine.h> // for PipeThrough... FIXME
#include <wibble/sys/fs.h>
#include <wibble/list.h>

#ifndef DIVINE_DRAW_H
#define DIVINE_DRAW_H

namespace divine {

template< typename > struct Simple;

struct DrawConfig {
    divine::Config *super;
    int maxDistance;
    std::string output;
    std::string drawTrace;
    std::string render;
    bool labels;
    bool traceLabels;
};

template< typename G >
struct Draw : virtual algorithm::Algorithm, algorithm::AlgorithmUtils< G >
{
    typedef Draw< G > This;
    typedef typename G::Node Node;
    typedef typename G::Successors Successors;
    typedef DrawConfig Config;
	typedef typename algorithm::AlgorithmUtils< G >::Table Table;

    struct Extension {
        int distance;
        int serial;
    };

    G g;
    Node initial;
    int drawn, maxdist, serial;

    std::string dot, output, render, trace;
    bool labels;
    bool traceLabels;
    Table *intrace;
    std::set< std::pair< int, int > > intrace_trans;

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    visitor::ExpansionAction expansion( Node st )
    {
        bool limit = extension( st ).distance > maxdist;

        dotNode( st, limit );
        g.porExpansion( st );

        if ( limit )
            return visitor::IgnoreState;
        else
            return visitor::ExpandState;
    }

    visitor::TransitionAction transition( Node f, Node t )
    {
        if ( extension( t ).serial == 0 ) {
            if ( !intrace->has( t ) )
                extension( t ).serial = ++serial;
            else
                extension( t ) = extension( intrace->get( t ) );
        }

        if ( !f.valid() )
            return visitor::FollowTransition;

        std::string color;
        if ( intrace_trans.count( std::make_pair( extension( f ).serial, extension( t ).serial ) ) )
            color = "red";
        dotEdge( f, t, color );

        if ( extension( t ).distance == 0 )
           extension( t ).distance = INT_MAX;

        g.porTransition( f, t, 0 );
        extension( t ).distance = std::min( extension( t ).distance, extension( f ).distance + 1 );
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
            return g.showNode( n );
        if ( labels )
            return g.showNode( n );
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
        if ( g.isAccepting( n ) )
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
        if ( !color.empty() )
            str << " [color = " << color << "]";
        str << std::endl;
        dot += str.str();
    }

    // TODO: We leak some memory here, roughly linear to the trace size, i.e. not too bad
    void loadTrace() {
        intrace = this->makeTable();

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
            typename G::Successors s = wibble::list::drop( trans[ i ] - 1, g.successors( from ) );
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

        initial = g.initial();
        extension( initial ).serial = 1;
        extension( initial ).distance = 1;

        loadTrace();

        visitor::BFV< visitor::Setup< G, This, Table > >
            visitor( g, *this, &this->table() );

        do {
            g.queueInitials( visitor );
            visitor.processQueue();
        } while ( g.porEliminateLocally( this->table() ) );

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

    Draw( DrawConfig *c = 0 )
        : Algorithm( c->super, sizeof( Extension ) )
    {
        this->initPeer( &g, NULL, 0 ); // only one peer
        maxdist = c->maxDistance;
        output = c->output;
        render = c->render;
        trace = c->drawTrace;
        labels = c->labels;
        traceLabels = labels || c->traceLabels;
        drawn = 0;
        serial = 1;
    }

    Result run() {
        progress() << "  exploring... \t\t\t\t" << std::flush;
        draw();
        progress() << "   done" << std::endl;

        progress() << "  running graphviz... \t\t\t" << std::flush;
        graphviz();
        progress() << "   done" << std::endl;

        return result();
    }
};

}

#endif
