#include "coin_common.hh"

#include <iostream>
#include <map>

using namespace std;

#define DEB(x) //std::cerr << x << endl

string translist;
#define TL(x) (translist += (x))

string renamelist;
#define RL(x) (renamelist += (x))

coin_system_t * the_coin_system;

void aut_t::add_state(name_t s) 
{
  coin_state_t * tmp = new coin_state_t;
  tmp->name = s;
  tmp->outtrans.clear();
  int result = states.add_new(s, tmp);
  if (result == -1) { yyerror ("duplicate state name"); exit(1); }
}

void aut_t::set_init_state(name_t s)
{
  int result = states.find_id(s);
  if (result == -1) { 
    yyerror("initial state is an udeclared identifier");
    exit(1);
  }
  init_state = result;
}

void aut_t::add_trans(name_t f, const label_t & l, name_t t)
{
  int from = states.find_id(f);
  if (from == -1) 
    { yyerror("transition's 'from' state not declared"); exit(1); }
  int to = states.find_id(t);
  if (to == -1)
    { yyerror("transition's 'to' state not declared"); exit(1); }
  trans_t * tmp = new trans_t;
  tmp->label = l;
  tmp->tostate_id = to;
  states.get(from)->add_trans(tmp);
}

bool aut_t::is_Ap_visible_trans(int from_id, int to_id) {
  coin_state_t * fst = states[from_id];
  return (fst->visible_trans_to.find(to_id) != fst->visible_trans_to.end());
}


void coin_system_t::automaton_begin(name_t s, bool prim)
{
  aut_t * tmp = new aut_t(s, prim);

//  std::cerr << "adding " << s << endl;

  int result = automata.add_new(s, tmp);

  if (result == -1) { 
    yyerror ("another automaton with this name already declared");
    exit(1);
  }

  act_aut_id = result;

//  DEB("automaton_begin: " << s << " (id: " << result << ")");
}

void coin_system_t::automaton_end()
{
//  DEB("automaton_end id: " << act_aut_id);
  act_aut_id = -1;
}

void coin_system_t::add_state(name_t s)
{
  if (act_aut_id >= 0) 
  (automata.get(act_aut_id))->add_state(s);
  else if (act_prop) {
  
    if (!acc_states) {
      prop_state * tmp = new prop_state;
      tmp->name = s;
      int result = property->prop_states.add_new(s, tmp);
      if (result == -1) { yyerror ("property: duplicate state name"); exit(1);}
    }
    else
    {
      int result = property->prop_states.find_id(s);
      if (result == -1) { 
        yyerror ("property: accepting state undeclared");
        exit(1);
      }
      property->accepting_states.insert(result);
    }
  }
}

void coin_system_t::set_init_state(name_t s)
{
  if (act_aut_id >= 0) 
  (automata.get(act_aut_id))->set_init_state(s);
  else if (act_prop)
  {
    int result = property->prop_states.find_id(s);
    if (result == -1) { yyerror("property: initial state undeclared");exit(1);}
    property->init_state = result;
  }
}

void coin_system_t::add_trans(name_t f, const label_t & l, name_t t)
{
  if (act_aut_id >= 0) 
  (automata.get(act_aut_id))->add_trans(f, l, t);
  last_state = f;
}

void coin_system_t::add_trans(const label_t & l, name_t s)
{
  if (act_aut_id >= 0) 
  (automata.get(act_aut_id))->add_trans(last_state, l, s);
}

void coin_system_t::add_comp_name(name_t s)
{ 
  component_t * tmp = new component_t;
  tmp->name = s;
  components.add_new(s, tmp);
  if (act_aut_id >= 0)
    (automata.get(act_aut_id))->components.add_new(s, new component_t(*tmp));
}


void coin_system_t::set_system(name_t s)
{
  system_aut_id = automata.find_id(s);
  if (system_aut_id == -1) 
  { yyerror("undeclared automaton proposed as system"); exit(1); }
}

void coin_system_t::set_restrict(bool r)
{
  if (act_aut_id >= 0) 
  (automata.get(act_aut_id))->is_restrict = r;
}

void coin_system_t::add_to_complist(name_t s)
{
  int newid = automata.find_id(s);
  if (newid == -1) { yyerror ("automaton name not declared"); }
  (automata.get(act_aut_id))->composed_of.push_back(newid);
  (automata.find(s))->setParent(automata.get(act_aut_id));
}

void coin_system_t::add_to_restrict(const label_t & l)
{
  if (act_aut_id >= 0) 
  (automata.get(act_aut_id))->restrictions.insert(l);
  else if (act_prop)
  {
    // insert label into Act' (interesting actions)
	int_Act.insert(l);
    // ------
    // hack
    property->act_trans->labels.insert(l);
    property->act_trans->any_label = false;
  }
}

void coin_system_t::any_label()
{
  property->act_trans->any_label = true;
}

label_t coin_system_t::mklabel_int (name_t from, name_t act, name_t to)
{
  label_t tmp;
  tmp.fromc_id = components.find_id(from);
  if (tmp.fromc_id == -1) { yyerror ("unknown component name"); exit(1); }
  tmp.toc_id = components.find_id(to);
  if (tmp.toc_id == -1) { yyerror ("unknown component name"); exit(1); }
  tmp.act_id = actions.find_id(act);
  if (tmp.act_id == -1)
  {
    action_t * newa = new action_t;
    newa->name = act;
    tmp.act_id = actions.add_new(act, newa);
  }
  return tmp;
}

label_t coin_system_t::mklabel_out (name_t from, name_t act)
{
  label_t tmp;
  tmp.fromc_id = components.find_id(from);
  if (tmp.fromc_id == -1) { yyerror ("unknown component name"); exit(1); }
  tmp.toc_id = -1;
  tmp.act_id = actions.find_id(act);
  if (tmp.act_id == -1)
  {
    action_t * newa = new action_t;
    newa->name = act;
    tmp.act_id = actions.add_new(act, newa);
  }
  return tmp;
}

label_t coin_system_t::mklabel_in (name_t act, name_t to)
 {
  label_t tmp;
  tmp.fromc_id = -1;
  tmp.toc_id = components.find_id(to);
  if (tmp.toc_id == -1) { yyerror ("unknown component name"); exit(1); }
  tmp.act_id = actions.find_id(act);
  if (tmp.act_id == -1)
  {
    action_t * newa = new action_t;
    newa->name = act;
    tmp.act_id = actions.add_new(act, newa);
  }
  return tmp;
}
   
bool common_elem(const set<label_t> & a, const set<label_t> & b)
{
  set<label_t>::iterator i, j;
  i = a.begin();
  j = b.begin();
  while (i != a.end() && j != b.end())
  {
    if (*i == *j) return true;
    else if (*i < *j) i++;
    else if (*i > *j) j++;
  }
  return false;
}

bool subset(const set<label_t> & a, const set<label_t> & b)
{ // a \subseteq b
  set<label_t>::iterator i, j;
  i = a.begin();
  j = b.begin();
  while (i != a.end() && j != b.end())
  {
    if (*i == *j) { i++; j++; } 
    else if (*i < *j) return false; // v a je neco, co neni v b
    else if (*i > *j) j++; // v b je neco, co neni v a, to je ok
  }
  return (i == a.end()); // dosli jsme na konec a, je to ok
                         // jinak je v a neco, co neni v b -> false
}

string coin_system_t::get_hier(int aid)
{
  aut_t * tmp = automata.get(aid);
  string s;
  if (tmp->is_prim()) {
    s = "(";
    for (int i = 0; i < tmp->components.size(); i++)
    {
      if (i > 0) s += ",";
      s += tmp->components.get(i)->name;
    } 
    s += ")";
  } 
  else
  {
    s = "(";
    bool first = true;
    for (vector<int>::iterator i = tmp->composed_of.begin();
         i != tmp->composed_of.end(); i++)
    {
      if (!first) s += ",";
      first = false;
      s += get_hier(*i);
    }      
    s += ")";
  }
  return s;
}

void coin_system_t::prop_begin()
{
  act_prop = true; act_aut_id = -1;
  property = new prop_t;
  //std::cerr << "PROPERTY IS ON!" << endl;
  acc_states = false;
}

void coin_system_t::prop_end()
{
  act_prop = false;
}

void coin_system_t::precompute() {
  
  // we now precompute some por info
  act_count = actions.size();
  // all bitset of actions will be of act_count size

  //	the visibility relation for all transitions of all simple automata
 
  for (int i = 0; i < automata.size(); i++) {
    if (automata[i]->is_primitive) {
  // first, compute enAp, input_act, output_act	for all states 
  // and all_input_act, all_output_act 		for all automata
  	aut_t * aut = automata[i];

	aut->all_input_act = new bit_set_t(act_count);
	aut->all_output_act = new bit_set_t(act_count);

	for (int i = 0; i < aut->states.size(); i++) {

		coin_state_t * cst = aut->states[i];

		cst->enAp = new bit_set_t(act_count);
		cst->input_act = new bit_set_t(act_count);
		cst->output_act = new bit_set_t(act_count);

		for (vector<trans_t *>::iterator j = cst->outtrans.begin();
		    j != cst->outtrans.end(); j++) {
		  	// this is probably not optimal
		  	int act_id = (*j)->label.act_id;

			if (int_Ap.find(act_id) != int_Ap.end()) {
			  cst->enAp->insert(act_id);
			}	  

			if ((*j)->label.fromc_id == -1) {
			  // input
			  aut->all_input_act->insert((*j)->label.act_id);
			  cst->input_act->insert((*j)->label.act_id);
			} else if ((*j)->label.toc_id == -1) {
			  // output
			  aut->all_output_act->insert((*j)->label.act_id);
			  cst->output_act->insert((*j)->label.act_id);
			}
		}
	}	  

	// second, compare enAp for all transitions' from and to states
	for (int i = 0; i < aut->states.size(); i++) {
	  	 coin_state_t * cst = aut->states[i];
		 for (vector<trans_t *>::iterator j = cst->outtrans.begin();
		                         j != cst->outtrans.end(); j++) {
		   	coin_state_t * cst2 =
			  	aut->states[ (*j)->tostate_id ];
		   	if (*(cst->enAp) != *(cst2->enAp)) {
			  	cst->visible_trans_to.insert((*j)->tostate_id);
			} 
		 }
	}
    }
  }

}

void coin_system_t::accept_start()
{
  acc_states = true;
}

void coin_system_t::prop_trans_begin(name_t f, name_t t)
{
  int from = property->prop_states.find_id(f);
  if (from == -1) 
    { yyerror("property: transition's 'from' state not declared"); exit(1); }
  int to = property->prop_states.find_id(t);
  if (to == -1)
    { yyerror("property: transition's 'to' state not declared"); exit(1); }
  prop_trans_t * tmp = new prop_trans_t;
  tmp->tostate_id = to;
  (property->prop_states.get(from))->outtrans.push_back(tmp);
  property->act_trans = tmp;
}

void coin_system_t::add_guard(label_t const & l, bool type)
{
  // insert label into Ap' (interesting atomic propositions)
	int_Ap.insert(l.act_id);
  // this is overapproximation of c(Ap')
  // ------
  if (act_prop && property->act_trans != NULL)
  {
    if (type) {
      property->act_trans->guard_true.insert(l);
    }
    else {
      property->act_trans->guard_false.insert(l);
    }
  }
}

symtable<aut_t> * coin_system_t::get_automata_list() {
    return &automata;
}
