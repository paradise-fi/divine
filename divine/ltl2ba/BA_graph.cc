/*                                                                              
*  class for representation of Buchi automaton
*    - implementation of methods
*
*  Milan Jaros - xjaros1@fi.muni.cz
*
*/

#include <iterator>
#include <stdexcept>

#include "KS_BA_graph.hh"
#include "formul.hh"
#include "BA_graph.hh"

//#include "error/error.hh"
//#include "deb/deb.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
using namespace std;
#endif //DOXYGEN_PROCESSING

typedef map<int, KS_BA_node_t*> node_list_t;


BA_node_t::BA_node_t()
{ }

BA_node_t::BA_node_t(const BA_node_t& N):KS_BA_node_t(N)
{ }

BA_node_t::BA_node_t(int N):KS_BA_node_t(N)
{ }

BA_node_t::~BA_node_t()
{ }

list<KS_BA_node_t*> BA_node_t::get_adj() const
{
	list<KS_BA_node_t*> L;
	list<BA_trans_t>::const_iterator t_b, t_e, t_i;

	t_b = adj.begin(); t_e = adj.end();
	for (t_i = t_b; t_i != t_e; t_i++) {
		L.push_back(t_i->target);
	}

	return(L);
}


BA_graph_t::BA_graph_t()
{ }

BA_graph_t::BA_graph_t(const BA_graph_t& G):KS_BA_graph_t(G)
{
	kopiruj(G);
}

BA_graph_t::~BA_graph_t()
{
  //tento destruktor zabezpeci zmazanie list-u adj, ktory ma
  //BA_node_t navyse oproti KS_BA_node_t, ktoreho je potomkom
	map<int, KS_BA_node_t*>::iterator nl_b, nl_e, nl_i;

	nl_b = node_list.begin(); nl_e = node_list.end();
	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		(dynamic_cast<BA_node_t*>(nl_i->second))->adj.clear();
	}
}


void BA_graph_t::kopiruj(const BA_graph_t& G)
{
	node_list_t::const_iterator n_b, n_e, n_i;
	list<BA_trans_t>::const_iterator t_b, t_e, t_i;
	KS_BA_node_t *p_N;

	n_b = G.node_list.begin(); n_e = G.node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		p_N = add_node(n_i->first);
		p_N->initial = n_i->second->initial;
		p_N->accept = n_i->second->accept;
		t_b = G.get_node_adj(n_i->second).begin();
		t_e = G.get_node_adj(n_i->second).end();
		for (t_i = t_b; t_i != t_e; t_i++) {
			add_trans(p_N, t_i->target->name, t_i->t_label);
		}
	}
}

KS_BA_node_t* BA_graph_t::add_node(int name)
{
	KS_BA_node_t *p_N;

	p_N = find_node(name);
	if (p_N == 0) {
		p_N = new BA_node_t(name);
		node_list[name] = p_N;
	}

	return(p_N);
}

void BA_graph_t::add_trans(int n_from, int n_to,
	const LTL_label_t& t_label)
{
	KS_BA_node_t *p_N;

	p_N = add_node(n_from);
	add_trans(p_N, n_to, t_label);
}

void BA_graph_t::add_trans(KS_BA_node_t *p_from, int n_to,
	const LTL_label_t& t_label)
{
	KS_BA_node_t *p_N;

	p_N = add_node(n_to);
	add_trans(p_from, p_N, t_label);
}

void BA_graph_t::add_trans(KS_BA_node_t *p_from, KS_BA_node_t *p_to,
	const LTL_label_t& t_label)
{
	BA_node_t *p_N = dynamic_cast<BA_node_t*>(p_from);
	BA_trans_t Tr;

	Tr.target = dynamic_cast<BA_node_t*>(p_to);
	Tr.t_label = t_label;
	p_N->adj.push_back(Tr);
}


bool BA_graph_t::del_node(int name)
{
	node_list_t::iterator n_b, n_e, n_i, n_f;
	list<BA_trans_t>::iterator t_i, t_j;
	BA_node_t *p_N;

	n_b = node_list.begin(); n_e = node_list.end();
	n_f = node_list.find(name);
	if (n_f == n_e) return(false);

	for (n_i = n_b; n_i != n_e; n_i++) {
		if (n_i == n_f) continue;
		p_N = dynamic_cast<BA_node_t*>(n_i->second);
		t_i = p_N->adj.begin();
		while (t_i != p_N->adj.end()) {
			if (t_i->target->name == name) {
				t_j = t_i; t_i++;
				p_N->adj.erase(t_j);
			} else {
				t_i++;
			}
		}
	}

	delete n_f->second;
	node_list.erase(n_f);
	return(true);
}

bool BA_graph_t::del_node(KS_BA_node_t *p_N)
{
	if (p_N == 0) return(false);

	return(del_node(p_N->name));
}

const list<BA_trans_t>& BA_graph_t::get_node_adj(const KS_BA_node_t *p_N) const
{
	const BA_node_t *p_baN;

	p_baN = dynamic_cast<const BA_node_t*>(p_N);

	if (!p_baN) {
		//cerr << "Nelze prevest uzel" << p_N->name << endl;
		throw runtime_error("Ukazatel neni typu BA_node_t*");
	}

	return(p_baN->adj);
}

list<BA_trans_t>& BA_graph_t::get_node_adj(KS_BA_node_t *p_N)
{
	BA_node_t *p_baN;

	p_baN = dynamic_cast<BA_node_t*>(p_N);

	if (!p_baN) {
		//cerr << "Nelze prevest uzel" << p_N->name << endl;
		throw runtime_error("Ukazatel neni typu BA_node_t*");
	}

	return(p_baN->adj);
}

BA_graph_t& BA_graph_t::operator=(const BA_graph_t& G)
{
	clear();
	timer = G.timer;

	kopiruj(G);

	return(*this);
}

void BA_graph_t::transpose(KS_BA_graph_t& G) const
{
	BA_graph_t& Gt = dynamic_cast<BA_graph_t&>(G);
	node_list_t::const_iterator n_b, n_e, n_i;
	list<BA_trans_t>::const_iterator t_b, t_e, t_i;
	KS_BA_node_t *p_N;

	Gt.clear();
	n_b = node_list.begin(); n_e = node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		p_N = Gt.add_node(n_i->first);
		p_N->initial = n_i->second->initial;
		p_N->accept = n_i->second->accept;
		t_b = get_node_adj(n_i->second).begin();
		t_e = get_node_adj(n_i->second).end();
		for (t_i = t_b; t_i != t_e; t_i++) {
			Gt.add_trans(t_i->target->name, n_i->first,
				t_i->t_label);
		}
	}
}


void BA_graph_t::SCC(list<list<int> > & SCC_list)
{
	BA_graph_t Gt;
	node_list_t::iterator n_b, n_e, nt_b, nt_e, nt_i;
	KS_BA_node_t *p_N;
	list<int>  SCC_tree;
	list<int> ::iterator s_b, s_e, s_i;

	SCC_list.erase(SCC_list.begin(), SCC_list.end());

	BA_graph_t::transpose(Gt);
	DFS();

	n_b = node_list.begin(); n_e = node_list.end();
	nt_b = Gt.node_list.begin(); nt_e = Gt.node_list.end();
	Gt.timer = 1;

	for (nt_i = nt_b; nt_i != nt_e; nt_i++) {
		nt_i->second->col = WHITE;
	}

	while ((p_N = get_max_node()) != 0) {
		SCC_tree.push_back(p_N->name);
		Gt.DFS_visit(Gt.node_list[p_N->name], SCC_tree);
		SCC_list.push_back(SCC_tree);
		s_b = SCC_tree.begin(); s_e = SCC_tree.end();
		for (s_i = s_b; s_i != s_e; s_i++) {
			(node_list[*s_i])->pom = 0;
		}
		SCC_tree.erase(s_b, s_e);
	}
}

bool is_universal(list<BA_trans_t> A)
{
	list<BA_trans_t>::iterator a_b, a_e, a_i;
	list<LTL_label_t> LL;
	list<LTL_label_t>::iterator l_i, l_j, l_k;
	LTL_label_t L;
	LTL_literal_t lit;
	LTL_label_t::iterator lt_b, lt_e, lt_i;

	a_b = A.begin(); a_e = A.end();
	for (a_i = a_b; a_i != a_e; a_i++) {
		L = a_i->t_label;
		l_i = LL.begin();
		/* Invariant: \forall i, j : LL[i] \not \subset LL[j] &&
			LL[j] \not \subset LL[i]
		*/
		while (l_i != LL.end()) {
			if (LTL_subset_label(L, *l_i)) break;
			if (LTL_subset_label(*l_i, L)) {
				l_j = l_i; l_i++;
				LL.erase(l_j);
			} else {
				l_i++;
			}
		}
		if (l_i == LL.end()) {
			LL.push_back(L);
		}
	}

	bool b;

	l_i = LL.begin();
	while (l_i != LL.end()) {
		if (l_i->size() == 0) {
			l_k = l_i; l_i++;
			LL.erase(l_k); continue;
		}

		L.erase(L.begin(), L.end());
		lt_b = l_i->begin(); lt_e = l_i->end();
		for (lt_i = lt_b; lt_i != lt_e; lt_i++) {
			lit.negace = !lt_i->negace;
			lit.predikat = lt_i->predikat;
			L.push_back(lit);
		}

		b = false;

		l_j = LL.begin();
		while (l_j != LL.end()) {
			if (l_j == l_i) {
				l_j++; continue;
			}
			if (LTL_equiv_labels(L, *l_j)) {
				l_k = l_j; l_j++;
				LL.erase(l_k); b = true;
			} else {
				l_j++;
			}
		}

		if (b) {
			l_k = l_i; l_i++;
			LL.erase(l_k);
		} else {
			l_i++;
		}
	}

	return(LL.size() == 0);
}

int BA_graph_t::get_aut_type()
{
	list<pair<int, list<int> > > SCC_type_list;
	list<pair<int, list<int> > >::iterator ll_b, ll_e, ll_i;
	list<int> ::iterator l_b, l_e, l_i;
	KS_BA_node_t *p_N;

	std::size_t acc_count;
	list<BA_trans_t> A;
	list<BA_trans_t>::iterator a_b, a_e, a_i;

	SCCt(SCC_type_list);

	ll_b = SCC_type_list.begin(); ll_e = SCC_type_list.end();

	for (ll_i = ll_b; ll_i != ll_e; ll_i++) {
		acc_count = 0;
		l_b = ll_i->second.begin(); l_e = ll_i->second.end();
		for (l_i = l_b; l_i != l_e; l_i++) {
			p_N = find_node(*l_i);
			if (p_N->accept) acc_count++;
			if ((ll_i->first == SCC_TYPE_1) ||
				(ll_i->first == SCC_TYPE_2)) {
				p_N->pom = 1; // uzel je v akc. komp.
			} else {
				p_N->pom = 0; // v neakc. komp.
			}
		}
		if ( (acc_count != 0) &&
			(acc_count != ll_i->second.size()) ) {
			return(BA_STRONG);
		}
	}

	for (ll_i = ll_b; ll_i != ll_e; ll_i++) {
		if ((ll_i->first != SCC_TYPE_1) &&
			(ll_i->first != SCC_TYPE_2)) continue;
		l_b = ll_i->second.begin(); l_e = ll_i->second.end();
		for (l_i = l_b; l_i != l_e; l_i++) {
			p_N = find_node(*l_i);
			A = get_node_adj(p_N);
			a_b = A.begin(); a_e = A.end();
			for (a_i = a_b; a_i != a_e; a_i++) {
				if (a_i->target->pom == 0)
					return(BA_WEAK);
			}
			if (!is_universal(A)) return(BA_WEAK);
		}
	}

	return(BA_TERMINAL);
}


void BA_graph_t::vypis(ostream& vystup, bool strict_output) const
{
	node_list_t::const_iterator n_b, n_e, n_i;
	list<BA_trans_t>::const_iterator t_b, t_e, t_i;
	LTL_label_t::const_iterator f_b, f_e, f_i;
	const BA_node_t *p_N;
	bool b1, b2;

	int tr_count = 0, init_count = 0, acc_count = 0;

	n_b = node_list.begin(); n_e = node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		p_N = dynamic_cast<const BA_node_t*>(n_i->second);
		vystup << n_i->first;
		if (p_N->initial) {
			vystup << " init";
			init_count++;
		}
		if (p_N->accept) {
			vystup << " accept";
			acc_count++;
		}
		vystup << " : -> {";
		tr_count += p_N->adj.size();
		b1 = false;
		t_b = p_N->adj.begin(); t_e = p_N->adj.end();
		for (t_i = t_b; t_i != t_e; t_i++) {
			if (b1) vystup << ", ";
			else b1 = true;
			vystup << "(<";
			f_b = t_i->t_label.begin();
			f_e = t_i->t_label.end();
			b2 = false;
			for (f_i = f_b; f_i != f_e; f_i++) {
				if (b2) vystup << ", ";
				else b2 = true;
				vystup << *f_i;
			}
			vystup << "> - " << t_i->target->name << ")";
		}
		vystup << "}" << endl;
	}

	if (strict_output) vystup << "# ";
	vystup << "nodes: " << node_list.size() << "; accept: " <<
	acc_count << "; initial: " << init_count << "; transitions: " <<
	tr_count << ";" << endl;
}

void BA_graph_t::vypis(bool strict_output) const
{
	vypis(cout, strict_output);
}


list<BA_trans_t> BA_graph_t::Complete(list<BA_trans_t> trans)
 {
   list<BA_trans_t> complete_trans;
   list<LTL_label_t> old_labels,new_labels ;
   LTL_label_t all_literal;
   bool found = false;
   LTL_label_t::iterator all_literal_iter;
   for(list<BA_trans_t>::iterator i=trans.begin(); i!= trans.end();i++)
   {
    for(list<LTL_literal_t>::iterator j = (*i).t_label.begin();  j != (*i).t_label.end();j++)
    {
      for(list<LTL_literal_t>::iterator all_literal_iter = all_literal.begin();all_literal_iter != all_literal.end();all_literal_iter++)
       if ( (*all_literal_iter).predikat == (*j).predikat ) found = true;
      if (!found) all_literal.push_back((*j));      
    }
   }
   for(list<BA_trans_t>::iterator i=trans.begin(); i!= trans.end();i++)
   {
    list<LTL_literal_t> base = (*i).t_label;
    old_labels.clear();
    new_labels.clear();
    old_labels.push_back((*i).t_label);
    for(list<LTL_literal_t>::iterator all_literal_iter = all_literal.begin();all_literal_iter != all_literal.end();all_literal_iter++) 
    {
     found = false;
     for(list<LTL_literal_t>::iterator base_iter = base.begin(); base_iter != base.end();base_iter++)
      if ( (*base_iter).predikat == (*all_literal_iter).predikat ) found = true; 
     
     if (!found) //neni v bazi
      {
        LTL_literal_t new_literal_1, new_literal_2;
        new_literal_1.predikat = (*all_literal_iter).predikat;
        new_literal_1.negace = (*all_literal_iter).negace;
        new_literal_2.predikat = (*all_literal_iter).predikat;
        new_literal_2.negace = !((*all_literal_iter).negace);
        for(list<LTL_label_t>::iterator old_labels_iter = old_labels.begin(); old_labels_iter != old_labels.end(); old_labels_iter++)
         {
            LTL_label_t pom_1 = (*old_labels_iter);
 	    LTL_label_t pom_2 = (*old_labels_iter);
            pom_1.push_back(new_literal_1);
            pom_2.push_back(new_literal_2);
            new_labels.push_back(pom_1);
            new_labels.push_back(pom_2);
         }         
        old_labels = new_labels;
        
      }
    }
    BA_trans_t new_trans;
    for(list<LTL_label_t>::iterator old_labels_iter = old_labels.begin(); old_labels_iter != old_labels.end(); old_labels_iter++)
      {
        new_trans.t_label =  (*old_labels_iter);
        new_trans.target = (*i).target;
        complete_trans.push_back(new_trans); 
      }
   }
  return complete_trans;
 }


bool    BA_graph_t::DFS_s(KS_BA_node_t* p_N)
{
	list<BA_trans_t> trans = (*dynamic_cast<BA_node_t*>(p_N)).adj;
        list<BA_trans_t> complete_trans = Complete(trans);
        for(list<BA_trans_t>::iterator iter1 = complete_trans.begin(); iter1 != complete_trans.end(); iter1++)
        {
         for(list<BA_trans_t>::iterator iter2 = complete_trans.begin(); iter2 != complete_trans.end(); iter2++)  
          if ( (*iter1).t_label == (*iter2).t_label )
           if ( (*iter1).target != (*iter2).target )
            return false;
        } 
        
        list<KS_BA_node_t*>::iterator n_b, n_e, n_i;
	list<KS_BA_node_t*> L;
        
        p_N->col = GRAY;
        L = p_N->get_adj();
        n_b = L.begin(); n_e = L.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if ((*n_i)->col == WHITE) {
			if ( !DFS_s(*n_i)) return false;
		}
	}

	p_N->col = BLACK;
	p_N->pom = timer; timer++;
        return true;
}




bool BA_graph_t::is_semideterministic()
{
  KS_BA_node_t *node;
  list<KS_BA_node_t*> accepts;
  bool is_semiderministic;

  node_list_t::iterator n_b, n_e, n_i;
  n_b = node_list.begin(); n_e = node_list.end();
  for (n_i = n_b; n_i != n_e; n_i++) {
   n_i->second->col = WHITE;
  }

  accepts = get_accept_nodes();
  
  for(list<KS_BA_node_t*>::iterator i = accepts.begin();i != accepts.end();i++)
  {
   node = (*i);
   if ( (*node).col == WHITE ) is_semiderministic = DFS_s(node);
  }

  return is_semiderministic; 
}

