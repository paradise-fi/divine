/*
*  General graph - representation of Buchi automaton or Kripke structure
*
*  Milan Jaros - xjaros1@fi.muni.cz
*
*/

#ifndef DIVINE_KS_BA_GRAPH
#define DIVINE_KS_BA_GRAPH

#ifndef DOXYGEN_PROCESSING
#include <list>
#include <map>

//#include "deb/deb.hh"

using namespace std;

namespace divine {
#endif //DOXYGEN_PROCESSING

/* constants definying typ of strongly conected component */

/* SCC does not contains accepting node or SCC is trivial */
#define SCC_TYPE_0 0
/* SCC contains accepting node and cycle without acc. node */
#define SCC_TYPE_1 1
/* SCC contains acc. node and does not contains cycle without acc. node */
#define SCC_TYPE_2 2

typedef pair<int, int> two_nodes_t;
typedef list<two_nodes_t> list_two_nodes_t;

inline bool operator<(const two_nodes_t& T1, const two_nodes_t& T2)
{
	return((T1.first < T2.first) || ((T1.first == T2.first) &&
		(T1.second < T2.second)));
}

enum KS_BA_colors_t {WHITE, GRAY, BLACK};

struct KS_BA_node_t {
	int name;
	bool initial, accept;

	/* helping variables */
	KS_BA_colors_t col;
	int pom;

	KS_BA_node_t();
	KS_BA_node_t(const KS_BA_node_t& N);
	KS_BA_node_t(int N);
	virtual ~KS_BA_node_t();

	virtual list<KS_BA_node_t*> get_adj() const = 0;
};

/* main class - general graph */
class KS_BA_graph_t {
public:
	map<int, KS_BA_node_t*> node_list;

	int timer;

/* helping methods */
private:
	/* type of SCC C */
	int SCC_type(const list<int>& C);

protected:
	/* helping methods for not_emptiness() */
	bool dfs1(KS_BA_node_t *p_N, list<int>& S);
	bool dfs2(KS_BA_node_t *p_N, const list<int>& S, list<int>& C);
      
protected:
	void DFS_visit(KS_BA_node_t *p_N, list<int>& DFS_tree);
	void DFS_visit(KS_BA_node_t *p_N);
	

protected:
	/* method returns node with maximal KS_BA_node::pom */
	KS_BA_node_t* get_max_node();

	/* returns 'true' if and only if there exists path from *p_from to
	*p_to contains at least one transition */
	bool is_reachable(KS_BA_node_t *p_from, KS_BA_node_t *p_to);

public:
	KS_BA_graph_t();
	KS_BA_graph_t(const KS_BA_graph_t& G);
	virtual ~KS_BA_graph_t();

	virtual void clear();

	/* adding a node */
	virtual KS_BA_node_t* add_node(int name) = 0;
	/* returns pointer to the node; if node does not exist, returns 0
	*/
	KS_BA_node_t* find_node(int name);
	const KS_BA_node_t* find_node(int name) const;

	/* removing node */
	virtual bool del_node(int name);
	virtual bool del_node(KS_BA_node_t *p_N);

	/* set node 'name' as initial; if node doesn't exist, the node
	will be create and add to graph */
	KS_BA_node_t* set_initial(int name, bool b = true);
	/* set node 'name' as accepting */
	KS_BA_node_t* set_accept(int name, bool b = true);

	/* reading graph */
	list<KS_BA_node_t*> get_all_nodes() const;
	list<KS_BA_node_t*> get_init_nodes() const;
	list<KS_BA_node_t*> get_accept_nodes() const;

	int e_size() const; // number of transitions
	int n_size() const; // number of nodes

	/* writing graph on output */
	virtual void vypis(std::ostream& vystup,
		bool strict_output = false) const = 0;
	/* writing on 'cout' */
	virtual void vypis(bool strict_output = false) const = 0; 


	/* Transpozition of graph */
	virtual void transpose(KS_BA_graph_t& Gt) const = 0;

	/* Depth First Search: DFS_forest is list of list of nodes, where
		the nodes are in one DF-tree */
	void DFS(list<list<int> >& DFS_forest);
	/* only set variables KS_BA_node_t::pom to finish time */
	void DFS();

	/* compute strongly conected components */
	virtual void SCC(list<list<int> >& SCC_list) = 0;

	/* Vypocet SCC vcetne jejich typu; prvni polozka je typ
	(SCC_TYPE_i), druha je seznam uzlu patricich do dane komponenty */
	/* compute SCC with their types */
	virtual void SCCt(list<pair<int, list<int> > >& SCC_type_list);

	/* neprazdnost Buchiho automatu + "protipriklad"
	(u nekterych odvozenych trid muze byt jina implementace) */
	/* not emptiness of Buchi automaton + "counterexample" */
	virtual bool not_emptiness(list<int>& S);
};

#ifndef DOXYGEN_PROCESSING
} //END of namespace DVE

//#include "deb/undeb.hh"
#endif //DOXYGEN_PROCESSING

#endif
