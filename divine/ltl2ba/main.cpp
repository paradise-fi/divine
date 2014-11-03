#include <divine/ltl2ba/main.h>
#include <sstream>

namespace divine {

// paste from ltl2ba.cc
void copy_graph(const ALT_graph_t& aG, BA_opt_graph_t& oG)
{
	std::list<ALT_ba_node_t*> node_list, init_nodes, accept_nodes;
	std::list<ALT_ba_node_t*>::const_iterator n_b, n_e, n_i;
	std::list<ALT_ba_trans_t>::const_iterator t_b, t_e, t_i;
	KS_BA_node_t *p_N;

	node_list = aG.get_all_BA_nodes();
	init_nodes = aG.get_init_BA_nodes();
	accept_nodes = aG.get_accept_BA_nodes();

	oG.clear();

	n_b = node_list.begin(); n_e = node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		p_N = oG.add_node((*n_i)->name);
		t_b = (*n_i)->adj.begin(); t_e = (*n_i)->adj.end();
		for (t_i = t_b; t_i != t_e; t_i++) {
			oG.add_trans(p_N, t_i->target->name,
				t_i->t_label);
		}
	}

	n_b = init_nodes.begin(); n_e = init_nodes.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		oG.set_initial((*n_i)->name);
	}

	n_b = accept_nodes.begin(); n_e = accept_nodes.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		oG.set_accept((*n_i)->name);
	}
}

BA_opt_graph_t buchi( std::string ltl, bool probabilistic )
{
    std::ostringstream str;
    LTL_parse_t L;
    LTL_formul_t F;
    ALT_graph_t G;
    BA_opt_graph_t oG, oG1;
    L.nacti( ltl );
    if ( !L.syntax_check( F ) )
        std::cerr << "Error: Syntax error in LTL formula: '" << ltl << "'." << std::endl;
    F = F.negace();
    G.create_graph( F );
    G.transform_vwaa();
    G.simplify_GBA();
    G.degeneralize();
    G.simplify_BA();
    copy_graph(G, oG);
    oG.to_one_initial();
    oG.optimize(oG1, 6);

    if ( oG1.is_empty() )
    {
        // XXX warning?
        oG1.clear();
        oG1.set_initial( 1 );
    }

    if ( probabilistic && !oG1.is_semideterministic() ) {
        DBA_graph_t DG;
        DG.semideterminization( oG1 );
        oG1 = DG.transform();
    }

    return oG1;
}

}
