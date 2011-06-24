/*
*  class representing general graph
*    - implementation of methods
*
*  Milan Jaros - xjaros1@fi.muni.cz
*
*/

#include <iterator>
#include <queue>
#include <algorithm>

#include "KS_BA_graph.hh"

//#include "error/error.hh"
//#include "deb/deb.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
using namespace std;
#endif //DOXYGEN_PROCESSING

typedef map<int, KS_BA_node_t*> node_list_t;


KS_BA_node_t::KS_BA_node_t():name(0), initial(false), accept(false)
{ }

KS_BA_node_t::KS_BA_node_t(const KS_BA_node_t& N):name(N.name),
initial(N.initial), accept(N.accept), col(N.col), pom(N.pom)
{ }

KS_BA_node_t::KS_BA_node_t(int N):name(N), initial(false), accept(false)
{ }

KS_BA_node_t::~KS_BA_node_t()
{ }

KS_BA_graph_t::KS_BA_graph_t():timer(0)
{ }

KS_BA_graph_t::KS_BA_graph_t(const KS_BA_graph_t& G):timer(G.timer)
{
	clear();
}

KS_BA_graph_t::~KS_BA_graph_t()
{
	clear();
}

void KS_BA_graph_t::clear()
{
	map<int, KS_BA_node_t*>::iterator nl_b, nl_e, nl_i;

	nl_b = node_list.begin(); nl_e = node_list.end();
	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		delete nl_i->second;
	}

	node_list.erase(nl_b, nl_e);
}

int KS_BA_graph_t::n_size() const
{
	return(node_list.size());
}

int KS_BA_graph_t::e_size() const
{
	map<int, KS_BA_node_t*>::const_iterator n_b, n_e, n_i;
	int count = 0;

	n_b = node_list.begin(); n_e = node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		count += n_i->second->get_adj().size();
	}

	return(count);
}

KS_BA_node_t* KS_BA_graph_t::find_node(int name)
{
	map<int, KS_BA_node_t*>::const_iterator n_e, n_f;

	n_e = node_list.end();
	n_f = node_list.find(name);
	if (n_f != n_e) {
		return(n_f->second);
	} else {
		return(0);
	}
}

const KS_BA_node_t* KS_BA_graph_t::find_node(int name) const
{
	map<int, KS_BA_node_t*>::const_iterator n_e, n_f;

	n_e = node_list.end();
	n_f = node_list.find(name);
	if (n_f != n_e) {
		return(n_f->second);
	} else {
		return(0);
	}
}

bool KS_BA_graph_t::del_node(int name)
{
	map<int, KS_BA_node_t*>::iterator n_e, n_f;

	n_e = node_list.end();
	n_f = node_list.find(name);
	if (n_f != n_e) {
		delete n_f->second;
		node_list.erase(n_f);
		return(true);
	} else {
		return(false);
	}
}

bool KS_BA_graph_t::del_node(KS_BA_node_t *p_N)
{
	if (p_N == 0) return(false);

	return(del_node(p_N->name));
}

KS_BA_node_t* KS_BA_graph_t::set_initial(int name, bool b)
{
	KS_BA_node_t *p_N;

	p_N = add_node(name);
	p_N->initial = b;
	return(p_N);
}

KS_BA_node_t* KS_BA_graph_t::set_accept(int name, bool b)
{
	KS_BA_node_t *p_N;

	p_N = add_node(name);
	p_N->accept = b;
	return(p_N);
}

list<KS_BA_node_t*> KS_BA_graph_t::get_all_nodes() const
{
	node_list_t::const_iterator n_b, n_e, n_i;
	list<KS_BA_node_t*> L;

	n_b = node_list.begin(); n_e = node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		L.push_back(n_i->second);
	}

	return(L);
}

list<KS_BA_node_t*> KS_BA_graph_t::get_init_nodes() const
{
	node_list_t::const_iterator n_b, n_e, n_i;
	list<KS_BA_node_t*> L;

	n_b = node_list.begin(); n_e = node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if (n_i->second->initial) L.push_back(n_i->second);
	}

	return(L);
}

list<KS_BA_node_t*> KS_BA_graph_t::get_accept_nodes() const
{
	node_list_t::const_iterator n_b, n_e, n_i;
	list<KS_BA_node_t*> L;

	n_b = node_list.begin(); n_e = node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if (n_i->second->accept) L.push_back(n_i->second);
	}

	return(L);
}


void KS_BA_graph_t::DFS_visit(KS_BA_node_t *p_N, list<int> & DFS_tree)
{
	list<KS_BA_node_t*> L;
	list<KS_BA_node_t*>::iterator n_b, n_e, n_i;

	p_N->col = GRAY;
	L = p_N->get_adj();
	n_b = L.begin(); n_e = L.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if ((*n_i)->col == WHITE) {
			DFS_tree.push_back((*n_i)->name);
			DFS_visit(*n_i, DFS_tree);
		}
	}

	p_N->col = BLACK;
	p_N->pom = timer; timer++;
}

void KS_BA_graph_t::DFS(list<list<int> > & DFS_forest)
{
	list<int>  DFS_tree;
	node_list_t::iterator n_b, n_e, n_i;

	DFS_forest.erase(DFS_forest.begin(), DFS_forest.end());
	n_b = node_list.begin(); n_e = node_list.end();

	for (n_i = n_b; n_i != n_e; n_i++) {
		n_i->second->col = WHITE;
	}
	timer = 1;

	for (n_i = n_b; n_i != n_e; n_i++) {
		if (n_i->second->col == WHITE) {
			DFS_tree.erase(DFS_tree.begin(), DFS_tree.end());
			DFS_tree.push_back(n_i->first);
			DFS_visit(n_i->second, DFS_tree);
			DFS_forest.push_back(DFS_tree);
		}
	}
}

void KS_BA_graph_t::DFS_visit(KS_BA_node_t* p_N)
{
	list<KS_BA_node_t*>::iterator n_b, n_e, n_i;
	list<KS_BA_node_t*> L;

	p_N->col = GRAY;
	L = p_N->get_adj();
	n_b = L.begin(); n_e = L.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if ((*n_i)->col == WHITE) {
			DFS_visit(*n_i);
		}
	}

	p_N->col = BLACK;
	p_N->pom = timer; timer++;
}

void KS_BA_graph_t::DFS()
{
	node_list_t::iterator n_b, n_e, n_i;

	n_b = node_list.begin(); n_e = node_list.end();

	for (n_i = n_b; n_i != n_e; n_i++) {
		n_i->second->col = WHITE;
	}
	timer = 1;

	for (n_i = n_b; n_i != n_e; n_i++) {
		if (n_i->second->col == WHITE) {
			DFS_visit(n_i->second);
		}
	}

}


KS_BA_node_t* KS_BA_graph_t::get_max_node()
{
	node_list_t::iterator n_b, n_e, n_i;
	int max_h = 0;
	KS_BA_node_t *p_N = 0;

	n_b = node_list.begin(); n_e = node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if (n_i->second->pom > max_h) {
			max_h = n_i->second->pom;
			p_N = n_i->second;
		}
	}

	return(p_N);
}


bool KS_BA_graph_t::is_reachable(KS_BA_node_t *p_from, KS_BA_node_t *p_to)
{
	list<KS_BA_node_t*> L;
	list<KS_BA_node_t*>::iterator n_b, n_e, n_i;

	p_from->col = GRAY;

	L = p_from->get_adj();
	n_b = L.begin(); n_e = L.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if ((*n_i)->name == p_to->name) return(true);
		if ((*n_i)->col == WHITE)
			if (is_reachable(*n_i, p_to)) return(true);
	}

	return(false);
}

int KS_BA_graph_t::SCC_type(const list<int> & C)
{
	list<int> ::const_iterator c_b, c_e, c_i;
	node_list_t::iterator n_b, n_e, n_i;
	list<KS_BA_node_t*> L, L1;
	list<KS_BA_node_t*>::iterator l_b, l_e, l_i, l_j;
	bool non_acc_cycle = false;
	int acc_nodes = 0;

	queue<KS_BA_node_t*> Q;
	KS_BA_node_t *p_N;
	list<KS_BA_node_t*>::iterator l1_b, l1_e, l1_i;

	n_b = node_list.begin(); n_e = node_list.end();
	c_b = C.begin(); c_e = C.end();

	for (n_i = n_b; n_i != n_e; n_i++) {
		n_i->second->col = BLACK;
	}

	for (c_i = c_b; c_i != c_e; c_i++) {
		L.push_back(node_list[*c_i]);
	}

	l_b = L.begin(); l_e = L.end();
	for (l_i = l_b; l_i != l_e; l_i++) {
		if ((*l_i)->accept) {
			acc_nodes++; continue;
		}
		if (non_acc_cycle) continue;

		for (l_j = l_b; l_j != l_e; l_j++) {
			(*l_j)->col = WHITE;
		}
		while (!Q.empty()) Q.pop();

		(*l_i)->col = GRAY; Q.push(*l_i);
		while (!Q.empty()) {
			p_N = Q.front(); Q.pop();
			L1 = p_N->get_adj();
			l1_b = L1.begin(); l1_e = L1.end();
			for (l1_i = l1_b; l1_i != l1_e; l1_i++) {
				if ((*l1_i)->name == (*l_i)->name) {
					non_acc_cycle = true; break;
				}
				if ((*l1_i)->accept) continue;

				if ((*l1_i)->col == WHITE) {
					Q.push(*l1_i);
					(*l1_i)->col = GRAY;
				}
			}
			if (non_acc_cycle) break;
		}
	}

	if (acc_nodes == 0) return(SCC_TYPE_0);
	if (non_acc_cycle) return(SCC_TYPE_1);

	if (C.size() == 1) {
		(*l_b)->col = WHITE;
		if (!is_reachable(*l_b, *l_b)) {
			return(SCC_TYPE_0);
		}
	}

	return(SCC_TYPE_2);
}

void KS_BA_graph_t::SCCt(list<pair<int, list<int> > >& SCC_type_list)
{
	list<list<int> >  SCC_list;
	list<list<int> > ::iterator ll_b, ll_e, ll_i;
	pair<int, list<int> > TL;

	SCC_type_list.erase(SCC_type_list.begin(), SCC_type_list.end());

	SCC(SCC_list);

	ll_b = SCC_list.begin(); ll_e = SCC_list.end();
	for (ll_i = ll_b; ll_i != ll_e; ll_i++) {
		TL.second = *ll_i;
		TL.first = SCC_type(*ll_i);
		SCC_type_list.push_back(TL);
	}
}


bool KS_BA_graph_t::dfs1(KS_BA_node_t *p_N, list<int> & S)
{
	list<KS_BA_node_t*>::iterator n_b, n_e, n_i;
	list<KS_BA_node_t*> L;
	list<int>  C;

	S.push_back(p_N->name);
	p_N->col = GRAY;

	L = p_N->get_adj();
	n_b = L.begin(); n_e = L.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if ((*n_i)->col == WHITE) {
			if (dfs1(*n_i, S)) return(true);
		}
	}

	if (p_N->accept) {
		C.erase(C.begin(), C.end());
		if (dfs2(p_N, S, C)) {
			S.insert(S.end(), C.begin(), C.end());
			return(true);
		}
	}

	S.pop_back();
	return(false);
}

bool KS_BA_graph_t::dfs2(KS_BA_node_t *p_N, const list<int> & S, list<int> & C)
{
	list<KS_BA_node_t*> L;
	list<KS_BA_node_t*>::iterator n_b, n_e, n_i;
	list<int> ::const_iterator s_b, s_e, s_f;

	p_N->pom = 1; // flag

	s_b = S.begin(); s_e = S.end();
	L = p_N->get_adj();
	n_b = L.begin(); n_e = L.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		C.push_back((*n_i)->name);

		s_f = find(s_b, s_e, (*n_i)->name);
		if (s_f != s_e) return(true);
		if ((*n_i)->pom != 1)
			if (dfs2(*n_i, S, C)) return(true);

		C.pop_back();
	}

	return(false);
}

bool KS_BA_graph_t::not_emptiness(list<int> & S)
{
	node_list_t::iterator n_b, n_e, n_i;

	n_b = node_list.begin(); n_e = node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		n_i->second->col = WHITE;
		n_i->second->pom = 0;
	}

	for (n_i = n_b; n_i != n_e; n_i++) {
		if (n_i->second->initial) {
			S.erase(S.begin(), S.end());
			if (dfs1(n_i->second, S)) return(true);
		}
	}

	return(false);
}
