/*
*  Transformation LTL formulae to Buchi automata - variant 2
*
*    - class for implementing transformation
*      - first is LTL formula translating to very weak alternating
*        co-Buchi automaton (VWAA)
*      - second phase is translating VWAA to generalized Buchi automaton
*        with accepting transitions (GBA)
*      - third and the last phase is translating GBA to Buchi automaton
*
*
*  Milan Jaros - xjaros1@fi.muni.cz
*/

#ifndef DIVINE_ALT_GRAPH
#define DIVINE_ALT_GRAPH

#ifndef DOXYGEN_PROCESSING

#include <list>
#include <map>

#include "formul.hh"

//#include "deb/deb.hh"

namespace divine {
#endif //DOXYGEN_PROCESSING

/* Alternating automaton */

struct ALT_vwaa_node_t;

/* transition of alternating automaton */
struct ALT_vwaa_trans_t {
	LTL_label_t t_label; /* conjunction of literals;
		empty conjunction = true */
	list<ALT_vwaa_node_t*> target;

	void clear();
	void op_times(ALT_vwaa_trans_t& T1, ALT_vwaa_trans_t& T2);

	bool operator==(const ALT_vwaa_trans_t& T) const;
	bool operator!=(const ALT_vwaa_trans_t& T) const;
};

/* node of alternating automaton */
struct ALT_vwaa_node_t {
	int name; // number of node
	LTL_formul_t label; // name of node
	list<ALT_vwaa_trans_t> adj;

	bool pom; // helping variable

	bool is_accept();
};

/* Generalized Buchi automaton */

struct ALT_gba_node_t;

/* transition of GBA */
struct ALT_gba_trans_t {
	LTL_label_t t_label; // konjunkce literalu
	ALT_gba_node_t *p_from, *p_to;
};

/* node of GBA */
struct ALT_gba_node_t {
	int name;
	list<ALT_vwaa_node_t*> node_label; // conjuction of states
	list<ALT_gba_trans_t*> adj;

	ALT_gba_node_t() { }
	~ALT_gba_node_t(); // also delete transitions
};

typedef pair<list<list<ALT_vwaa_node_t*> >, ALT_gba_node_t*>
ALT_eq_gba_nodes_t;
typedef list< pair<list<list<ALT_vwaa_node_t*> >, ALT_gba_node_t*> >
ALT_list_eq_gba_nodes_t;

/* Buchi automaton */

struct ALT_ba_node_t;

/* transition of BA */
struct ALT_ba_trans_t {
	LTL_label_t t_label;
	ALT_ba_node_t *target;
};

/* node of BA */
struct ALT_ba_node_t {
	int name;
	int citac;
	ALT_gba_node_t *p_old;
	list<ALT_ba_trans_t> adj;
};

/* main class */
class ALT_graph_t {
private:
	/* alternating automaton */
	map<int, ALT_vwaa_node_t*> vwaa_node_list;
	list<list<ALT_vwaa_node_t*> > vwaa_init_nodes;
	list<ALT_vwaa_node_t*> vwaa_accept_nodes;

	/* GBA */
	list<ALT_gba_node_t*> gba_node_list;
	list<list<ALT_gba_trans_t*> > acc_set;
	list<ALT_gba_node_t*> gba_init_nodes;
	/* equivalent nodes */
	ALT_list_eq_gba_nodes_t gba_equiv_nodes;

	/* Buchi automaton */
	list<ALT_ba_node_t*> ba_node_list;
	list<ALT_ba_node_t*> ba_init_nodes, ba_accept_nodes;

	int timer;
	bool simp_GBA; // simplifying GBA (on-the-fly)

	/* Methods for copying graph */
	void copy_vwaa(const ALT_graph_t& G);
	void copy_GBA(const ALT_graph_t& G);
	void copy_BA(const ALT_graph_t& G);

	ALT_vwaa_node_t* find_vwaa_node(int name);
	ALT_gba_node_t* find_gba_node(int name);
	ALT_ba_node_t* find_ba_node(int name);

	void clear_vwaa();

	ALT_vwaa_node_t* find_vwaa_node(const LTL_formul_t& F);
	ALT_vwaa_node_t* add_vwaa_node(const LTL_formul_t& F, int n);
	ALT_vwaa_node_t* add_vwaa_node(const LTL_formul_t& F);

	void op_times(list<ALT_vwaa_trans_t>& J1,
		list<ALT_vwaa_trans_t>& J2,
		list<ALT_vwaa_trans_t>& JV);
	void op_sum(list<ALT_vwaa_trans_t>& J1,
		list<ALT_vwaa_trans_t>& J2,
		list<ALT_vwaa_trans_t>& JV);
	void op_over(LTL_formul_t& F, list<list<ALT_vwaa_node_t*> >& LV);

	void mkdelta(ALT_vwaa_node_t *p_N, list<ALT_vwaa_trans_t>& LV);
	void mkDELTA(LTL_formul_t& F, list<ALT_vwaa_trans_t>& LV);

	/* methods of  GBA */
	ALT_gba_node_t*
		find_gba_node(const list<ALT_vwaa_node_t*>& node_label);

	bool less_eq(ALT_vwaa_trans_t& T1, ALT_vwaa_trans_t& T2);
	bool is_acc_trans(ALT_vwaa_trans_t& T, ALT_vwaa_node_t *p_ac_N);
	bool is_acc_trans(ALT_gba_trans_t& T, ALT_vwaa_node_t *p_ac_N);

	bool tr_implies(ALT_gba_trans_t *p_T1, ALT_gba_trans_t *p_T2);

	ALT_gba_node_t* mknode(const list<ALT_vwaa_node_t*>& LN, bool& b);
		/* create node with LN; if new node - b = 'true';
		return pointer to the node */
	void mkdelta(ALT_gba_node_t *p_N,
		list<ALT_gba_node_t*>& new_nodes);

	/* removing redundant transitions from p_N->adj */
	void simp_gba_trans(ALT_gba_node_t *p_N);

	void clear_GB();

	/* Methods of  BA */
	ALT_ba_node_t* add_ba_node(ALT_gba_node_t *p_gN, int citac);
	void add_ba_trans(int t, ALT_gba_trans_t *p_gt,
		ALT_ba_trans_t& Tr);

	void clear_BA();

	void sim_BA_edges();

public:
	ALT_graph_t();
	ALT_graph_t(const ALT_graph_t& G);
	~ALT_graph_t();

	void clear();

	/* Methods for transformation LTL formula to Buchi automaton */
	void create_graph(LTL_formul_t& F); // Phase 1
	void transform_vwaa(); // Phase 2
	void degeneralize(); // Phase 3

	/* simplification */
	void simplify_GBA(); // zjednodusi GBA
	void simplify_BA();  // zjednodusi BA

	void simplify_on_the_fly(bool s = true);

	/* writing automata to 'cout' - in propriet format */
	void vypis_vwaa();
	void vypis_GBA();
	void vypis_BA();

	/* Methods for reading Buchi automaton */
	list<ALT_ba_node_t*> get_all_BA_nodes() const;
	list<ALT_ba_node_t*> get_init_BA_nodes() const;
	list<ALT_ba_node_t*> get_accept_BA_nodes() const;

	ALT_graph_t& operator=(const ALT_graph_t& G);
};

#ifndef DOXYGEN_PROCESSING
} //END of namespace DVE
//#include "deb/undeb.hh"
#endif //DOXYGEN_PROCESSING

#endif
