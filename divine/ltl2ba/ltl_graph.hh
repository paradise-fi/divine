/*
*  Transformation LTL formulae to Buchi automata - variant 1
*    - class for implementing transformation
*
*  Milan Jaros - xjaros1@fi.muni.cz
*
*/

#ifndef DIVINE_LTL_GRAPH
#define DIVINE_LTL_GRAPH

#ifndef DOXYGEN_PROCESSING

#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <set>
#include <map>

#include "formul.hh"

//#include "deb/deb.hh"

namespace divine {

#endif //DOXYGEN_PROCESSING

//typedef list<int> LTL_list_int_t;

typedef set<int> LTL_acc_set_t; // type - set of accept states
// type - set of set of accept states
typedef vector<LTL_acc_set_t> LTL_set_acc_set_t; 

/* node of Generalized Buchi automaton */
struct ltl_gba_node_t {
	int name;
	set<int> incoming;
	list<LTL_formul_t> set_new, set_old, set_next;

	void get_label(list<LTL_formul_t>& L) const;
	void get_label(list<LTL_literal_t>& L) const;

	bool is_initial() const;

	bool operator==(int node_name) const
		{ return(name == node_name); }
	friend bool operator==(int node_name, const ltl_gba_node_t& N)
		{ return(node_name == N.name); }
	bool operator!=(int node_name) const
		{ return(name != node_name); }
	friend bool operator!=(int node_name, const ltl_gba_node_t& N)
		{ return(node_name != N.name); }
};

/* node of Buchi automaton */
struct ltl_ba_node_t {
	int name;
	int old_name, citac; // node is pair <old_name, citac (counter)>
	LTL_label_t n_label;
	bool initial, accept;
	list<ltl_ba_node_t*> adj;
};

/* main class */
class ltl_graph_t {
private:
	/* GBA */
	list<ltl_gba_node_t> node_set; // list of nodes of GBA
	LTL_set_acc_set_t set_acc_set; // set of accepting sets

	/* BA */
	map<int, ltl_ba_node_t*> ba_node_set;


	int timer;

	/* Helping methods for expand() */
	bool find_gba_node(const list<LTL_formul_t>& old,
		const list<LTL_formul_t>& next,
		list<ltl_gba_node_t>::iterator& p_N);

	/* expansion of node */
	void expand(ltl_gba_node_t& N);

	/* method for compute set of accepting set */
	void compute_set_acc_set(LTL_formul_t& F);

	/* removing nodes, who have not succesors */
	void remove_non_next_nodes();

	/* helping methods for translation GBA -> BA */
	void clear_BA();

	ltl_ba_node_t* add_ba_node(int name);

	void copy_nodes();

public:
	ltl_graph_t();
	ltl_graph_t(const ltl_graph_t& G);
	~ltl_graph_t();

	void clear();

	/* method for transformation LTL formula to GBA */
	void create_graph(LTL_formul_t& F);

	/* method for transformation GBA to BA */
	void degeneralize();

	ltl_graph_t& operator=(const ltl_graph_t& G);

	/* reading GBA - if node doesn't exists, methods will return
		'false'
	*/

	/* list of succesors of node */
	bool get_gba_node_adj(int name, list<int>& L) const;
	bool get_gba_node_adj(int name, list<ltl_gba_node_t>& L) const;

	/* node label */
	bool get_gba_node_label(int name, list<LTL_formul_t>& L) const;
	bool get_gba_node_label(int name, list<LTL_literal_t>& L) const;

	bool find_gba_node(int name, ltl_gba_node_t& N) const;

	/* initial nodes of GBA */
	void get_gba_init_nodes(list<int>& L) const;
	void get_gba_init_nodes(list<ltl_gba_node_t>& L) const;

	/* set of accepting sets
	set_acc_set_t = vector<set<int> >
	*/
	const LTL_set_acc_set_t& get_gba_set_acc_set() const;

	/* all nodes of GBA */
	const list<ltl_gba_node_t>& get_all_gba_nodes() const;
	void get_all_gba_nodes(list<int>& L) const;

	/* writing GBA on output */
	void vypis_GBA(std::ostream& vystup,
		bool strict_output = false) const;
	/* writing to 'cout' */
	void vypis_GBA(bool strict_output = false) const;


	/* initial nodes of BA */
	list<ltl_ba_node_t*> get_ba_init_nodes() const;

	/* accepting nodes of BA */
	list<ltl_ba_node_t*> get_ba_accept_nodes() const;

	/* all nodes of BA */
	const map<int, ltl_ba_node_t*>& get_all_ba_nodes() const;


	/* writing BA on output */
	void vypis_BA(std::ostream& vystup, bool strict_output = false,
		bool output_node_pair = false) const;
	void vypis_BA(bool strict_output = false,
		bool output_node_pair = false) const;
};

#ifndef DOXYGEN_PROCESSING
} //END of namespace DVE
//#include "deb/undeb.hh"
#endif //DOXYGEN_PROCESSING

#endif
