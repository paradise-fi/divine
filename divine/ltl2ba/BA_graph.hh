/*
*  Representation of Buchi automaton
*
*  Milan Jaros - xjaros1@fi.muni.cz
*
*/

#ifndef DIVINE_BA_GRAPH
#define DIVINE_BA_GRAPH

#ifndef DOXYGEN_PROCESSING

#include "KS_BA_graph.hh"
#include "formul.hh"

//#include "deb/deb.hh"

namespace divine {
#endif //DOXYGEN_PROCESSING

/* strength of Buchi automaton */
#define BA_STRONG 1 
#define BA_WEAK 2
#define BA_TERMINAL 3

//using namespace std;

struct BA_node_t;

/* transition of Buchi automaton */
struct BA_trans_t {
	LTL_label_t t_label;
	BA_node_t *target;
};

/* node/state of Buchi automaton */
struct BA_node_t : public KS_BA_node_t {
	list<BA_trans_t> adj;
       
	BA_node_t();
	BA_node_t(const BA_node_t& N); // do not copy 'adj'
	BA_node_t(int N);
	virtual ~BA_node_t();

	virtual list<KS_BA_node_t*> get_adj() const;
};

/* Buchi automaton */
class BA_graph_t : public KS_BA_graph_t {
protected:
//private:
	/* helping method */
	void kopiruj(const BA_graph_t& G);

public:
	BA_graph_t();
	BA_graph_t(const BA_graph_t& G);
	virtual ~BA_graph_t();

	/* adding node to automaton */
	virtual KS_BA_node_t* add_node(int name);

	/* adding transition to automaton
		- pointers must be pointers to BA_node_t
	*/
	/* adding transition n_from --(t_label)--> n_to */
	void add_trans(int n_from, int n_to, const LTL_label_t& t_label);

	/* adding transition *p_from --(t_label)--> n_to */
	void add_trans(KS_BA_node_t *p_from, int n_to,
		const LTL_label_t& t_label);

	/* adding transition *p_from --(t_label)--> *p_to */
	void add_trans(KS_BA_node_t *p_from, KS_BA_node_t *p_to,
		const LTL_label_t& t_label);

	/* removing nodes with related transitions */
	virtual bool del_node(int name);
	virtual bool del_node(KS_BA_node_t *p_N);

	/* get list of succesors (transitions) */
	const list<BA_trans_t>&
		get_node_adj(const KS_BA_node_t* p_N) const;
	list<BA_trans_t>& get_node_adj(KS_BA_node_t* p_N);

	BA_graph_t& operator=(const BA_graph_t& G);


	/* new implementations of virtual methods */

	/* transposition of graph/automaton */
	virtual void transpose(KS_BA_graph_t& Gt) const;

	/* strongly conected components */
	virtual void SCC(list<list<int> >& SCC_list);

	/* writing graph on output (in propriet format) */
	virtual void vypis(std::ostream& vystup,
		bool strict_output = false) const;
	virtual void vypis(bool strict_output = false) const;

        list<BA_trans_t> Complete(list<BA_trans_t> trans);
 
        bool DFS_s(KS_BA_node_t* p_N);

        bool is_semideterministic();
	/* returns strength of automaton: strong (BA_STRONG),
		weak (BA_WEAK), terminal (BA_TERMINAL)
	*/
	int get_aut_type();

};

#ifndef DOXYGEN_PROCESSING
} //END of namespace DVE

//#include "deb/undeb.hh"
#endif //DOXYGEN_PROCESSING

#endif
