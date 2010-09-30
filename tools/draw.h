// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#include <limits.h>
#include <divine/algorithm/common.h>
#include <divine/visitor.h>
#include <divine/report.h>
#include <tools/combine.h> // for PipeThrough... FIXME
#include <wibble/sys/fs.h>

#ifndef DIVINE_DRAW_H
#define DIVINE_DRAW_H

namespace divine {

template< typename > struct Simple;

template< typename G >
struct Draw : algorithm::Algorithm
{
    typedef Draw< G > This;
    typedef typename G::Node Node;
    typedef typename G::Successors Successors;

    struct Extension {
        int distance;
        int serial;
    };

    G g;
    Node initial;
    int drawn, maxdist, serial;

    std::string dot, output;

    Extension &extension( Node n ) {
        return n.template get< Extension >();
    }

    visitor::ExpansionAction expansion( Node st )
    {
        if ( st == initial )
            dotNode( st, "red" );
        else
            dotNode( st, "black" );
        return visitor::ExpandState;
    }

    visitor::TransitionAction transition( Node f, Node t )
    {
        if ( extension( t ).serial == 0 )
            extension( t ).serial = ++serial;

        dotEdge( f, t, "red" );

        if ( extension( t ).distance == 0 )
           extension( t ).distance = INT_MAX;

        extension( t ).distance = std::min( extension( t ).distance, extension( f ).distance + 1 );
        if ( maxdist > extension( t ).distance )
            return visitor::FollowTransition;
        else {
            dotNode( t, "black", true );
            return visitor::IgnoreTransition;
        }
    }

    void dotNode( Node n, std::string color, bool dashed = false ) {
        stringstream str;
        str << extension( n ).serial << " [ color = \"" << color << "\" ";
        if ( g.isAccepting( n ) )
            str << "shape=\"doublecircle\" ";
        else
            str << "shape=\"circle\" ";
        if ( dashed )
            str << "style=\"dashed\" ";
        str << "]\n";
        dot += str.str();
    }

    void dotEdge( Node f, Node t, std::string color ) {
        stringstream str;
        str << extension( f ).serial << " -> " << extension( t ).serial << "\n";
        dot += str.str();
    }

    void draw() {
        dot += "digraph {";

        initial = g.initial();
        extension( initial ).serial = 1;
        extension( initial ).distance = 1;

        visitor::BFV< visitor::Setup< G, This, Table > >
            visitor( g, *this, &table() );
        visitor.exploreFrom( initial );

        dot += "}";
    }

    void graphviz() {
        if ( output.empty() ) {
            PipeThrough p( "dot -Tx11" );
            p.run( dot );
        } else {
            PipeThrough p( "dot -Tpdf" );
            wibble::sys::fs::writeFile( output, p.run( dot ) );
        }
    }

    Draw( Config *c = 0 )
        : Algorithm( c, sizeof( Extension ) )
    {
        initGraph( g );
        maxdist = c->maxDistance;
        output = c->output;
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
