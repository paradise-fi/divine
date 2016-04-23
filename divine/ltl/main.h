// -*- C++ -*- (c) 2011 Petr Rockai <me@mornfall.net>

#include <divine/ltl2ba/ltl.hh>
#include <divine/ltl2ba/alt_graph.hh>
#include <divine/ltl2ba/formul.hh>
#include <divine/ltl2ba/support_dve.hh>
#include <divine/ltl2ba/DBA_graph.hh>

#ifndef DIVINE_BUCHI_H
#define DIVINE_BUCHI_H

namespace divine {

void copy_graph(const ALT_graph_t& aG, BA_opt_graph_t& oG);
int ltl2buchi( int argc, char **argv );
BA_opt_graph_t buchi( std::string ltl, bool probabilistic = false );

}

#endif
