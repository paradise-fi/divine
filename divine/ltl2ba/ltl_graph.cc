/*
*  Transformation LTL formulae to Buchi automata - variant 1
*    - implementation
*
*  Milan Jaros - xjaros1@fi.muni.cz
*
*/

#include <iterator>
#include <algorithm>
#include <stack>
#include <queue>

#include "ltl_graph.hh"
#include "formul.hh"

//#include "error/error.hh"
//#include "deb/deb.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
using namespace std;
#endif //DOXYGEN_PROCESSING

typedef pair<int, int> node_pair;

inline bool operator<(const node_pair& T1, const node_pair& T2)
{
	return((T1.first < T2.first) || ((T1.first == T2.first) &&
		(T1.second < T2.second)));
}


/* Porovnavani seznamu ltl_gba_node_t::set_old */
bool equiv_old(const list<LTL_formul_t>& LF1, const list<LTL_formul_t>& LF2);

/* pridani (nove) LTL_formul_te do seznamu */
void dopln(list<LTL_formul_t>& LF, const LTL_formul_t& F);

void dopln(list<LTL_formul_t>& LF1,
	const list<LTL_formul_t>& LF2); // LF1 <- LF2

void dopln(list<LTL_formul_t>& LV, const list<LTL_formul_t>& set_old,
	const LTL_formul_t& F1, const LTL_formul_t& F2);


/* */
void new1(list<LTL_formul_t>& LV, list<LTL_formul_t>& set_old,
	LTL_formul_t& F);

void next1(list<LTL_formul_t>& LV, LTL_formul_t& F);

void new2(list<LTL_formul_t>& LV, list<LTL_formul_t>& set_old,
	LTL_formul_t& F);


bool equiv_old(const list<LTL_formul_t>& LF1, const list<LTL_formul_t>& LF2)
{
	list<LTL_formul_t>::const_iterator i1_b, i1_e, i1_i, i2_b, i2_e, i2_i;
	list<LTL_formul_t>::const_iterator i_f;
	int t;

	i1_b = LF1.begin(); i1_e = LF1.end();
	i2_b = LF2.begin(); i2_e = LF2.end();

	/* neignoruji se binarni operatory */
	for (i1_i = i1_b; i1_i != i1_e; i1_i++) {
		t = i1_i->typ();
		//if ((t != LTL_BIN_OP) /*&& (t != LTL_BOOL)*/) {
			i_f = find(i2_b, i2_e, *i1_i);
			if (i_f == i2_e) return(false);
		//}
	}

	for (i2_i = i2_b; i2_i != i2_e; i2_i++) {
		t = i2_i->typ();
		//if ((t != LTL_BIN_OP) /*&& (t != LTL_BOOL)*/) {
			i_f = find(i1_b, i1_e, *i2_i);
			if (i_f == i1_e) return(false);
		//}
	}

	return(true);
}

void dopln(list<LTL_formul_t>& LF, const LTL_formul_t& F)
{
	list<LTL_formul_t>::iterator f_e, f_f;

	f_e = LF.end();
	f_f = find(LF.begin(), f_e, F);
	if (f_f == f_e) LF.push_back(F);
}

void dopln(list<LTL_formul_t>& LF1, const list<LTL_formul_t>& LF2)
{
	list<LTL_formul_t>::const_iterator l2_b, l2_e, l2_i;
	list<LTL_formul_t>::iterator l1_f;

	if (LF2.empty()) return;
	if (LF1.empty()) {
		LF1 = LF2; return;
	}

	l2_b = LF2.begin(); l2_e = LF2.end();
	for (l2_i = l2_b; l2_i != l2_e; l2_i++) {
		l1_f = find(LF1.begin(), LF1.end(), *l2_i);
		if (l1_f == LF1.end()) LF1.push_back(*l2_i);
	}
}

void dopln(list<LTL_formul_t>& LV, const list<LTL_formul_t>& set_old,
	const LTL_formul_t& F1, const LTL_formul_t& F2)
{
	list<LTL_formul_t>::const_iterator l_b, l_e, l_f;

	l_b = set_old.begin(); l_e = set_old.end();

	l_f = find(l_b, l_e, F1);
	if (l_f == l_e) LV.push_back(F1);

	if (F1 != F2) {
		l_f = find(l_b, l_e, F2);
		if (l_f == l_e) LV.push_back(F2);
	}
}


void new1(list<LTL_formul_t>& LV, list<LTL_formul_t>& set_old,
	LTL_formul_t& F)
{
	LTL_formul_t F1, F2;
	LTL_oper_t op;
	list<LTL_formul_t>::const_iterator i;

	F.get_binar_op(op, F1, F2);
	switch (op) {
	case op_U:
	case op_or:
		i = find(set_old.begin(), set_old.end(), F1);
		if (i == set_old.end()) {
			dopln(LV, F1);
		}
		break;
	case op_V:
		i = find(set_old.begin(), set_old.end(), F2);
		if (i == set_old.end()) {
			dopln(LV, F2);
		}
		break;
        default: break;//nothing
	}
}

void next1(list<LTL_formul_t>& LV, LTL_formul_t& F)
{
	LTL_formul_t F1, F2;
	LTL_oper_t op;

	F.get_binar_op(op, F1, F2);
	switch (op) {
	case op_U:
	case op_V:
		LV.push_back(F);
		break;
        default: break;//nothing
	}
}

void new2(list<LTL_formul_t>& LV, list<LTL_formul_t>& set_old,
	LTL_formul_t& F)
{
	LTL_formul_t F1, F2;
	LTL_oper_t op;
	list<LTL_formul_t>::iterator i;

	F.get_binar_op(op, F1, F2);

	i = find(set_old.begin(), set_old.end(), F2);
	if (i == set_old.end()) {
		LV.push_back(F2);
	}
	if ((op == op_V) && (F1 != F2)) {
		i = find(set_old.begin(), set_old.end(), F1);
		if (i == set_old.end()) LV.push_back(F1);
	}
}


/* Metody struktury ltl_gba_node_t */

void ltl_gba_node_t::get_label(list<LTL_formul_t>& L) const
{
	list<LTL_formul_t>::const_iterator f_b, f_e, f_i;

	L.erase(L.begin(), L.end());

	f_b = set_old.begin(); f_e = set_old.end();
	for (f_i = f_b; f_i != f_e; f_i++) {
		if (f_i->typ() == LTL_LITERAL) L.push_back(*f_i);
	}
}

void ltl_gba_node_t::get_label(LTL_label_t& L) const
{
	list<LTL_formul_t>::const_iterator f_b, f_e, f_i;
	LTL_literal_t Lit;

	L.erase(L.begin(), L.end());

	f_b = set_old.begin(); f_e = set_old.end();
	for (f_i = f_b; f_i != f_e; f_i++) {
		if (f_i->typ() == LTL_LITERAL) {
			f_i->get_literal(Lit.negace, Lit.predikat);
			L.push_back(Lit);
		}
	}
}

bool ltl_gba_node_t::is_initial() const
{
	set<int>::const_iterator n_f;

	n_f = incoming.find(0);
	return(n_f != incoming.end());
}


/* Metody tridy ltl_graph_t */

ltl_graph_t::ltl_graph_t():timer(0)
{ }

ltl_graph_t::ltl_graph_t(const ltl_graph_t& G):node_set(G.node_set),
set_acc_set(G.set_acc_set), timer(G.timer)
{
	map<int, ltl_ba_node_t*>::const_iterator n_b, n_e, n_i;
	ltl_ba_node_t *p_N, *p_N1;
	list<ltl_ba_node_t*>::const_iterator a_b, a_e, a_i;

	n_b = G.ba_node_set.begin(); n_e = G.ba_node_set.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		p_N = add_ba_node(n_i->first);
		p_N->old_name = n_i->second->old_name;
		p_N->citac = n_i->second->citac;
		p_N->n_label = n_i->second->n_label;
		p_N->initial = n_i->second->initial;
		p_N->accept = n_i->second->accept;
		a_b = n_i->second->adj.begin();
		a_e = n_i->second->adj.end();
		for (a_i = a_b; a_i != a_e; a_i++) {
			p_N1 = add_ba_node((*a_i)->name);
			p_N->adj.push_back(p_N1);
		}
	}
}

ltl_graph_t::~ltl_graph_t()
{
	clear();
}

void ltl_graph_t::clear()
{
	node_set.erase(node_set.begin(), node_set.end());
	set_acc_set.erase(set_acc_set.begin(), set_acc_set.end());

	clear_BA();
}

void ltl_graph_t::clear_BA()
{
	map<int, ltl_ba_node_t*>::iterator n_b, n_e, n_i;

	n_b = ba_node_set.begin(); n_e = ba_node_set.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		delete n_i->second;
	}

	ba_node_set.erase(n_b, n_e);
}

ltl_graph_t& ltl_graph_t::operator=(const ltl_graph_t& G)
{
	map<int, ltl_ba_node_t*>::const_iterator n_b, n_e, n_i;
	ltl_ba_node_t *p_N, *p_N1;
	list<ltl_ba_node_t*>::const_iterator a_b, a_e, a_i;

	clear();

	node_set = G.node_set; set_acc_set = G.set_acc_set;
	timer = G.timer;

	n_b = G.ba_node_set.begin(); n_e = G.ba_node_set.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		p_N = add_ba_node(n_i->first);
		p_N->old_name = n_i->second->old_name;
		p_N->citac = n_i->second->citac;
		p_N->n_label = n_i->second->n_label;
		p_N->initial = n_i->second->initial;
		p_N->accept = n_i->second->accept;
		a_b = n_i->second->adj.begin();
		a_e = n_i->second->adj.end();
		for (a_i = a_b; a_i != a_e; a_i++) {
			p_N1 = add_ba_node((*a_i)->name);
			p_N->adj.push_back(p_N1);
		}
	}

	return(*this);
}

bool ltl_graph_t::find_gba_node(const list<LTL_formul_t>& old,
	const list<LTL_formul_t>& next, list<ltl_gba_node_t>::iterator& p_N)
{
	list<ltl_gba_node_t>::iterator n_b, n_e, n_i;

	n_b = node_set.begin(); n_e = node_set.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if (equiv_old(old, n_i->set_old) &&
			LTL_equiv_labels(next, n_i->set_next)) {
				//p_N = &(*n_i);
				p_N = n_i;
				return(true);
			}
	}

	return(false);
}

void ltl_graph_t::expand(ltl_gba_node_t& N)
{
	ltl_gba_node_t N1, N2;
	list<LTL_formul_t>::iterator i, i1;
	list<LTL_formul_t> pom;
	LTL_formul_t F, F1, F2, F_neg;
	LTL_oper_t op;
	bool b, b_presun;

	list<ltl_gba_node_t>::iterator p_N;

	if (N.set_new.empty()) {
		if (find_gba_node(N.set_old, N.set_next, p_N)) {
			p_N->incoming.insert(N.incoming.begin(),
				N.incoming.end());
			return;
		} else {
			node_set.push_back(N);
			N2.name = timer; timer++;
			N2.incoming.insert(N.name);
			N2.set_new = N.set_next;
			expand(N2);
			return;
		}
	} else {
		i = N.set_new.begin();
		F = *i; N.set_new.erase(i);

		F_neg = F.negace();
		i1 = find(N.set_old.begin(), N.set_old.end(), F_neg);
		if (i1 != N.set_old.end()) {
			return; // F_neg je v N.set_old => kontradikce
		}

		switch (F.typ()) {
		case LTL_BOOL:
			F.get_bool(b);
			if (!b) {
				return;
			} else {
				dopln(N.set_old, F);
				expand(N);
				return;
			}
			break;
		case LTL_LITERAL:
			dopln(N.set_old, F);
			expand(N);
			return; break;
		case LTL_BIN_OP:
			F.get_binar_op(op, F1, F2);
			switch (op) {
			case op_U:
				i1 = find(N.set_old.begin(),
					N.set_old.end(), F2);
				if (i1 != N.set_old.end()) {
					dopln(N.set_old, F);
					expand(N);
					return; break;
				} else {
					i1 = find(N.set_new.begin(),
						N.set_new.end(), F2);
					if (i1 != N.set_new.end()) {
						dopln(N.set_old, F);
						expand(N);
						return; break;
					}
				}
			case op_V:
				if (op == op_V) {
					b_presun = true;
					i1 = find(N.set_old.begin(),
						N.set_old.end(), F1);

					if (i1 == N.set_old.end()) {
					i1 = find(N.set_new.begin(),
						N.set_new.end(), F1);
					if (i1 == N.set_new.end())
						b_presun = false;
					}
					if (b_presun) {
					i1 = find(N.set_old.begin(),
						N.set_old.end(), F2);

					if (i1 == N.set_old.end()) {
					i1 = find(N.set_new.begin(),
						N.set_new.end(), F2);
					if (i1 == N.set_new.end())
						b_presun = false;
					}
					}
					if (b_presun) {
						dopln(N.set_old, F);
						expand(N);
						return; break;
					}
				}
			case op_or:
				N1.name = timer; timer++;
				N1.incoming = N.incoming;
				N1.set_old = N.set_old;
				dopln(N1.set_old, F);
				new1(pom, N.set_old, F);
				dopln(pom, N.set_new);
				N1.set_new = pom;

				pom.erase(pom.begin(), pom.end());
				next1(pom, F);
				dopln(pom, N.set_next);
				N1.set_next = pom;

				pom.erase(pom.begin(), pom.end());
				new2(pom, N.set_old, F);
				dopln(N.set_new, pom);
				dopln(N.set_old, F);

				expand(N1);
				expand(N);
				return; break;
			case op_and:
				N1.name = N.name;
				N1.incoming = N.incoming;
				dopln(pom, N.set_old, F1, F2);
				dopln(pom, N.set_new);
				N1.set_new = pom;
				N1.set_old = N.set_old;
				dopln(N1.set_old, F);
				N1.set_next = N.set_next;
				expand(N1);
				return; break;
			}
			break;
		case LTL_NEXT_OP:
			F.get_next_op(F1);

			N1.name = N.name;
			N1.incoming = N.incoming;
			N1.set_new = N.set_new;
			N1.set_old = N.set_old;
			dopln(N1.set_old, F);
			N1.set_next = N.set_next;
			dopln(N1.set_next, F1);
			expand(N1);
			return; break;
		}
	}
}

bool is_accept(ltl_gba_node_t& N, LTL_formul_t& F0, LTL_formul_t& F2)
{
	list<LTL_formul_t>::iterator i;

	i = find(N.set_old.begin(), N.set_old.end(), F2);
	if (i != N.set_old.end()) return(true);

	i = find(N.set_old.begin(), N.set_old.end(), F0);
	if (i == N.set_old.end()) return(true);

	return(false);
}

void ltl_graph_t::compute_set_acc_set(LTL_formul_t& F)
{
	list<ltl_gba_node_t>::iterator n_b, n_e, n_i;
	LTL_formul_t F0, F1, F2;
	LTL_oper_t op;
	int typ;
	stack<LTL_formul_t> Z;
	LTL_acc_set_t acc_set;

	if (node_set.empty()) return;

	n_b = node_set.begin(); n_e = node_set.end();
	F0 = F;
	while ( typ = F0.typ(),
		((typ == LTL_BIN_OP) || (typ == LTL_NEXT_OP)) ) {
		F1.clear(); F2.clear();
		if (typ == LTL_BIN_OP) {
			F0.get_binar_op(op, F1, F2);
			if (op == op_U) {
				acc_set.erase(acc_set.begin(),
					acc_set.end());
				for (n_i = n_b; n_i != n_e; n_i++) {
					if (is_accept(*n_i, F0, F2))
					acc_set.insert(n_i->name);
				}
				//if (!acc_set.empty()) {
					set_acc_set.push_back(acc_set);
				//}
			}
			Z.push(F1);
			Z.push(F2);
		} else {
			F0.get_next_op(F1);
			Z.push(F1);
		}
		while (!Z.empty()) {
			F0 = Z.top(); Z.pop();
			typ = F0.typ();
			if ((typ == LTL_BIN_OP) ||
				(typ == LTL_NEXT_OP)) break;
		}
	}
}

void ltl_graph_t::remove_non_next_nodes()
{
	set<int> S;
	set<int>::iterator s_f, s_e;
	list<ltl_gba_node_t>::iterator n_b, n_e, n_i, n_j;
	set<int>::iterator i_b, i_e, i_i;

	n_b = node_set.begin(); n_e = node_set.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		i_b = n_i->incoming.begin(); i_e = n_i->incoming.end();
		for (i_i = i_b; i_i != i_e; i_i++) {
			S.insert(*i_i);
		}
	}

	s_e = S.end();
	n_i = node_set.begin();
	while (n_i != node_set.end()) {
		s_f = S.find(n_i->name);
		if (s_f == s_e) {
			n_j = n_i; n_i++;
			node_set.erase(n_j);
		} else {
			n_i++;
		}
	}
}

void ltl_graph_t::create_graph(LTL_formul_t& F)
{
	ltl_gba_node_t N;

	clear(); timer = 1;

	N.name = timer; timer++;
	N.incoming.insert(0);
	N.set_new.push_back(F);

	expand(N);

	remove_non_next_nodes();

	compute_set_acc_set(F);
}

ltl_ba_node_t* ltl_graph_t::add_ba_node(int name)
{
	map<int, ltl_ba_node_t*>::iterator n_f;
	ltl_ba_node_t *p_N;

	n_f = ba_node_set.find(name);
	if (n_f == ba_node_set.end()) {
		p_N = new ltl_ba_node_t;
		p_N->name = name;
		ba_node_set[name] = p_N;

		p_N->accept = false; p_N->initial = false;
	} else {
		p_N = n_f->second;
	}

	return(p_N);
}

void ltl_graph_t::copy_nodes()
{
	list<ltl_gba_node_t>::iterator n_b, n_e, n_i;
	list<int> L;
	list<int>::const_iterator i_b, i_e, i_i;
	ltl_ba_node_t *p_N, *p_N1;

	n_b = node_set.begin(); n_e = node_set.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		p_N = add_ba_node(n_i->name);
		p_N->old_name = n_i->name; p_N->citac = 0;
		n_i->get_label(p_N->n_label);
		p_N->initial = n_i->is_initial();
		get_gba_node_adj(n_i->name, L);
		i_b = L.begin(); i_e = L.end();
		for (i_i = i_b; i_i != i_e; i_i++) {
			p_N1 = add_ba_node(*i_i);
			p_N->adj.push_back(p_N1);
		}
	}
}

void ltl_graph_t::degeneralize()
{
	map<node_pair, ltl_ba_node_t*> pom_node_set;
	map<node_pair, ltl_ba_node_t*>::iterator pn_f;
	node_pair T;

	list<ltl_gba_node_t>::iterator n1_b, n1_e, n1_i;
	map<int, ltl_ba_node_t*>::iterator n2_b, n2_e, n2_i;

	LTL_acc_set_t::const_iterator a_e, a_f;

	int cit, cit1, d;
	queue<ltl_ba_node_t*> Q;
	ltl_ba_node_t *p_N, *p_N1;
	list<int> L_adj;
	list<int>::iterator li_b, li_e, li_i;


	clear_BA(); timer = 1;

	if (set_acc_set.size() == 0) {
		copy_nodes();
		n2_b = ba_node_set.begin(); n2_e = ba_node_set.end();
		for (n2_i = n2_b; n2_i != n2_e; n2_i++) {
			n2_i->second->accept = true;
		}
		return;
	}

	d = set_acc_set.size();
	n1_b = node_set.begin(); n1_e = node_set.end();
	T.second = 1;
	for (n1_i = n1_b; n1_i != n1_e; n1_i++) {
		if (n1_i->is_initial()) {
			p_N = add_ba_node(timer); timer++;
			p_N->old_name = n1_i->name; p_N->citac = 1;
			n1_i->get_label(p_N->n_label);
			p_N->initial = true;
			T.first = n1_i->name;
			pom_node_set[T] = p_N;
			Q.push(p_N);
		}
	}

	while (!Q.empty()) {
		p_N = Q.front(); Q.pop();

		get_gba_node_adj(p_N->old_name, L_adj);
		li_b = L_adj.begin(); li_e = L_adj.end();
		cit = p_N->citac;
		a_e = set_acc_set[cit - 1].end();
		a_f = set_acc_set[cit - 1].find(p_N->old_name);
		if (a_f == a_e) {
			cit1 = cit;
		} else {
			if (cit == d) cit1 = 1;
			else cit1 = cit + 1;
			if (cit == 1) p_N->accept = true;
		}

		T.second = cit1;
		for (li_i = li_b; li_i != li_e; li_i++) {
			T.first = *li_i;
			pn_f = pom_node_set.find(T);
			if (pn_f == pom_node_set.end()) {
				p_N1 = add_ba_node(timer); timer++;
				p_N1->old_name = T.first;
				p_N1->citac = T.second;
				get_gba_node_label(*li_i, p_N1->n_label);

				pom_node_set[T] = p_N1;
				Q.push(p_N1);
			} else {
				p_N1 = pn_f->second;
			}
			p_N->adj.push_back(p_N1);
		}
	}
}

bool ltl_graph_t::get_gba_node_adj(int name, list<int>& L) const
{
	list<ltl_gba_node_t>::const_iterator n_b, n_e, n_i;
	set<int>::const_iterator in_e, in_f;
	bool b = false;

	L.erase(L.begin(), L.end());

	n_b = node_set.begin(); n_e = node_set.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if (n_i->name == name) b = true;
		in_e = n_i->incoming.end();
		in_f = n_i->incoming.find(name);
		if (in_f != in_e) L.push_back(n_i->name);
	}

	return(b);
}

bool ltl_graph_t::get_gba_node_adj(int name, list<ltl_gba_node_t>& L) const
{
	list<ltl_gba_node_t>::const_iterator n_b, n_e, n_i;
	set<int>::const_iterator in_e, in_f;
	bool b = false;

	L.erase(L.begin(), L.end());

	n_b = node_set.begin(); n_e = node_set.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if (n_i->name == name) b = true;
		in_e = n_i->incoming.end();
		in_f = n_i->incoming.find(name);
		if (in_f != in_e) L.push_back(*n_i);
	}

	return(b);
}

bool ltl_graph_t::get_gba_node_label(int name, list<LTL_formul_t>& L) const
{
	list<ltl_gba_node_t>::const_iterator n_b, n_e, n_f;

	L.erase(L.begin(), L.end());

	n_b = node_set.begin(); n_e = node_set.end();
	n_f = find(n_b, n_e, name);
	if (n_f == n_e) return(false);

	n_f->get_label(L);
	return(true);
}

bool ltl_graph_t::get_gba_node_label(int name, list<LTL_literal_t>& L)
const
{
	list<ltl_gba_node_t>::const_iterator n_b, n_e, n_f;

	L.erase(L.begin(), L.end());

	n_b = node_set.begin(); n_e = node_set.end();
	n_f = find(n_b, n_e, name);
	if (n_f == n_e) return(false);

	n_f->get_label(L);
	return(true);
}

bool ltl_graph_t::find_gba_node(int name, ltl_gba_node_t& N) const
{
	list<ltl_gba_node_t>::const_iterator n_b, n_e, n_f;

	n_b = node_set.begin(); n_e = node_set.end();
	n_f = find(n_b, n_e, name);

	if (n_f == n_e) return(false);

	N = *n_f;
	return(true);
}

void ltl_graph_t::get_gba_init_nodes(list<int>& L) const
{
	get_gba_node_adj(0, L);
}

void ltl_graph_t::get_gba_init_nodes(list<ltl_gba_node_t>& L) const
{
	get_gba_node_adj(0, L);
}

const LTL_set_acc_set_t& ltl_graph_t::get_gba_set_acc_set() const
{
	return(set_acc_set);
}

const list<ltl_gba_node_t>& ltl_graph_t::get_all_gba_nodes() const
{
	return(node_set);
}

void ltl_graph_t::get_all_gba_nodes(list<int>& L) const
{
	list<ltl_gba_node_t>::const_iterator n_b, n_e, n_i;

	L.erase(L.begin(), L.end());

	n_b = node_set.begin(); n_e = node_set.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		L.push_back(n_i->name);
	}
}

void ltl_graph_t::vypis_GBA(ostream& vystup,
	bool strict_output) const
{
	list<ltl_gba_node_t>::const_iterator n_b, n_e, n_i;
	LTL_label_t LF;
	LTL_label_t::const_iterator f_b, f_e, f_i;
	list<int> LI;
	list<int>::const_iterator i_b, i_e, i_i;
	LTL_set_acc_set_t::const_iterator sas_b, sas_e, sas_i;
	LTL_acc_set_t::const_iterator as_b, as_e, as_i;

	bool b, b1;
	int tr_count = 0, init_count = 0;

	n_b = node_set.begin(); n_e = node_set.end();

	get_gba_init_nodes(LI);
	init_count = LI.size();
	i_b = LI.begin(); i_e = LI.end();
	vystup << "init -> {";
	b = false;
	for (i_i = i_b; i_i != i_e; i_i++) {
		if (b) vystup << ", ";
		else b = true;
		vystup << *i_i;
	}
	vystup << "}" << endl;

	for (n_i = n_b; n_i != n_e; n_i++) {
		vystup << n_i->name << ": {";
		n_i->get_label(LF);
		b = false;
		f_b = LF.begin(); f_e = LF.end();
		for (f_i = f_b; f_i != f_e; f_i++) {
			if (b) vystup << ", ";
			else b = true;
			vystup << *f_i;
		}
		vystup << "} -> {";

		get_gba_node_adj(n_i->name, LI);
		tr_count += LI.size(); b = false;
		i_b = LI.begin(); i_e = LI.end();
		for (i_i = i_b; i_i != i_e; i_i++) {
			if (b) vystup << ", ";
			else b = true;
			vystup << *i_i;
		}
		vystup << "}" << endl;
	}

	vystup << "Accept sets: {";

	b = false;
	sas_b = set_acc_set.begin(); sas_e = set_acc_set.end();
	for (sas_i = sas_b; sas_i != sas_e; sas_i++) {
		if (b) vystup << ", ";
		else b = true;

		vystup << "{";
		b1 = false;
		as_b = sas_i->begin(); as_e = sas_i->end();
		for (as_i = as_b; as_i != as_e; as_i++) {
			if (b1) vystup << ", ";
			else b1 = true;
			vystup << *as_i;
		}
		vystup << "}";
	}
	vystup << "}" << endl;

	if (strict_output) vystup << "# ";
	vystup << "nodes: " << node_set.size() << "; accept sets: " <<
	set_acc_set.size() << "; initial: " << init_count <<
	"; transitions: " << tr_count << ";" << endl;
}

void ltl_graph_t::vypis_GBA(bool strict_output) const
{
	vypis_GBA(cout, strict_output);
}

list<ltl_ba_node_t*> ltl_graph_t::get_ba_init_nodes() const
{
	map<int, ltl_ba_node_t*>::const_iterator n_b, n_e, n_i;
	list<ltl_ba_node_t*> L;

	n_b = ba_node_set.begin(); n_e = ba_node_set.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if (n_i->second->initial) L.push_back(n_i->second);
	}

	return(L);
}

list<ltl_ba_node_t*> ltl_graph_t::get_ba_accept_nodes() const
{
	map<int, ltl_ba_node_t*>::const_iterator n_b, n_e, n_i;
	list<ltl_ba_node_t*> L;

	n_b = ba_node_set.begin(); n_e = ba_node_set.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if (n_i->second->accept) L.push_back(n_i->second);
	}

	return(L);
}

const map<int, ltl_ba_node_t*>& ltl_graph_t::get_all_ba_nodes() const
{
	return(ba_node_set);
}

void ltl_graph_t::vypis_BA(ostream& vystup, bool strict_output,
	bool output_node_pair) const
{
	map<int, ltl_ba_node_t*>::const_iterator n_b, n_e, n_i;
	list<ltl_ba_node_t*> L;
	list<ltl_ba_node_t*>::const_iterator l_b, l_e, l_i;
	LTL_label_t::const_iterator f_b, f_e, f_i;

	int tr_count = 0, init_count = 0, accept_count = 0;
	bool b;

	vystup << "init -> {";
	L = get_ba_init_nodes();
	init_count = L.size();
	l_b = L.begin(); l_e = L.end(); b = false;
	for (l_i = l_b; l_i != l_e; l_i++) {
		if (b) vystup << ", ";
		else b = true;
		vystup << (*l_i)->name;
	}
	vystup << "}" << endl;

	n_b = ba_node_set.begin(); n_e = ba_node_set.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		vystup << n_i->first;
		if (output_node_pair && !strict_output) {
			vystup << " (" << n_i->second->old_name << ", " <<
			n_i->second->citac << ")";
		}
		vystup << ": {";
		f_b = n_i->second->n_label.begin();
		f_e = n_i->second->n_label.end(); b = false;
		for (f_i = f_b; f_i != f_e; f_i++) {
			if (b) vystup << ", ";
			else b = true;
			vystup << *f_i;
		}
		vystup << "} -> {";
		tr_count += n_i->second->adj.size();
		l_b = n_i->second->adj.begin();
		l_e = n_i->second->adj.end(); b = false;
		for (l_i = l_b; l_i != l_e; l_i++) {
			if (b) vystup << ", ";
			else b = true;
			vystup << (*l_i)->name;
		}
		vystup << "}" << endl;
	}

	L = get_ba_accept_nodes();
	accept_count = L.size();
	vystup << "Accept: {";
	l_b = L.begin(); l_e = L.end(); b = false;
	for (l_i = l_b; l_i != l_e; l_i++) {
		if (b) vystup << ", ";
		else b = true;
		vystup << (*l_i)->name;
	}
	vystup << "}" << endl;

	if (strict_output) vystup << "# ";
	vystup << "nodes: " << ba_node_set.size() << "; accept: " <<
	accept_count << "; initial: " << init_count <<
	"; transitions: " << tr_count << ";" << endl;
}

void ltl_graph_t::vypis_BA(bool strict_output, bool output_node_pair) const
{
	vypis_BA(cout, strict_output, output_node_pair);
}
