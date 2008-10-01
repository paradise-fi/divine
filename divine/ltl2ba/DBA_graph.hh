/*                                                                              
*  class for representation of (semi)deterministic Buchi automaton
*    - implementation of methods
*
*  Milan Ceska - xceska@fi.muni.cz
*
*/

#ifndef DIVINE_DBA_GRAPH
#define DIVINE_DBA_GRAPH

#ifndef DOXYGEN_PROCESSING

#include "KS_BA_graph.hh"
#include "BA_graph.hh"
#include "opt_buchi.hh"
#include "formul.hh"
#include <set>

//#include "deb/deb.hh"

namespace divine {
#endif //DOXYGEN_PROCESSING

typedef pair<set<int>,set<int> > set_pair_t;
typedef map<int, KS_BA_node_t*> node_list_t;
typedef map<set_pair_t,int> set_pair_list_t;
typedef map<int,int> pure_state_to_state_t;

struct DBA_node_t;
class BA_opt_graph_t;

struct DBA_trans_t {
	LTL_label_t t_label;
	DBA_node_t *target;
};

struct DBA_node_t : public KS_BA_node_t {
       set_pair_t sets;
       int pure_name;
       list<DBA_trans_t> adj;

     
       DBA_node_t(int N,set_pair_t S);
       DBA_node_t(int N,int M);
	virtual ~DBA_node_t();

	virtual list<KS_BA_node_t*> get_adj() const;
       
};


class DBA_graph_t : public KS_BA_graph_t{
protected:

        set_pair_list_t set_pair_list;
        pure_state_to_state_t pure_state_to_state;
	
	public:
	
        DBA_graph_t();
	
	virtual ~DBA_graph_t();

        void clear();
       
        /* adding node to automaton */
        pair<DBA_node_t*,bool> add_node(set_pair_t S);
        pair<DBA_node_t*,bool> add_node_s(set_pair_t S);
        
        pair<DBA_node_t*,bool> add_pure_node(int N);
	
	/* adding transition *p_from --(t_label)--> *p_to */
	void add_trans(DBA_node_t *p_from, DBA_node_t *p_to,
		const LTL_label_t& t_label);

	
	
        list<DBA_trans_t>& get_node_adj(KS_BA_node_t* p_N);


	/* writing graph on output (in propriet format) */
	virtual void vypis(std::ostream& vystup,
		bool strict_output = false) const;
	virtual void vypis(bool strict_output = false) const;


        virtual KS_BA_node_t* add_node(int name);
        virtual void transpose(KS_BA_graph_t& Gt) const;
        virtual void SCC(list<list<int> >& SCC_list);

        //void determinization(BA_opt_graph_t G);
        
        BA_opt_graph_t transform();
              
        list<BA_trans_t> Complete(list<BA_trans_t> trans);    
        
        void semideterminization(BA_opt_graph_t G);

};

#ifndef DOXYGEN_PROCESSING
} //END of namespace DVE

//#include "deb/undeb.hh"
#endif //DOXYGEN_PROCESSING

#endif
