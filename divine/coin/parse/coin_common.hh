
#ifndef _COIN_COMMON_HH
#define _COIN_COMMON_HH

#define yyin coin_yyin
#define yyparse coin_yyparse
#define yyerror coin_yyerror
#define yylex coin_yylex

/* Max (recognizable) length of identifiers */
#define MAXLEN 64

#include <cstdio>
#include <cstdlib>

extern FILE *yyin;

int yyparse();

void yyerror(const char *);

int yylex();

// parser

#include <map>
#include <vector>
#include <string>
#include <queue>
#include <set>
#include <iostream>

#include "../bit_set.h"

typedef const std::string & name_t;


/* This is a generic symbol table */

template<typename T> class symtable {
  
  private:
    std::vector<T *> table;		// the actual table of pointers to objects
    std::map<std::string, int> mapper;	// the mapping from names to IDs
    int index;			// index to the end of table

  public:
    symtable() { table.clear(); mapper.clear(); index = 0; }
    ~symtable() {
		for (typename std::vector<T *>::iterator i = table.begin();
		    i != table.end(); i++) {
		  	delete *i;
		}
//		std::cerr << "symtable destroyed" << std::endl;
    }
    int add_new (name_t, T *);  // introduce new object to the table
    int find_id (name_t);	// get ID of a named object
    T * get (unsigned int);		// get an object according to its ID
    T * find (name_t name) { return get(find_id(name)); }	// just...
    T * operator[] (int id) { return get(id); }			// ...shortcuts
    int size () { return index; }
};

template<typename T>
int symtable<T>::add_new (name_t name, T * elem)
{
  if (mapper.find(name) == mapper.end())
  {
    table.push_back(elem);
    mapper[name] = index;
    return index++; // returns old value of index
  }
  else return -1; // object with that name already present in table
}

template<typename T>
int symtable<T>::find_id (name_t name)
{
  std::map<std::string, int>::iterator i = mapper.find(name);
  if (i != mapper.end()) return i->second; else return -1;
}

template<typename T>
T * symtable<T>::get (unsigned int id)
{
  if (id < table.size()) return table[id]; else return NULL;
}



/* the actual objects */

struct component_t {
  std::string name;
};

struct action_t {
  std::string name;
};

struct label_t {
  int fromc_id, toc_id;
  int act_id;
  int compare(const label_t & l2) const {
    // -1 *this < l2
    //  1 *this > l2
    //  0 *this == l2
	if (this->fromc_id < l2.fromc_id) return -1;
	if (this->fromc_id > l2.fromc_id) return +1;
	if (this->toc_id < l2.toc_id) return -1;
	if (this->toc_id > l2.toc_id) return +1;
	if (this->act_id < l2.act_id) return -1;
	if (this->act_id > l2.act_id) return +1;
    // equal
	return 0;	
  }
  bool operator==(const label_t & l2) const {
    return compare(l2) == 0;
  }
  bool operator!=(const label_t & l2) const {
   	return compare(l2) != 0;
  }

  bool operator<(const label_t & l2) const { 
    return compare(l2) == -1;
  }

  bool operator>(const label_t & l2) const { 
    return compare(l2) == 1;
  } 

  bool isInput() {
    return this->fromc_id < 0;
  }

  bool isOutput() {
    return this->toc_id < 0;
  }

  bool isInternal() {
    return !this->isInput() && !this->isOutput();
  }

  friend std::ostream& operator<< (std::ostream & out, const label_t & l) {
    out << '(' << l.fromc_id << ',' << l.act_id << ',' << l.toc_id << ')';
    return out;
  }
};

struct trans_t {
  label_t label;
  int tostate_id;

  bool isInput() {
      return label.isInput();
  }

  bool isOutput() {
      return label.isOutput();
  }

  bool isInternal() {
      return label.isInternal();
  }
};

struct coin_state_t {
  std::string name;
  std::vector<trans_t *> outtrans;

  bit_set_t * enAp;	// for C2 precomputation

  std::set<int> visible_trans_to; // for C2 (visibility predicate)

  bit_set_t * input_act;	// for C1 computation
  bit_set_t * output_act;

  std::vector<trans_t *> input_trans; //use std::multimap to be able to access transitions with certain act_id
  std::vector<trans_t *> output_trans; //std::vector of output transitions
  std::vector<trans_t *> internal_trans; //std::vector of input transitions

  coin_state_t() 
    : enAp(NULL), input_act(NULL), output_act(NULL) {
      outtrans.clear();
      input_trans.clear();
      output_trans.clear();
      internal_trans.clear();
  }

  ~coin_state_t() {
	for (std::vector<trans_t *>::iterator i = outtrans.begin();
	    	i != outtrans.end(); i++) {
	  	delete *i;
	}
	if (input_act) delete input_act;
	if (output_act) delete output_act;
	if (enAp) delete enAp;
  }

  void add_trans(trans_t * const t) {
      outtrans.push_back(t);

      if(t->isInput()) {
          input_trans.push_back(t);
      } else if(t->isOutput()) {
          output_trans.push_back(t);
      } else if(t->isInternal()) {
          internal_trans.push_back(t);
      }
  }
};

class aut_t {
  public:
    aut_t () {}
	aut_t (name_t s, bool prim) :
		is_restrict(true), aut_name(s), is_primitive(prim),
		all_input_act(NULL), all_output_act(NULL),
		parent(NULL), visited(false), is_system_component(false) {}
	~aut_t () {
		if (all_input_act) delete all_input_act;
		if (all_output_act) delete all_output_act;
	}

    void add_state(name_t s);

    void add_trans(name_t f, const label_t & l, name_t t);
    void set_init_state(name_t s);

    bool is_prim() { return is_primitive; }
    // if this is simple automaton

    bool is_Ap_visible_trans(int, int);

    std::vector<int> composed_of; // if it is composite
    bool is_restrict;
    std::set<label_t> restrictions;

//  private:
    std::string aut_name;
    int init_state;
    bool is_primitive;	// primitive or composed
    
    /* if primitive */
    symtable<coin_state_t> states;
    symtable<component_t> components;

    bit_set_t * all_input_act; // only for primitive automata
    bit_set_t * all_output_act;

    aut_t * parent;
    bool visited; // used to compute LCA
    bool is_system_component; // is reachable from system automaton?

    void setParent(aut_t * p) {
        this->parent = p;
    }
};

struct prop_trans_t {
  std::set<label_t> labels;
  bool any_label;
  int tostate_id;
  std::set<label_t> guard_true;
  std::set<label_t> guard_false;
};

struct prop_state {
  std::string name;
  std::vector<prop_trans_t*> outtrans;
};

class prop_t : public aut_t {
  public:
    prop_t() { act_trans = NULL; }
    prop_trans_t* act_trans;
    symtable<prop_state> prop_states;
    std::set<int> accepting_states;
    
};

// transition of whole system

struct sometrans {
  bool single; // single automaton trans or synchronization?
  int  aut_id;
  int  state_id;

  label_t label;

  int aut_id2;
  int state_id2;

  //id of the topmost automaton this transition belongs to,
  //which has already been processed in the composition
  int top_aut_id;

  bool prop;  // property
  int ps_id;
};

struct tree_node_t {
    bool nonempty_succs; // satisfies condition C0
    bool c2; // satisfies condition C2
    bool in_composition; // automaton is in I

    tree_node_t(): nonempty_succs(false), c2(false), in_composition(false) {}
};

/* the main symbol table */

class coin_system_t {
  
  private:
    symtable<aut_t> automata;
    symtable<component_t> components;
    symtable<action_t> actions;
    int act_aut_id;

    bool act_prop;
    bool acc_states;

    std::string last_state;

  public:

    int system_aut_id;
    prop_t * property;
    int state_size;
    symtable<aut_t> * get_automata_list();

	unsigned int act_count;

    std::set<int> int_Ap; // c(Ap')
    	// actions (action IDs) from interesting E(label) atomic propositions
    std::set<label_t> int_Act; // Act', interesting labels


    coin_system_t () : act_aut_id(-1), act_prop(false), property(NULL) {}

    ~coin_system_t () {}

    void automaton_begin(name_t s, bool prim = true); 
    void automaton_end();
    void add_state(name_t s);
    void set_init_state(name_t s);
    void add_trans(name_t f, const label_t & l, name_t t);
    void add_trans(const label_t & l, name_t s);
    void add_comp_name(name_t s);
    void set_system(name_t s);
    void set_restrict(bool r);
    void add_to_complist(name_t s);
    void add_to_restrict(const label_t & l);

    void prop_begin();
    void prop_end();
    void prop_trans_begin(name_t, name_t);
    void add_guard(const label_t & guard, bool guard_type);
    void accept_start();
    void any_label();

    label_t mklabel_int (name_t from, name_t act, name_t to);
    label_t mklabel_out (name_t from, name_t act);
    label_t mklabel_in  (name_t act, name_t to);
    
    std::string get_hier(int);

    void precompute();
};


extern coin_system_t * the_coin_system;

bool common_elem(const std::set<label_t> &, const std::set<label_t> &);
bool subset(const std::set<label_t> &, const std::set<label_t> &);


#endif
