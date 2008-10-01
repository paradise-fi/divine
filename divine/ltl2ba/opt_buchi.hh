/*
*  Implementation of optimization of Buchi automaton
*
*  Milan Jaros - xjaros1@fi.muni.cz
*/

#ifndef DIVINE_OPT_BUCHI
#define DIVINE_OPT_BUCHI

#ifndef DOXYGEN_PROCESSING

#include "KS_BA_graph.hh"
#include "BA_graph.hh"
#include "formul.hh"

//#include "deb/deb.hh"

namespace divine {
#endif //DOXYGEN_PROCESSING

/* class for implementing methods for optimizing Buchi automata */
class BA_opt_graph_t : public BA_graph_t {
protected:

	/* 'true' if and only if the SCC is 'fixed formula ball' */
	bool is_ball(const list<int>& SCC_tree);

	/* 'true' if and only if there is a accepting node reachable form
		*p_N
		- in list L is "path" from *p_N to acc. node
	*/
	bool reach_accept(KS_BA_node_t* p_N, list<KS_BA_node_t*>& L);

public:
	BA_opt_graph_t();
	BA_opt_graph_t(const BA_graph_t& G);
	virtual ~BA_opt_graph_t();

	BA_opt_graph_t& operator=(const BA_graph_t& G);


	/* transformation to automaton with only one initial state */
	void to_one_initial();

	/* removing nodes where are unreachable from initial states */
	void remove_unreachable_nodes();

	/* removing nodes from acc. set, if that nodes does not lying on
		cycle */
	void remove_bad_accept();

	/* set nodes as accepting, if that nodes does not lying on cycle;
		- dual method for remove_bad_accept()
		- use only with another optimization (simulation
			reduction)
	*/
	void set_redundand_accept();

	/* removing "never accepting nodes" - nodes, where can't lying on
		(any) accepting run.
		This method also provides remove_bad_accept() !
	*/
	void remove_never_accept();

	/* Reduction of "fixed formula ball's"
		- the result automaton is G
	*/
	void fix_form_ball_reduction(BA_opt_graph_t& G);

	/* Bisimulation reduction
		- result automaton is G
	*/
	void basic_bisim_reduction(BA_opt_graph_t& G);

	/* Simulation reduction
		- result automaton is G
	*/
	void strong_fair_sim_reduction(BA_opt_graph_t& G);

	/* collection of optimization of Buchi automaton */
	void optimize(BA_opt_graph_t& G_opt, int stupen);
        

};

#ifndef DOXYGEN_PROCESSING
} //END of namespace DVE

//#include "deb/undeb.hh"
#endif //DOXYGEN_PROCESSING

#endif
