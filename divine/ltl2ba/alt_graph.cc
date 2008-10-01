/*
*  Transformation LTL formulae to Buchi automata - variant 2
*    - implementation
*
*  Milan Jaros - xjaros1@fi.muni.cz
*/

#include <iterator>
#include <queue>
#include <set>
#include <algorithm>

#include "alt_graph.hh"
#include "formul.hh"

//#include "error/error.hh"
//#include "deb/deb.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
using namespace std;
#endif //DOXYGEN_PROCESSING

/* pomocne funkce */

bool equiv_C(const list<ALT_vwaa_node_t*> & L1,
	const list<ALT_vwaa_node_t*> & L2);

void or_sum(const list<list<ALT_vwaa_node_t*> >& L1,
	const list<list<ALT_vwaa_node_t*> >& L2,
	list<list<ALT_vwaa_node_t*> >& LV);

void conjunct(const list<ALT_vwaa_node_t*> & L1,
	const list<ALT_vwaa_node_t*> & L2,
	list<ALT_vwaa_node_t*> & C);

void and_sum(const list<list<ALT_vwaa_node_t*> >& L1,
	const list<list<ALT_vwaa_node_t*> >& L2,
	list<list<ALT_vwaa_node_t*> >& LV);


bool equiv_ALT_gba_node_ts(ALT_gba_node_t *p_N1, ALT_gba_node_t *p_N2);


void ALT_vwaa_trans_t::clear()
{
	t_label.erase(t_label.begin(), t_label.end());
	target.erase(target.begin(), target.end());
}

void ALT_vwaa_trans_t::op_times(ALT_vwaa_trans_t& T1, ALT_vwaa_trans_t& T2)
{
	LTL_label_t::iterator l_b, l_e, l_i, l2_b, l2_e, l2_i;
	list<ALT_vwaa_node_t*> ::iterator t_b, t_e, t_i, t2_b, t2_e, t2_i;

	clear();

	l2_b = T2.t_label.begin(); l2_e = T2.t_label.end();
	t_label = T1.t_label;
	for (l2_i = l2_b; l2_i != l2_e; l2_i++) {
		l_b = t_label.begin(); l_e = t_label.end();
		for (l_i = l_b; l_i != l_e; l_i++) {
			if (*l_i == *l2_i) break;
		}
		if (l_i == l_e) t_label.push_back(*l2_i);
	}

	t2_b = T2.target.begin(); t2_e = T2.target.end();
	target = T1.target;
	for (t2_i = t2_b; t2_i != t2_e; t2_i++) {
		t_b = target.begin(); t_e = target.end();
		for (t_i = t_b; t_i != t_e; t_i++) {
			if (*t_i == *t2_i) break;
		}
		if (t_i == t_e) target.push_back(*t2_i);
	}
}

bool ALT_vwaa_trans_t::operator==(const ALT_vwaa_trans_t& T) const
{
	LTL_label_t::const_iterator l_b, l_e, l2_b, l2_e;
	list<ALT_vwaa_node_t*>::const_iterator t_b, t_e, t_i,
		t2_b, t2_e, t2_i;

	if ((t_label.size() != T.t_label.size()) ||
		(target.size() != T.target.size()))
		return(false);

	l_b = t_label.begin(); l_e = t_label.end();
	l2_b = T.t_label.begin(); l2_e = T.t_label.end();

	/*for (l_i = l_b; l_i != l_e; l_i++) {
		l2_i = find(l2_b, l2_e, *l_i);
		if (l2_i == l2_e) return(false);
	}*/
	if (!LTL_equiv_labels(t_label, T.t_label)) return(false);

	t_b = target.begin(); t_e = target.end();
	t2_b = T.target.begin(); t2_e = T.target.end();

	for (t_i = t_b; t_i != t_e; t_i++) {
		t2_i = find(t2_b, t2_e, *t_i);
		if (t2_i == t2_e) return(false);
	}

	for (t2_i = t2_b; t2_i != t2_e; t2_i++) {
		t_i = find(t_b, t_e, *t2_i);
		if (t_i == t_e) return(false);
	}

	return(true);
}

bool ALT_vwaa_trans_t::operator!=(const ALT_vwaa_trans_t& T) const
{
	return(!(*this == T));
}


bool ALT_vwaa_node_t::is_accept()
{
	LTL_oper_t op;

	if (label.get_binar_op(op)) {
		if (op == op_U) return(true);
	}
	return(false);
}


ALT_graph_t::ALT_graph_t():timer(0), simp_GBA(false)
{ }

ALT_graph_t::ALT_graph_t(const ALT_graph_t& G):timer(G.timer),
  simp_GBA(G.simp_GBA)

{
	copy_vwaa(G);
	copy_GBA(G);
	copy_BA(G);
}

void ALT_graph_t::copy_vwaa(const ALT_graph_t& G)
{
	map<int, ALT_vwaa_node_t*>::const_iterator gn_b, gn_e, gn_i;
	list<ALT_vwaa_trans_t> ::const_iterator ad_b, ad_e, ad_i;
	list<ALT_vwaa_node_t*> ::const_iterator t_b, t_e, t_i;
	list<list<ALT_vwaa_node_t*> >::const_iterator gi_b, gi_e, gi_i;
	list<ALT_vwaa_node_t*>  I_S;
	ALT_vwaa_node_t *p_N, *p_N1;
	const ALT_vwaa_node_t *p_gN, *p_gN1;
	ALT_vwaa_trans_t Tr;

	gn_b = G.vwaa_node_list.begin();
	gn_e = G.vwaa_node_list.end();

	for (gn_i = gn_b; gn_i != gn_e; gn_i++) {
		p_gN = gn_i->second;
		p_N = add_vwaa_node(p_gN->label, p_gN->name);

		ad_b = p_gN->adj.begin();
		ad_e = p_gN->adj.end();
		for (ad_i = ad_b; ad_i != ad_e; ad_i++) {
			Tr.clear();
			Tr.t_label = ad_i->t_label;
			t_b = ad_i->target.begin();
			t_e = ad_i->target.end();
			for (t_i = t_b; t_i != t_e; t_i++) {
				p_gN1 = *t_i;
				p_N1 = add_vwaa_node(p_gN1->label,
					p_gN1->name);
				Tr.target.push_back(p_N1);
			}
			p_N->adj.push_back(Tr);
		}
	}

	gi_b = G.vwaa_init_nodes.begin();
	gi_e = G.vwaa_init_nodes.end();
	for (gi_i = gi_b; gi_i != gi_e; gi_i++) {
		t_b = gi_i->begin(); t_e = gi_i->end();
		I_S.erase(I_S.begin(), I_S.end());
		for (t_i = t_b; t_i != t_e; t_i++) {
			I_S.push_back(vwaa_node_list[(*t_i)->name]);
		}
		vwaa_init_nodes.push_back(I_S);
	}
}

void ALT_graph_t::copy_GBA(const ALT_graph_t& G)
{
	list<ALT_gba_node_t*>::const_iterator gn_b, gn_e, gn_i;
	list<ALT_gba_node_t*>::iterator gn_j;
	list<ALT_vwaa_node_t*> ::const_iterator v_b, v_e, v_i;
	ALT_gba_node_t *p_N;
	ALT_vwaa_node_t *p_vN;
	list<list<ALT_gba_trans_t*> >::const_iterator at_b, at_e, at_i;
	list<ALT_gba_trans_t*>::const_iterator t_b, t_e, t_i;
	list<ALT_gba_trans_t*>::iterator nt_b, nt_e, nt_i;
	ALT_gba_trans_t *p_T;
	list<ALT_gba_trans_t*> LT;
	int name;

	gn_b = G.gba_node_list.begin();
	gn_e = G.gba_node_list.end();
	for (gn_i = gn_b; gn_i != gn_e; gn_i++) {
		p_N = new ALT_gba_node_t;
		p_N->name = (*gn_i)->name;
		gba_node_list.push_back(p_N);

		v_b = (*gn_i)->node_label.begin();
		v_e = (*gn_i)->node_label.end();
		for (v_i = v_b; v_i != v_e; v_i++) {
			p_vN = find_vwaa_node((*v_i)->name);
			if (p_vN != 0) {
				p_N->node_label.push_back(p_vN);
			} else {
				cerr << "Nelze najit ALT_vwaa_node_t: " <<
				(*v_i)->name << endl;
			}
		}
	}

	gn_j = gba_node_list.begin();
	for (gn_i = gn_b; gn_i != gn_e; gn_i++, gn_j++) {
		t_b = (*gn_i)->adj.begin(); t_e = (*gn_i)->adj.end();
		for (t_i = t_b; t_i != t_e; t_i++) {
			p_T = new ALT_gba_trans_t;
			p_T->t_label = (*t_i)->t_label;
			p_T->p_from = *gn_j;
			p_T->p_to = find_gba_node((*t_i)->p_to->name);
			(*gn_j)->adj.push_back(p_T);
		}
	}

	gn_b = G.gba_init_nodes.begin(); gn_e = G.gba_init_nodes.end();
	for (gn_i = gn_b; gn_i != gn_e; gn_i++) {
		p_N = find_gba_node((*gn_i)->name);
		gba_init_nodes.push_back(p_N);
	}

	at_b = G.acc_set.begin(); at_e = G.acc_set.end();
	for (at_i = at_b; at_i != at_e; at_i++) {
		LT.erase(LT.begin(), LT.end());
		t_b = at_i->begin(); t_e = at_i->end();
		for (t_i = t_b; t_i != t_e; t_i++) {
			p_N = find_gba_node((*t_i)->p_from->name);
			name = (*t_i)->p_to->name;
			nt_b = p_N->adj.begin(); nt_e = p_N->adj.end();
			for (nt_i = nt_b; nt_i != nt_e; nt_i++) {
				if (((*nt_i)->p_to->name == name) &&
					LTL_equiv_labels((*nt_i)->t_label,
					(*t_i)->t_label)) {
					p_T = *nt_i; break;
				}
			}
			LT.push_back(p_T);
		}
		acc_set.push_back(LT);
	}

	/* list< pair<list<list<ALT_vwaa_node_t*> >,
		ALT_gba_node_t*> > ALT_list_eq_gba_nodes_t */
	ALT_list_eq_gba_nodes_t::const_iterator eq_b, eq_e, eq_i;
	list<list<ALT_vwaa_node_t*> >::const_iterator lv_b, lv_e, lv_i;
	list<ALT_vwaa_node_t*>  LV;
	ALT_eq_gba_nodes_t P;

	eq_b = G.gba_equiv_nodes.begin();
	eq_e = G.gba_equiv_nodes.end();
	for (eq_i = eq_b; eq_i != eq_e; eq_i++) {
		P.first.erase(P.first.begin(), P.first.end());
		p_N = find_gba_node(eq_i->second->name);
		P.second = p_N;
		lv_b = eq_i->first.begin(); lv_e = eq_i->first.end();
		for (lv_i = lv_b; lv_i != lv_e; lv_i++) {
			LV.erase(LV.begin(), LV.end());
			v_b = lv_i->begin(); v_e = lv_i->end();
			for (v_i = v_b; v_i != v_e; v_i++) {
				p_vN = find_vwaa_node((*v_i)->name);
				LV.push_back(p_vN);
			}
			P.first.push_back(LV);
		}
		gba_equiv_nodes.push_back(P);
	}
}

void ALT_graph_t::copy_BA(const ALT_graph_t& G)
{
	list<ALT_ba_node_t*>::const_iterator n_b, n_e, n_i;
	list<ALT_ba_node_t*>::iterator n_j;
	list<ALT_ba_trans_t>::const_iterator t_b, t_e, t_i;
	ALT_ba_node_t *p_N;
	ALT_ba_trans_t T;

	n_b = G.ba_node_list.begin(); n_e = G.ba_node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		p_N = new ALT_ba_node_t;
		p_N->name = (*n_i)->name; p_N->citac = (*n_i)->citac;
		if ((*n_i)->p_old != 0) {
			p_N->p_old = find_gba_node((*n_i)->p_old->name);
		} else {
			p_N->p_old = 0;
		}
		ba_node_list.push_back(p_N);
	}

	n_j = ba_node_list.begin();
	for (n_i = n_b; n_i != n_e; n_i++, n_j++) {
		t_b = (*n_i)->adj.begin(); t_e = (*n_i)->adj.end();
		for (t_i = t_b; t_i != t_e; t_i++) {
			T.t_label = t_i->t_label;
			T.target = find_ba_node(t_i->target->name);
			(*n_j)->adj.push_back(T);
		}
	}

	n_b = G.ba_init_nodes.begin(); n_e = G.ba_init_nodes.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		p_N = find_ba_node((*n_i)->name);
		ba_init_nodes.push_back(p_N);
	}

	n_b = G.ba_accept_nodes.begin(); n_e = G.ba_accept_nodes.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		p_N = find_ba_node((*n_i)->name);
		ba_accept_nodes.push_back(p_N);
	}
}

ALT_vwaa_node_t* ALT_graph_t::find_vwaa_node(int name)
{
	map<int, ALT_vwaa_node_t*>::const_iterator n_f;

	n_f = vwaa_node_list.find(name);

	if (n_f != vwaa_node_list.end()) return(n_f->second);
	else return(0);
}

ALT_gba_node_t* ALT_graph_t::find_gba_node(int name)
{
	list<ALT_gba_node_t*>::const_iterator n_b, n_e, n_i;
	ALT_gba_node_t *p_N = 0;

	n_b = gba_node_list.begin(); n_e = gba_node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if ((*n_i)->name == name) {
			p_N = *n_i;
			break;
		}
	}

	return(p_N);
}

ALT_ba_node_t* ALT_graph_t::find_ba_node(int name)
{
	list<ALT_ba_node_t*>::const_iterator n_b, n_e, n_i;
	ALT_ba_node_t *p_N = 0;

	n_b = ba_node_list.begin(); n_e = ba_node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if ((*n_i)->name == name) {
			p_N = *n_i;
			break;
		}
	}

	return(p_N);
}

ALT_graph_t::~ALT_graph_t()
{
	clear();
}

ALT_graph_t& ALT_graph_t::operator=(const ALT_graph_t& G)
{
	clear();

	timer = G.timer; simp_GBA = G.simp_GBA;

	copy_vwaa(G);
	copy_GBA(G);
	copy_BA(G);

	return(*this);
}

void ALT_graph_t::clear()
{
	clear_vwaa();
	clear_GB();
	clear_BA();
}

void ALT_graph_t::clear_vwaa()
{
	map<int, ALT_vwaa_node_t*>::iterator n_b, n_e, n_i;

	n_b = vwaa_node_list.begin();
	n_e = vwaa_node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		delete n_i->second;
	}

	vwaa_node_list.erase(n_b, n_e);
	vwaa_init_nodes.erase(vwaa_init_nodes.begin(),
		vwaa_init_nodes.end());
	vwaa_accept_nodes.erase(vwaa_accept_nodes.begin(),
		vwaa_accept_nodes.end());
}

void ALT_graph_t::clear_GB()
{
	list<ALT_gba_node_t*>::iterator gn_b, gn_e, gn_i;

	gn_b = gba_node_list.begin(); gn_e = gba_node_list.end();
	for (gn_i = gn_b; gn_i != gn_e; gn_i++) {
		delete (*gn_i);
	}

	gba_node_list.erase(gn_b, gn_e);
	acc_set.erase(acc_set.begin(), acc_set.end());
	gba_init_nodes.erase(gba_init_nodes.begin(),
		gba_init_nodes.end());

	gba_equiv_nodes.erase(gba_equiv_nodes.begin(),
		gba_equiv_nodes.end());
}

void ALT_graph_t::clear_BA()
{
	list<ALT_ba_node_t*>::iterator n_b, n_e, n_i;

	n_b = ba_node_list.begin(); n_e = ba_node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		delete (*n_i);
	}

	ba_node_list.erase(n_b, n_e);
	ba_init_nodes.erase(ba_init_nodes.begin(), ba_init_nodes.end());
	ba_accept_nodes.erase(ba_accept_nodes.begin(),
		ba_accept_nodes.end());
}

ALT_vwaa_node_t* ALT_graph_t::find_vwaa_node(const LTL_formul_t& F)
{
	map<int, ALT_vwaa_node_t*>::iterator n_b, n_e, n_f;

	n_b = vwaa_node_list.begin();
	n_e = vwaa_node_list.end();
	for (n_f = n_b; n_f != n_e; n_f++) {
		if (n_f->second->label == F) return(n_f->second);
	}

	return(0);
}

ALT_vwaa_node_t* ALT_graph_t::add_vwaa_node(const LTL_formul_t& F, int n)
{
	map<int, ALT_vwaa_node_t*>::iterator n_e, n_f;
	ALT_vwaa_node_t *p_N;

	n_e = vwaa_node_list.end();
	n_f = vwaa_node_list.find(n);
	if (n_f == n_e) {
		p_N = new ALT_vwaa_node_t;
		p_N->name = n;
		p_N->label = F;
		vwaa_node_list[n] = p_N;
	} else {
		p_N = n_f->second;
	}

	return(p_N);
}

ALT_vwaa_node_t* ALT_graph_t::add_vwaa_node(const LTL_formul_t& F)
{
	ALT_vwaa_node_t *p_N;

	p_N = find_vwaa_node(F);
	if (p_N == 0) {
		p_N = new ALT_vwaa_node_t;
		p_N->label = F;
		p_N->name = timer;
		vwaa_node_list[timer] = p_N;
		timer++;
	}

	return(p_N);
}

void ALT_graph_t::op_times(list<ALT_vwaa_trans_t>& J1,
	list<ALT_vwaa_trans_t>& J2,
	list<ALT_vwaa_trans_t>& JV)
{
	list<ALT_vwaa_trans_t>::iterator j1_b, j1_e, j1_i, j2_b, j2_e, j2_i;
	list<ALT_vwaa_trans_t>::iterator jv_e, jv_i;

	ALT_vwaa_trans_t Tr;

	JV.erase(JV.begin(), JV.end());

	j1_b = J1.begin(); j1_e = J1.end();
	j2_b = J2.begin(); j2_e = J2.end();

	for (j1_i = j1_b; j1_i != j1_e; j1_i++) {
		for (j2_i = j2_b; j2_i != j2_e; j2_i++) {
			Tr.op_times(*j1_i, *j2_i);
			jv_e = JV.end();
			jv_i = find(JV.begin(), jv_e, Tr);
			if (jv_i == jv_e)
				JV.push_back(Tr);
		}
	}
}

void ALT_graph_t::op_sum(list<ALT_vwaa_trans_t> & J1,
	list<ALT_vwaa_trans_t> & J2,
	list<ALT_vwaa_trans_t> & JV)
{
	list<ALT_vwaa_trans_t> ::iterator j_b, j_e, j_i, jv_e, jv_i;

	JV = J1;
	j_b = J2.begin(); j_e = J2.end();
	for (j_i = j_b; j_i != j_e; j_i++) {
		jv_e = JV.end();
		jv_i = find(JV.begin(), jv_e, *j_i);
		if (jv_i == jv_e)
			JV.push_back(*j_i);
	}
}

bool equiv_C(const list<ALT_vwaa_node_t*> & L1,
	const list<ALT_vwaa_node_t*> & L2)
{
	list<ALT_vwaa_node_t*>::const_iterator l1_b, l1_e, l1_i,
		l2_b, l2_e, l2_i;

	if (L1.size() != L2.size()) return(false);

	l1_b = L1.begin(); l1_e = L1.end();
	l2_b = L2.begin(); l2_e = L2.end();

	for (l1_i = l1_b; l1_i != l1_e; l1_i++) {
		l2_i = find(l2_b, l2_e, *l1_i);
		if (l2_i == l2_e) return(false);
	}

	return(true);
}

void or_sum(const list<list<ALT_vwaa_node_t*> >& L1,
	const list<list<ALT_vwaa_node_t*> >& L2,
	list<list<ALT_vwaa_node_t*> >& LV)
{
	list<list<ALT_vwaa_node_t*> >::const_iterator l_b, l_e, l_i;
	list<list<ALT_vwaa_node_t*> >::iterator lv_e, lv_i;

	LV = L1;
	l_b = L2.begin(); l_e = L2.end();
	for (l_i = l_b; l_i != l_e; l_i++) {
		lv_e = LV.end();
		for (lv_i = LV.begin(); lv_i != lv_e; lv_i++) {
			if (equiv_C(*l_i, *lv_i)) break;
		}
		if (lv_i == lv_e) {
			LV.push_back(*l_i);
		}
	}
}

void conjunct(const list<ALT_vwaa_node_t*> & L1,
	const list<ALT_vwaa_node_t*> & L2,
	list<ALT_vwaa_node_t*> & C)
{
	list<ALT_vwaa_node_t*> ::const_iterator l_b, l_e, l_i;
	list<ALT_vwaa_node_t*> ::iterator c_e, c_i;

	C = L1;
	l_b = L2.begin(); l_e = L2.end();
	for (l_i = l_b; l_i != l_e; l_i++) {
		c_e = C.end();
		c_i = find(C.begin(), c_e, *l_i);
		if (c_i == c_e) C.push_back(*l_i);
	}
}

void and_sum(const list<list<ALT_vwaa_node_t*> >& L1,
	const list<list<ALT_vwaa_node_t*> >& L2,
	list<list<ALT_vwaa_node_t*> >& LV)
{
	list<list<ALT_vwaa_node_t*> >::const_iterator l1_b, l1_e, l1_i;
	list<list<ALT_vwaa_node_t*> >::const_iterator l2_b, l2_e, l2_i;
	list<list<ALT_vwaa_node_t*> >::iterator lv_e, lv_i;
	list<ALT_vwaa_node_t*>  C;

	l1_b = L1.begin(); l1_e = L1.end();
	l2_b = L2.begin(); l2_e = L2.end();

	for (l1_i = l1_b; l1_i != l1_e; l1_i++) {
		for (l2_i = l2_b; l2_i != l2_e; l2_i++) {
			conjunct(*l1_i, *l2_i, C);
			lv_e = LV.end();
			for (lv_i = LV.begin(); lv_i != lv_e; lv_i++) {
				if (equiv_C(*lv_i, C)) break;
			}
			if (lv_i == lv_e)
				LV.push_back(C);
		}
	}
}

void ALT_graph_t::op_over(LTL_formul_t& F, list<list<ALT_vwaa_node_t*> >& LV)
{
	map<int, ALT_vwaa_node_t*>::const_iterator n_b, n_e;
	LTL_formul_t F1, F2;
	LTL_oper_t op;
	list<ALT_vwaa_node_t*>  C; // konjunkce stavu
	list<list<ALT_vwaa_node_t*> > LV1, LV2;
	ALT_vwaa_node_t *p_N;

	LV.erase(LV.begin(), LV.end());
	n_b = vwaa_node_list.begin(); n_e = vwaa_node_list.end();

	switch (F.typ()) {
	case LTL_LITERAL:
	case LTL_BOOL:
	case LTL_NEXT_OP:
		p_N = find_vwaa_node(F);
		C.push_back(p_N); LV.push_back(C);
		return; break;
	case LTL_BIN_OP:
		F.get_binar_op(op, F1, F2);
		switch (op) {
		case op_U:
		case op_V:
			p_N = find_vwaa_node(F);
			C.push_back(p_N); LV.push_back(C);
			return; break;
		case op_or:
			op_over(F1, LV1); op_over(F2, LV2);
			or_sum(LV1, LV2, LV);
			return; break;
		case op_and:
			op_over(F1, LV1); op_over(F2, LV2);
			and_sum(LV1, LV2, LV);
			return; break;
		}
	}
}

void ALT_graph_t::mkDELTA(LTL_formul_t& F, list<ALT_vwaa_trans_t> & LV)
{
	list<ALT_vwaa_trans_t>  LT1, LT2;
	ALT_vwaa_node_t *p_N;
	LTL_formul_t F1, F2;
	LTL_oper_t op;

	switch (F.typ()) {
	case LTL_LITERAL:
	case LTL_BOOL:
	case LTL_NEXT_OP:
		p_N = find_vwaa_node(F);
		mkdelta(p_N, LV);
		break;
	case LTL_BIN_OP:
		F.get_binar_op(op);
		switch (op) {
		case op_U:
		case op_V:
			p_N = find_vwaa_node(F);
			mkdelta(p_N, LV);
			break;
		case op_and:
			F.get_binar_op(op, F1, F2);
			mkDELTA(F1, LT1); mkDELTA(F2, LT2);
			op_times(LT1, LT2, LV);
			break;
		case op_or:
			F.get_binar_op(op, F1, F2);
			mkDELTA(F1, LT1); mkDELTA(F2, LT2);
			op_sum(LT1, LT2, LV);
			break;
		}
		break;
	}
}

void ALT_graph_t::mkdelta(ALT_vwaa_node_t *p_N, list<ALT_vwaa_trans_t> & LV)
{
	list<list<ALT_vwaa_node_t*> > LLN;
	list<list<ALT_vwaa_node_t*> >::iterator lln_b, lln_e, lln_i;
	bool b;
	ALT_vwaa_trans_t Tr;
	LTL_formul_t F1, F2;
	LTL_oper_t op;
	LTL_literal_t Lit;
	list<ALT_vwaa_trans_t>  LT1, LT2, LT3, LT4;

	LV.erase(LV.begin(), LV.end());

	switch (p_N->label.typ()) {
	case LTL_BOOL:
		p_N->label.get_bool(b);
		if (b) {
			LV.push_back(Tr);
		}
		break;
	case LTL_LITERAL:
		p_N->label.get_literal(Lit.negace, Lit.predikat);
		Tr.t_label.push_back(Lit);
		LV.push_back(Tr);
		break;
	case LTL_NEXT_OP:
		p_N->label.get_next_op(F1);
		op_over(F1, LLN);
		lln_b = LLN.begin(); lln_e = LLN.end();
		for (lln_i = lln_b; lln_i != lln_e; lln_i++) {
			Tr.target = *lln_i;
			LV.push_back(Tr);
		}
		break;
	case LTL_BIN_OP:
		p_N->label.get_binar_op(op, F1, F2);
		switch (op) {
		case op_U:
			mkDELTA(F2, LT1);
			Tr.target.push_back(p_N);
			LT2.push_back(Tr);
			mkDELTA(F1, LT3);
			op_times(LT2, LT3, LT4);
			op_sum(LT1, LT4, LV);
			break;
		case op_V:
			mkDELTA(F1, LT1);
			Tr.target.push_back(p_N);
			LT2.push_back(Tr);
			op_sum(LT1, LT2, LT3);
			mkDELTA(F2, LT4);
			op_times(LT3, LT4, LV);
			break;
                default:break;//nothing
		}
		break;
	}
}

void ALT_graph_t::create_graph(LTL_formul_t& F_in)
{
	LTL_formul_t F1, F2, F;
	LTL_oper_t op;
	queue<LTL_formul_t> QF;

	clear(); timer = 1;

	QF.push(F_in);
	while (!QF.empty()) {
		F = QF.front(); QF.pop();
		switch (F.typ()) {
		case LTL_BOOL:
		case LTL_LITERAL:
			//cerr << "add " << F << endl;
			add_vwaa_node(F); break;
		case LTL_NEXT_OP:
			//cerr << "add " << F << endl;
			add_vwaa_node(F);
			F.get_next_op(F1);
			QF.push(F1); break;
		case LTL_BIN_OP:
			F.get_binar_op(op, F1, F2);
			QF.push(F1); QF.push(F2);
			if ((op == op_U) || (op == op_V)) {
				//cerr << "add " << F << endl;
				add_vwaa_node(F);
			}
			break;
		}
	}

	/* cast druha - doplneni prechodove funkce */
	map<int, ALT_vwaa_node_t*>::iterator nl_b, nl_e, nl_i;
	ALT_vwaa_node_t *p_N;

	op_over(F_in, vwaa_init_nodes);

	nl_b = vwaa_node_list.begin();
	nl_e = vwaa_node_list.end();
	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		p_N = nl_i->second;
		mkdelta(p_N, p_N->adj);

		list<ALT_vwaa_trans_t> ::iterator tr_i, tr_j;
		tr_i = p_N->adj.begin();
		while (tr_i != p_N->adj.end()) {
			if (LTL_is_contradiction(tr_i->t_label)) {
				tr_j = tr_i; tr_i++;
				p_N->adj.erase(tr_j);
			} else {
				tr_i++;
			}
		}
	}

	/* cast treti - zjednoduseni automatu */
	map<int, ALT_vwaa_node_t*> pom_node_list;
	list<list<ALT_vwaa_node_t*> >::iterator in_i, in_e;
	list<ALT_vwaa_node_t*> ::iterator l_e, l_i;
	list<ALT_vwaa_trans_t> ::iterator ad_e, ad_i;
	queue<ALT_vwaa_node_t*> QN;

	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		nl_i->second->pom = false;
	}

	in_e = vwaa_init_nodes.end();
	for (in_i = vwaa_init_nodes.begin(); in_i != in_e; in_i++) {
		l_e = in_i->end();
		for (l_i = in_i->begin(); l_i != l_e; l_i++) {
			if (!(*l_i)->pom) {
				QN.push(*l_i);
				(*l_i)->pom = true;
			}
		}
	}

	while (!QN.empty()) {
		p_N = QN.front(); QN.pop();
		ad_e = p_N->adj.end();
		for (ad_i = p_N->adj.begin(); ad_i != ad_e; ad_i++) {
			l_e = ad_i->target.end();
			for (l_i = ad_i->target.begin(); l_i != l_e;
				l_i++) {
				if (!(*l_i)->pom) {
					QN.push(*l_i);
					(*l_i)->pom = true;
				}
			}
		}
	}

	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		if (nl_i->second->pom) {
			pom_node_list[nl_i->first] = nl_i->second;
		} else {
			delete nl_i->second;
		}
	}

	vwaa_node_list = pom_node_list;

	/* doplneni akceptujicich stavu */
	nl_b = vwaa_node_list.begin(); nl_e = vwaa_node_list.end();
	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		if (nl_i->second->is_accept())
			vwaa_accept_nodes.push_back(nl_i->second);
	}
}

void ALT_graph_t::vypis_vwaa()
{
	list<list<ALT_vwaa_node_t*> >::const_iterator in_b, in_e, in_i;
	list<ALT_vwaa_node_t*> ::const_iterator n_b, n_e, n_i;
	map<int, ALT_vwaa_node_t*>::const_iterator nl_b, nl_e, nl_i;
	list<ALT_vwaa_trans_t> ::const_iterator t_b, t_e, t_i;
	LTL_label_t::const_iterator f_b, f_e, f_i;
	bool b, b1;
	const ALT_vwaa_node_t *p_N;

	int node_counter = 0, trans_counter = 0, acc_counter = 0;

	cout << endl;
	cout << "Initial: {";
	in_b = vwaa_init_nodes.begin();
	in_e = vwaa_init_nodes.end();
	b = false;
	for (in_i = in_b; in_i != in_e; in_i++) {
		if (b) {
			cout << ", ";
		} else {
			b = true;
		}
		cout << "{";
		n_b = in_i->begin(); n_e = in_i->end();
		b1 = false;
		for (n_i = n_b; n_i != n_e; n_i++) {
			if (b1) {
				cout << ", ";
			} else {
				b1 = true;
			}
			cout << (*n_i)->name;
		}
		cout << "}";
	}
	cout << "}" << endl;

	//cerr << ALT_vwaa_node_t_list.size();
	nl_b = vwaa_node_list.begin();
	nl_e = vwaa_node_list.end();
	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		node_counter++;
		p_N = nl_i->second;
		cout << p_N->name << ": " << p_N->label << " -> {";
		t_b = p_N->adj.begin();
		t_e = p_N->adj.end();
		b = false;
		for (t_i = t_b; t_i != t_e; t_i++) {
			trans_counter++;
			if (b) {
				cout << ", ";
			} else {
				b = true;
			}
			cout << "(<";
			f_b = t_i->t_label.begin();
			f_e = t_i->t_label.end();
			b1 = false;
			for (f_i = f_b; f_i != f_e; f_i++) {
				if (b1) {
					cout << ", ";
				} else {
					b1 = true;
				}
				cout << *f_i;
			}
			cout << "> ; {";
			n_b = t_i->target.begin();
			n_e = t_i->target.end();
			b1 = false;
			for (n_i = n_b; n_i != n_e; n_i++) {
				if (b1) {
					cout << ", ";
				} else {
					b1 = true;
				}
				cout << (*n_i)->name;
			}
			cout << "})";
		}
		cout << "}" << endl;
	}

	cout << "Accept: {";
	b = false;
	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		if (nl_i->second->is_accept()) {
			acc_counter++;
			if (b) {
				cout << ", ";
			} else {
				b = true;
			}
			cout << nl_i->second->name;
		}
	}
	cout << "}" << endl << endl;

	cout << "nodes: " << node_counter << "; transitions: " <<
	trans_counter << "; initial: " << vwaa_init_nodes.size() <<
	"; accept nodes: " << acc_counter << endl;
}


/* GBA */

ALT_gba_node_t::~ALT_gba_node_t()
{
	list<ALT_gba_trans_t*>::iterator tr_e, tr_i;

	tr_e = adj.end();
	for (tr_i = adj.begin(); tr_i != tr_e; tr_i++) {
		delete *tr_i;
	}
}


/* k nalezeni uzlu pouzije seznam gba_equiv_nodes */
ALT_gba_node_t*
ALT_graph_t::find_gba_node(const list<ALT_vwaa_node_t*> & node_label)
{
	/*list< pair<list<list<ALT_vwaa_node_t*> >, ALT_gba_node_t*> >*/
	ALT_list_eq_gba_nodes_t::iterator eqn_b, eqn_e, eqn_i;
	list<list<ALT_vwaa_node_t*> >::iterator n_b, n_e, n_i;

	eqn_b = gba_equiv_nodes.begin(); eqn_e = gba_equiv_nodes.end();
	for (eqn_i = eqn_b; eqn_i != eqn_e; eqn_i++) {
		n_b = eqn_i->first.begin(); n_e = eqn_i->first.end();
		for (n_i = n_b; n_i != n_e; n_i++) {
			if (equiv_C(*n_i, node_label)) {
				return(eqn_i->second);
			}
		}
	}

	return(0);
}

ALT_gba_node_t* ALT_graph_t::mknode(const list<ALT_vwaa_node_t*> & LN,
	bool& b)
{
	ALT_gba_node_t *p_N1;

	p_N1 = find_gba_node(LN);

	if (p_N1 == 0) {
		p_N1 = new ALT_gba_node_t;
		p_N1->node_label = LN;
		p_N1->name = timer; timer++;
		gba_node_list.push_back(p_N1);

		ALT_eq_gba_nodes_t eqN;
		eqN.first.push_back(LN); eqN.second = p_N1;
		gba_equiv_nodes.push_back(eqN);

		/*cerr << "creating node " << p_N1->name << " : ";
		bool b1 = false;
		list<ALT_vwaa_node_t*> ::const_iterator n_b, n_e, n_i;
		n_b = LN.begin(); n_e = LN.end();
		for (n_i = n_b; n_i != n_e; n_i++) {
			if (b1) {
				cout << ", ";
			} else {
				b1 = true;
			}
			cout << (*n_i)->label;
		}
		cout << endl;*/

		b = true;
		return(p_N1);
	} else {
		b = false;
		return(p_N1);
	}
}

bool subset_eq(const list<ALT_vwaa_node_t*> & LN1,
	const list<ALT_vwaa_node_t*> & LN2)
{
	list<ALT_vwaa_node_t*> ::const_iterator l1_b, l1_e, l1_i, l2_b, l2_e,
		l2_i;

	if (LN1.size() > LN2.size()) return(false);

	l1_b = LN1.begin(); l1_e = LN1.end();
	l2_b = LN2.begin(); l2_e = LN2.end();
	for (l1_i = l1_b; l1_i != l1_e; l1_i++) {
		l2_i = find(l2_b, l2_e, *l1_i);
		if (l2_i == l2_e) return(false);
	}

	return(true);
}


bool ALT_graph_t::less_eq(ALT_vwaa_trans_t& T1, ALT_vwaa_trans_t& T2)
{ // T1 <= T2
	list<ALT_vwaa_node_t*> ::const_iterator ac_b, ac_e, ac_i;

	/* T1.target \subseteq T2.target */

	if (!subset_eq(T1.target, T2.target)) return(false);
	//if (!equiv_C(T1.target, T2.target)) return(false);

	/* \Sigma(T2.t_label) \subseteq \Sigma(T1.t_label) */
	/* T1.t_label \subseteq T2.t_label */

	//if (!subset_eq(T2.t_label, T1.t_label)) return(false);
	if (!LTL_subset_label(T1.t_label, T2.t_label)) return(false);


	ac_b = vwaa_accept_nodes.begin();
	ac_e = vwaa_accept_nodes.end();

	for (ac_i = ac_b; ac_i != ac_e; ac_i++) {
		if (is_acc_trans(T2, *ac_i)) {
			if (!is_acc_trans(T1, *ac_i)) {
				return(false);
			}
		}
	}

	return(true);
}

bool ALT_graph_t::is_acc_trans(ALT_vwaa_trans_t& T, ALT_vwaa_node_t *p_ac_N)
{
	list<ALT_vwaa_node_t*> ::const_iterator n_b, n_e, n_i, n_f;
	list<ALT_vwaa_trans_t> ::const_iterator t_b, t_e, t_i;

	n_b = T.target.begin(); n_e = T.target.end();
	n_i = find(n_b, n_e, p_ac_N);
	if (n_i == n_e) return(true);

	t_b = p_ac_N->adj.begin(); t_e = p_ac_N->adj.end();
	for (t_i = t_b; t_i != t_e; t_i++) {
		if (subset_eq(t_i->target, T.target)) {
			n_f = find(t_i->target.begin(), t_i->target.end(),
				p_ac_N);
			if (n_f == t_i->target.end()) {
				if (LTL_subset_label(t_i->t_label,
					T.t_label))
					return(true);
			}
		}
	}

	return(false);
}

bool ALT_graph_t::is_acc_trans(ALT_gba_trans_t& T, ALT_vwaa_node_t *p_ac_N)
{
	list<ALT_vwaa_node_t*> ::const_iterator n_b, n_e, n_i, n_f;
	list<ALT_vwaa_trans_t> ::const_iterator t_b, t_e, t_i;

	n_b = T.p_to->node_label.begin();
	n_e = T.p_to->node_label.end();

	//n_b = T.target.begin(); n_e = T.target.end();
	n_i = find(n_b, n_e, p_ac_N);
	if (n_i == n_e) return(true);

	t_b = p_ac_N->adj.begin(); t_e = p_ac_N->adj.end();
	for (t_i = t_b; t_i != t_e; t_i++) {
		if (subset_eq(t_i->target, T.p_to->node_label)) {
			n_f = find(t_i->target.begin(), t_i->target.end(),
				p_ac_N);
			if (n_f == t_i->target.end()) {
				if (LTL_subset_label(t_i->t_label,
					T.t_label))
					return(true);
			}
		}
	}

	return(false);
}

void ALT_graph_t::mkdelta(ALT_gba_node_t *p_N,
	list<ALT_gba_node_t*>& new_nodes)
{
	list<ALT_vwaa_trans_t>  LT, LT1;
	list<ALT_vwaa_node_t*> ::iterator vt_b, vt_e, vt_i, vt_i1;
	list<ALT_vwaa_trans_t> ::iterator tr_b, tr_e, tr_i, tr_i1, tr_i2;

	list<ALT_vwaa_node_t*> ::iterator ac_b, ac_e, ac_i;
	list<list<ALT_gba_trans_t*> >::iterator sas_i;

	ac_b = vwaa_accept_nodes.begin();
	ac_e = vwaa_accept_nodes.end();

	new_nodes.erase(new_nodes.begin(), new_nodes.end());

	if (p_N->node_label.size() == 0) {
		ALT_gba_trans_t *p_Tr;

		p_Tr = new ALT_gba_trans_t;
		p_Tr->p_from = p_N; p_Tr->p_to = p_N;
		p_N->adj.push_back(p_Tr);

		if (simp_GBA) {
			sas_i = acc_set.begin();
			for (ac_i = ac_b; ac_i != ac_e; ac_i++) {
				if (is_acc_trans(*p_Tr, *ac_i)) {
					sas_i->push_back(p_Tr);
				}
				sas_i++;
			}
		}

		return;
	}


	if (p_N->node_label.size() == 1) {
		LT = (p_N->node_label.front())->adj;
	} else {
		vt_b = p_N->node_label.begin();
		vt_e = p_N->node_label.end();
		vt_i = vt_b; vt_i1 = vt_b; vt_i1++;
		op_times((*vt_i)->adj, (*vt_i1)->adj, LT);
		if (p_N->node_label.size() > 2)
			for (vt_i1++; vt_i1 != vt_e; vt_i1++) {
				op_times(LT, (*vt_i1)->adj, LT1);
				LT = LT1;
			}
	}

	tr_b = LT.begin(); tr_e = LT.end();

/**/
	tr_i = LT.begin();
	while (tr_i != LT.end()) {
		if (LTL_is_contradiction(tr_i->t_label)) {
			tr_i2 = tr_i; tr_i++;
			LT.erase(tr_i2);
			continue;
		}

		tr_i1 = LT.begin();
		while (tr_i1 != LT.end()) {
			if (tr_i1 == tr_i) {
				tr_i1++;
				continue;
			}
			if (less_eq(*tr_i, *tr_i1)) {
				tr_i2 = tr_i1; tr_i2++;
				LT.erase(tr_i1);
				tr_i1 = tr_i2;
			} else {
				tr_i1++;
			}
		}
		tr_i++;
	}
/**/

/*
Pri on-the-fly optimalizaci: porovnat vygenerovane prechody v LT a
zjednodussit
*/

	if (simp_GBA) {
		ALT_list_eq_gba_nodes_t::iterator eqn_b, eqn_e, eqn_i;
		list<list<ALT_vwaa_node_t*> >::iterator lvn_b, lvn_e, lvn_i;
		bool is_new;

		eqn_b = gba_equiv_nodes.begin();
		eqn_e = gba_equiv_nodes.end();

		tr_i = LT.begin();
		while (tr_i != LT.end()) {
			for (eqn_i = eqn_b; eqn_i != eqn_e; eqn_i++) {
				lvn_b = eqn_i->first.begin();
				lvn_e = eqn_i->first.end();
				for (lvn_i = lvn_b; lvn_i != lvn_e;
					lvn_i++) {
					if (equiv_C(*lvn_i, tr_i->target))
						break;
				}
				if (lvn_i != lvn_e) break;
			}
			is_new = (eqn_i == eqn_e);

			tr_i1 = LT.begin();
			while (tr_i1 != LT.end()) {
				if (tr_i == tr_i1) {
					tr_i1++; continue;
				}
				if (!LTL_subset_label(tr_i->t_label,
					tr_i1->t_label)) {
					tr_i1++; continue;
				}

				if (is_new) {
					if (!equiv_C(tr_i->target,
					tr_i1->target)) {
						tr_i1++; continue;
					}
				} else {
					for (lvn_i = lvn_b; lvn_i != lvn_e;
					lvn_i++) {
					if (equiv_C(tr_i1->target,*lvn_i))
						break;
					}
					if (lvn_i == lvn_e) {
					tr_i1++; continue;
					}
				}

				for (ac_i = ac_b; ac_i != ac_e; ac_i++) {
				if (is_acc_trans(*tr_i1, *ac_i))
					if (!is_acc_trans(*tr_i, *ac_i))
						break;
				}
				if (ac_i == ac_e) {
					tr_i2 = tr_i1; tr_i1++;
					LT.erase(tr_i2);
				} else {
					tr_i1++;
				}
			}
			tr_i++;
		}
	}

	ALT_gba_node_t *p_N1;
	ALT_gba_trans_t *p_Tr;
	bool b;

	tr_b = LT.begin(); tr_e = LT.end();
	for (tr_i = tr_b; tr_i != tr_e; tr_i++) {
		p_N1 = mknode(tr_i->target, b);
		if (b) {
			new_nodes.push_back(p_N1);
		}
		p_Tr = new ALT_gba_trans_t;
		p_Tr->t_label = tr_i->t_label;
		p_Tr->p_from = p_N;
		p_Tr->p_to = p_N1;
		p_N->adj.push_back(p_Tr);

		if (simp_GBA) {
			sas_i = acc_set.begin();
			for (ac_i = ac_b; ac_i != ac_e; ac_i++) {
				if (is_acc_trans(*p_Tr, *ac_i)) {
					sas_i->push_back(p_Tr);
				}
				sas_i++;
			}
		}
	}
}

void ALT_graph_t::transform_vwaa()
{
	map<int, ALT_vwaa_node_t*>::const_iterator vwl_b, vwl_e;
	list<list<ALT_vwaa_node_t*> >::const_iterator vwi_b, vwi_e, vwi_i;

	list<ALT_vwaa_node_t*> ::iterator ac_b, ac_e, ac_i;
	list<list<ALT_gba_trans_t*> >::iterator sas_i;
	list<ALT_gba_trans_t*>::iterator tr_i1;

	queue<ALT_gba_node_t*> QN;
	ALT_gba_node_t *p_N;
	bool b, b1;
	list<ALT_gba_node_t*> new_nodes;
	list<ALT_gba_node_t*>::iterator nn_b, nn_e, nn_i;

	clear_GB();
	timer = 1;

	ac_b = vwaa_accept_nodes.begin();
	ac_e = vwaa_accept_nodes.end();

	if (simp_GBA) {
		list<ALT_gba_trans_t*> L;

		for (ac_i = ac_b; ac_i != ac_e; ac_i++) {
			acc_set.push_back(L);
		}
	}

	vwl_b = vwaa_node_list.begin(); vwl_e = vwaa_node_list.end();
	vwi_b = vwaa_init_nodes.begin(); vwi_e = vwaa_init_nodes.end();

	for (vwi_i = vwi_b; vwi_i != vwi_e; vwi_i++) {
		p_N = mknode(*vwi_i, b);
		if (b) {
			QN.push(p_N);
			gba_init_nodes.push_back(p_N);
		}
	}

	while (!QN.empty()) {
		p_N = QN.front(); QN.pop();
		//cerr << "creating delta of " << p_N->name << endl;
		mkdelta(p_N, new_nodes);
		nn_b = new_nodes.begin(); nn_e = new_nodes.end();
		for (nn_i = nn_b; nn_i != nn_e; nn_i++) {
			QN.push(*nn_i);
		}

		if (simp_GBA && new_nodes.empty()) {
		ALT_list_eq_gba_nodes_t::iterator eqn_b, eqn_e, eqn_i, eqn_i1;
		list<ALT_gba_trans_t*>::iterator tr_b, tr_e, tr_i;
		list<ALT_gba_node_t*>::iterator gn_f;
		ALT_gba_node_t *p_N1;

			eqn_b = gba_equiv_nodes.begin();
			eqn_e = gba_equiv_nodes.end();
			for (eqn_i = eqn_b; eqn_i != eqn_e; eqn_i++) {
				if ((eqn_i->second != p_N) &&
				equiv_ALT_gba_node_ts(p_N, eqn_i->second)) {
					break;
				}
			}

			if (eqn_i != eqn_e) {
				b1 = false;
				p_N1 = eqn_i->second;
				for (eqn_i1 = eqn_b; eqn_i1 != eqn_e;
					eqn_i1++) {
					if (eqn_i1->second == p_N)
						break;
				}
				eqn_i->first.insert(eqn_i->first.end(),
					eqn_i1->first.begin(),
					eqn_i1->first.end());
				gba_equiv_nodes.erase(eqn_i1);

				eqn_b = gba_equiv_nodes.begin();
				eqn_e = gba_equiv_nodes.end();
				for (eqn_i = eqn_b; eqn_i != eqn_e;
					eqn_i++) {
					tr_b = eqn_i->second->adj.begin();
					tr_e = eqn_i->second->adj.end();
					b = false;
					for (tr_i = tr_b; tr_i != tr_e;
						tr_i++) {
						if ((*tr_i)->p_to == p_N){
						(*tr_i)->p_to = p_N1;
						b = true;
						}
					}
					if (b) {
					simp_gba_trans(eqn_i->second);
					//b1 = true;
					}
				}

				gn_f = find(gba_node_list.begin(),
					gba_node_list.end(), p_N);
				gba_node_list.erase(gn_f);

				/*if (b1) {
					simp_ALT_gba_node_ts();
				}*/

				sas_i = acc_set.begin();
				for (ac_i = ac_b; ac_i != ac_e; ac_i++) {
					tr_i = sas_i->begin();
					while (tr_i != sas_i->end()) {
					if ((*tr_i)->p_from == p_N) {
						tr_i1 = tr_i; tr_i++;
						sas_i->erase(tr_i1);
					} else {
						tr_i++;
					}
					}
					sas_i++;
				}

				delete p_N;
			}
		}
	}

	/* doplneni akc. hran */
	if (!simp_GBA) {

	list<ALT_vwaa_node_t*> ::iterator va_b, va_e, va_i;
	list<ALT_gba_node_t*>::iterator nl_b, nl_e, nl_i;
	list<ALT_gba_trans_t*> LT;
	list<ALT_gba_trans_t*>::iterator tr_b, tr_e, tr_i;

	va_b = vwaa_accept_nodes.begin();
	va_e = vwaa_accept_nodes.end();
	nl_b = gba_node_list.begin(); nl_e = gba_node_list.end();
	for (va_i = va_b; va_i != va_e; va_i++) {
		for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
			tr_b = (*nl_i)->adj.begin();
			tr_e = (*nl_i)->adj.end();
			for (tr_i = tr_b; tr_i != tr_e; tr_i++) {
				if (is_acc_trans(**tr_i, *va_i))
					LT.push_back(*tr_i);
			}
		}
		acc_set.push_back(LT);
		LT.erase(LT.begin(), LT.end());
	}

	}
}

bool ALT_graph_t::tr_implies(ALT_gba_trans_t *p_T1, ALT_gba_trans_t *p_T2)
{ // T1 => T2
	if (p_T1->p_to != p_T2->p_to) return(false);

	if (!LTL_subset_label(p_T1->t_label, p_T2->t_label))
		return(false);

	list<ALT_vwaa_node_t*> ::iterator ac_b, ac_e, ac_i;

	ac_b = vwaa_accept_nodes.begin();
	ac_e = vwaa_accept_nodes.end();
	for (ac_i = ac_b; ac_i != ac_e; ac_i++) {
		if (is_acc_trans(*p_T2, *ac_i)) {
			if (!is_acc_trans(*p_T1, *ac_i)) {
				return(false);
			}
		}
	}

	return(true);
}

bool equiv_ALT_gba_node_ts(ALT_gba_node_t *p_N1, ALT_gba_node_t *p_N2)
{
	list<ALT_gba_trans_t*>::iterator a1_b, a1_e, a1_i, a2_b, a2_e, a2_i;

	if (p_N1->adj.size() != p_N2->adj.size()) return(false);

	a1_b = p_N1->adj.begin(); a1_e = p_N1->adj.end();
	a2_b = p_N2->adj.begin(); a2_e = p_N2->adj.end();

	for (a1_i = a1_b; a1_i != a1_e; a1_i++) {
		for (a2_i = a2_b; a2_i != a2_e; a2_i++) {
			if ((*a1_i)->p_to == (*a2_i)->p_to) {
				if (LTL_equiv_labels((*a1_i)->t_label,
					(*a2_i)->t_label)) {
					break;
				}
			}
		}
		if (a2_i == a2_e) return(false);
	}

	return(true);
}

void ALT_graph_t::simp_gba_trans(ALT_gba_node_t *p_N)
{
	list<ALT_gba_trans_t*>::iterator t_i1, t_i2, t_pom, t_b, t_e, t_i;
	list<ALT_gba_trans_t*> del_trans;
	list<list<ALT_gba_trans_t*> >::iterator sas_b, sas_e, sas_i;

	sas_b = acc_set.begin(); sas_e = acc_set.end();

	t_i1 = p_N->adj.begin();
	while (t_i1 != p_N->adj.end()) {
		t_i2 = p_N->adj.begin();
		while (t_i2 != p_N->adj.end()) {
			if (t_i1 == t_i2) {
				t_i2++; continue;
			}
			if ((*t_i1)->p_to != (*t_i2)->p_to) {
				t_i2++; continue;
			}
			if (!LTL_subset_label((*t_i1)->t_label,
				(*t_i2)->t_label)) {
				t_i2++; continue;
			}
			for (sas_i = sas_b; sas_i != sas_e; sas_i++) {
				t_b = sas_i->begin(); t_e = sas_i->end();
				t_pom = find(t_b, t_e, *t_i2);
				if (t_pom != t_e) {
					t_pom = find(t_b, t_e, *t_i1);
					if (t_pom == t_e) break;
				}
			}

			if (sas_i == sas_e) {
				del_trans.push_back(*t_i2);
				t_pom = t_i2; t_i2++;
				p_N->adj.erase(t_pom);
			} else {
				t_i2++;
			}
		}
		t_i1++;
	}

	t_b = del_trans.begin(); t_e = del_trans.end();
	for (t_i = t_b; t_i != t_e; t_i++) {
		for (sas_i = sas_b; sas_i != sas_e; sas_i++) {
			t_pom = find(sas_i->begin(), sas_i->end(), *t_i);
			if (t_pom != sas_i->end()) {
				sas_i->erase(t_pom);
			}
		}
		delete *t_i;
	}
}

/*void ALT_graph_t::simp_ALT_gba_node_ts()
{

}*/

void ALT_graph_t::simplify_GBA()
{
	list<ALT_gba_node_t*>::iterator nl_b, nl_e, nl_i, nl_i1;
	list<ALT_gba_trans_t*>::iterator tr_b, tr_e, tr_i, tr_i1, tr_i2;
	list<ALT_gba_trans_t*> *p_adj;

	nl_b = gba_node_list.begin(); nl_e = gba_node_list.end();

	/* Odstraneni redundantnich hran */
	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		p_adj = &((*nl_i)->adj);
		for (tr_i = p_adj->begin(); tr_i != p_adj->end();
			tr_i++) {
			tr_i1 = p_adj->begin();
			while (tr_i1 != p_adj->end()) {
				if (tr_i == tr_i1) {
					tr_i1++;
					continue;
				}
				if (tr_implies(*tr_i, *tr_i1)) {
					tr_i2 = tr_i1; tr_i2++;
					delete *tr_i1;
					p_adj->erase(tr_i1);
					tr_i1 = tr_i2;
				} else {
					tr_i1++;
				}
			}
		}
	}

	/* spojeni ekvivalentnich stavu */
	map<int, ALT_gba_node_t*> old_name_list; // stare jmeno, novy uzel
	map<int, ALT_gba_node_t*>::iterator on_f;
	set<ALT_gba_node_t*> new_init_nodes;
	list<ALT_gba_node_t*> equiv_states, old_node_list;
	list<ALT_gba_node_t*> new_node_list;
	list<ALT_gba_node_t*>::iterator eq_b, eq_e, eq_i;
	list<ALT_gba_node_t*>::iterator nn_b, nn_e, nn_i;
	//list<ALT_gba_trans_t*>::iterator tr_b, tr_e, tr_i;
	ALT_gba_node_t *p_N, *p_N1;
	ALT_gba_trans_t *p_Tr;

	timer = 1;

	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		on_f = old_name_list.find((*nl_i)->name);
		if (on_f != old_name_list.end()) continue;

		equiv_states.push_back(*nl_i);
		for (nl_i1 = nl_b; nl_i1 != nl_e; nl_i1++) {
			if (nl_i == nl_i1) continue;
			if (equiv_ALT_gba_node_ts(*nl_i, *nl_i1)) {
				equiv_states.push_back(*nl_i1);
			}
		}

		p_N = new ALT_gba_node_t;
		p_N->name = timer; timer++;

		eq_b = equiv_states.begin();
		eq_e = equiv_states.end();
		for (eq_i = eq_b; eq_i != eq_e; eq_i++) {
			old_name_list[(*eq_i)->name] = p_N;
		}
		old_node_list.push_back(*eq_b);
		new_node_list.push_back(p_N);
		equiv_states.erase(eq_b, eq_e);
	}

	//if (new_node_list.size() == ALT_gba_node_t_list.size()) return;

	nn_b = old_node_list.begin(); nn_e = old_node_list.end();

	for (nn_i = nn_b; nn_i != nn_e; nn_i++) {
		p_N = old_name_list[(*nn_i)->name];
		tr_b = (*nn_i)->adj.begin(); tr_e = (*nn_i)->adj.end();
		for (tr_i = tr_b; tr_i != tr_e; tr_i++) {
			p_Tr = new ALT_gba_trans_t;
			p_Tr->p_from = p_N;
			p_Tr->p_to = old_name_list[(*tr_i)->p_to->name];
			p_Tr->t_label = (*tr_i)->t_label;
			p_N->adj.push_back(p_Tr);
		}
	}

	nn_b = gba_init_nodes.begin(); nn_e = gba_init_nodes.end();
	for (nn_i = nn_b; nn_i != nn_e; nn_i++) {
		new_init_nodes.insert(old_name_list[(*nn_i)->name]);
	}

	/* doplneni akceptujicich hran */
	set<ALT_gba_trans_t*> new_acc_trans;
	list<set<ALT_gba_trans_t*> > new_acc_set;
	list<set<ALT_gba_trans_t*> >::iterator las_b, las_e, las_i;
	list<ALT_gba_trans_t*>::iterator as_b, as_e, as_i;
	list<list<ALT_gba_trans_t*> >::iterator ass_b, ass_e, ass_i;

	ass_b = acc_set.begin(); ass_e = acc_set.end();
	for (ass_i = ass_b; ass_i != ass_e; ass_i++) {
		as_b = ass_i->begin(); as_e = ass_i->end();
		for (as_i = as_b; as_i != as_e; as_i++) {
			p_N = old_name_list[(*as_i)->p_from->name];
			p_N1 = old_name_list[(*as_i)->p_to->name];
			tr_b = p_N->adj.begin(); tr_e = p_N->adj.end();
			for (tr_i = tr_b; tr_i != tr_e; tr_i++) {
				if ( ((*tr_i)->p_to == p_N1) &&
				LTL_equiv_labels((*as_i)->t_label,
					(*tr_i)->t_label) ) {
					new_acc_trans.insert(*tr_i);
					//break;
				}
			}
		}
		new_acc_set.push_back(new_acc_trans);
		new_acc_trans.erase(new_acc_trans.begin(),
			new_acc_trans.end());
	}

	/* zapsani noveho grafu */
	set<ALT_gba_node_t*>::iterator sn_b, sn_e, sn_i;
	set<ALT_gba_trans_t*>::iterator st_b, st_e, st_i;
	list<ALT_gba_trans_t*> new_acc_trans_list;

	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		delete *nl_i;
	}
	gba_node_list.erase(nl_b, nl_e);
	acc_set.erase(acc_set.begin(), acc_set.end());
	gba_init_nodes.erase(gba_init_nodes.begin(),
		gba_init_nodes.end());

	gba_node_list = new_node_list;
	sn_b = new_init_nodes.begin(); sn_e = new_init_nodes.end();
	for (sn_i = sn_b; sn_i != sn_e; sn_i++) {
		gba_init_nodes.push_back(*sn_i);
	}

	las_b = new_acc_set.begin(); las_e = new_acc_set.end();
	for (las_i = las_b; las_i != las_e; las_i++) {
		st_b = las_i->begin(); st_e = las_i->end();
		for (st_i = st_b; st_i != st_e; st_i++) {
			new_acc_trans_list.push_back(*st_i);
		}
		acc_set.push_back(new_acc_trans_list);
		new_acc_trans_list.erase(new_acc_trans_list.begin(),
			new_acc_trans_list.end());
	}
}

bool eq_trans(const list<ALT_ba_trans_t>& T1, const list<ALT_ba_trans_t>& T2)
{
	list<ALT_ba_trans_t>::const_iterator t1_b, t1_e, t1_i,
		t2_b, t2_e, t2_i;

	if (T1.size() != T2.size()) return(false);

	t1_b = T1.begin(); t1_e = T1.end();
	t2_b = T2.begin(); t2_e = T2.end();

	for (t1_i = t1_b; t1_i != t1_e; t1_i++) {
		for (t2_i = t2_b; t2_i != t2_e; t2_i++) {
			if ((t1_i->target == t2_i->target) &&
			LTL_equiv_labels(t1_i->t_label, t2_i->t_label) ) {
				break;
			}
		}
		if (t2_i == t2_e) return(false);
	}

	for (t2_i = t2_b; t2_i != t2_e; t2_i++) {
		for (t1_i = t1_b; t1_i != t1_e; t1_i++) {
			if ((t1_i->target == t2_i->target) &&
			LTL_equiv_labels(t1_i->t_label, t2_i->t_label) ) {
				break;
			}
		}
		if (t1_i == t1_e) return(false);
	}

	return(true);
}

void ALT_graph_t::sim_BA_edges()
{
	list<ALT_ba_node_t*>::iterator nl_b, nl_e, nl_i;
	list<ALT_ba_trans_t>::iterator tr_i, tr_j, tr_k;

	nl_b = ba_node_list.begin(); nl_e = ba_node_list.end();
	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		tr_i = (*nl_i)->adj.begin();
		while (tr_i != (*nl_i)->adj.end()) {
			tr_j = (*nl_i)->adj.begin();
			while (tr_j != (*nl_i)->adj.end()) {
				if (tr_j == tr_i) {
					tr_j++; continue;
				}
				if ((tr_i->target == tr_j->target) &&
					LTL_subset_label(tr_i->t_label,
					tr_j->t_label)) {
					tr_k = tr_j; tr_j++;
					(*nl_i)->adj.erase(tr_k);
				} else {
					tr_j++;
				}
			}
			tr_i++;
		}
	}

}

void ALT_graph_t::simplify_BA()
{
	sim_BA_edges();

	/* spojeni ekvivalentnich stavu */
	list<ALT_ba_node_t*>::iterator nl_b, nl_e, nl_i, nl_j;
	list<ALT_ba_trans_t>::iterator tr_b, tr_e, tr_i;	

	list<list<ALT_ba_node_t*> > list_equiv_nodes;
	list<list<ALT_ba_node_t*> >::iterator len_b, len_e, len_i;
	list<ALT_ba_node_t*> equiv_nodes;
	list<ALT_ba_node_t*>::iterator en_b, en_e, en_i;
	list<ALT_ba_node_t*>::iterator acn_b, acn_e, acn_f,
		inn_b, inn_e, inn_f;
	list<ALT_ba_node_t*> new_node_list, new_init_nodes, new_accept_nodes;
	set<ALT_ba_node_t*> zpracovano;
	set<ALT_ba_node_t*>::iterator zp_f;
	bool acc_i, acc_j;

	nl_b = ba_node_list.begin(); nl_e = ba_node_list.end();
	acn_b = ba_accept_nodes.begin(); acn_e = ba_accept_nodes.end();
	inn_b = ba_init_nodes.begin(); inn_e = ba_init_nodes.end();

	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		zp_f = zpracovano.find(*nl_i);
		if (zp_f != zpracovano.end()) continue;
		zpracovano.insert(*nl_i);
		equiv_nodes.erase(equiv_nodes.begin(), equiv_nodes.end());
		equiv_nodes.push_back(*nl_i);

		acn_f = find(acn_b, acn_e, *nl_i);
		acc_i = (acn_f != acn_e);
		for (nl_j = nl_i, nl_j++; nl_j != nl_e; nl_j++) {
			zp_f = zpracovano.find(*nl_j);
			if (zp_f != zpracovano.end()) continue;

			acn_f = find(acn_b, acn_e, *nl_j);
			acc_j = (acn_f != acn_e);
			if (acc_i != acc_j) continue;
			if (eq_trans((*nl_i)->adj, (*nl_j)->adj)) {
				zpracovano.insert(*nl_j);
				equiv_nodes.push_back(*nl_j);
			}
		}
		list_equiv_nodes.push_back(equiv_nodes);
	}

	if (list_equiv_nodes.size() == ba_node_list.size()) return;

	/* */
	map<int, ALT_ba_node_t*> old_name_list;
	ALT_ba_node_t *p_N, *p_N1;
	ALT_ba_trans_t Tr;
	int i = 1;

	len_b = list_equiv_nodes.begin();
	len_e = list_equiv_nodes.end();
	for (len_i = len_b; len_i != len_e; len_i++, i++) {
		p_N = new ALT_ba_node_t; p_N->name = i;
		p_N->p_old = 0;
		new_node_list.push_back(p_N);
		en_b = len_i->begin(); en_e = len_i->end();
		for (en_i = en_b; en_i != en_e; en_i++) {
			old_name_list[(*en_i)->name] = p_N;
		}
	}

	for (len_i = len_b; len_i != len_e; len_i++) {
		en_b = len_i->begin(); en_e = len_i->end();
		p_N = old_name_list[(*en_b)->name];
		tr_b = (*en_b)->adj.begin(); tr_e = (*en_b)->adj.end();
		for (tr_i = tr_b; tr_i != tr_e; tr_i++) {
			p_N1 = old_name_list[tr_i->target->name];
			Tr.target = p_N1;
			Tr.t_label = tr_i->t_label;
			p_N->adj.push_back(Tr);
		}
		acn_f = find(acn_b, acn_e, *en_b);
		if (acn_f != acn_e) new_accept_nodes.push_back(p_N);
		for (en_i = en_b; en_i != en_e; en_i++) {
			inn_f = find(inn_b, inn_e, *en_i);
			if (inn_f != inn_e) {
				new_init_nodes.push_back(p_N);
				break;
			}
		}
	}

	/* Zapsani noveho grafu */
	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		delete (*nl_i);
	}

	ba_node_list = new_node_list;
	ba_init_nodes = new_init_nodes;
	ba_accept_nodes = new_accept_nodes;

	sim_BA_edges();
}

void ALT_graph_t::simplify_on_the_fly(bool s)
{
	simp_GBA = s;
}

void ALT_graph_t::vypis_GBA()
{
	list<ALT_gba_node_t*>::const_iterator nl_b, nl_e, nl_i;
	list<ALT_gba_trans_t*>::const_iterator tr_b, tr_e, tr_i;
	list<ALT_vwaa_node_t*> ::const_iterator vn_b, vn_e, vn_i;
	LTL_label_t::const_iterator lf_b, lf_e, lf_i;
	bool b, b1;

	int node_counter = 0, trans_counter = 0;

	cout << endl;
	cout << "Initial: {";
	nl_b = gba_init_nodes.begin(); nl_e = gba_init_nodes.end();
	b = false;
	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		if (b) {
			cout << ", ";
		} else {
			b = true;
		}
		cout << (*nl_i)->name;
	}
	cout << "}" << endl;

	nl_b = gba_node_list.begin(); nl_e = gba_node_list.end();
	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		node_counter++;
		if ((*nl_i)->node_label.size() > 0) {
			cout << (*nl_i)->name << ": {";
			vn_b = (*nl_i)->node_label.begin();
			vn_e = (*nl_i)->node_label.end();
			b = false;
			for (vn_i = vn_b; vn_i != vn_e; vn_i++) {
				if (b) {
					cout << ", ";
				} else {
					b = true;
				}
				cout << (*vn_i)->label;
			}
			cout << "} -> {";
		} else {
			cout << (*nl_i)->name << " -> {";
		}

		tr_b = (*nl_i)->adj.begin();
		tr_e = (*nl_i)->adj.end();
		b = false;
		for (tr_i = tr_b; tr_i != tr_e; tr_i++) {
			trans_counter++;
			if (b) {
				cout << ", ";
			} else {
				b = true;
			}
			cout << "(<";
			lf_b = (*tr_i)->t_label.begin();
			lf_e = (*tr_i)->t_label.end();
			b1 = false;
			for (lf_i = lf_b; lf_i != lf_e; lf_i++) {
				if (b1) {
					cout << ", ";
				} else {
					b1 = true;
				}
				cout << *lf_i;
			}
			cout << "> - " << (*tr_i)->p_to->name;
			cout << ")";
		}
		cout << "}" << endl;
	}

	list<list<ALT_gba_trans_t*> >::iterator aa_b, aa_e, aa_i;
	list<ALT_gba_trans_t*>::iterator a_b, a_e, a_i;

	cout << "Accept sets of set of transitions:" << endl;
	aa_b = acc_set.begin(); aa_e = acc_set.end();
	for (aa_i = aa_b; aa_i != aa_e; aa_i++) {
		cout << "{";
		a_b = aa_i->begin(); a_e = aa_i->end();
		b = false;
		for (a_i = a_b; a_i != a_e; a_i++) {
			if (b) {
				cout << ", ";
			} else {
				b = true;
			}
			cout << "(" << (*a_i)->p_from->name;
			cout << " - <";
			lf_b = (*a_i)->t_label.begin();
			lf_e = (*a_i)->t_label.end();
			b1 = false;
			for (lf_i = lf_b; lf_i != lf_e; lf_i++) {
				if (b1) {
					cout << ", ";
				} else {
					b1 = true;
				}
				cout << *lf_i;
			}
			cout << "> -> " << (*a_i)->p_to->name;
			cout << ")";
		}
		cout << "}" << endl;
	}

	cout << endl;
	cout << "nodes: " << node_counter << "; transitions: " <<
	trans_counter << "; initial: " << gba_init_nodes.size() <<
	"; accept sets: " << acc_set.size() << endl;
}

ALT_ba_node_t* ALT_graph_t::add_ba_node(ALT_gba_node_t *p_gN, int citac)
{
	list<ALT_ba_node_t*>::iterator nl_b, nl_e, nl_i;
	ALT_ba_node_t *p_N;

	nl_b = ba_node_list.begin(); nl_e = ba_node_list.end();
	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		if (((*nl_i)->p_old == p_gN) &&
			((*nl_i)->citac == citac)) break;
	}

	if (nl_i == nl_e) {
		p_N = new ALT_ba_node_t;
		p_N->name = timer; timer++;
		p_N->citac = citac;
		p_N->p_old = p_gN;
		ba_node_list.push_back(p_N);
		return(p_N);
	} else {
		return(*nl_i);
	}
}

void ALT_graph_t::add_ba_trans(int j, ALT_gba_trans_t *p_gt,
	ALT_ba_trans_t& Tr)
{
	list<list<ALT_gba_trans_t*> >::iterator llt_b, llt_e, llt_i;
	list<ALT_gba_trans_t*>::iterator lt_e, lt_i;
	int i, r;

	r = acc_set.size();
	if ((j > r) || (j < 0)) return;

	llt_b = acc_set.begin(); llt_e = acc_set.end();

	if (j != r) {
		i = 0;
		for (llt_i = llt_b; i < j; llt_i++, i++) ;
		//llt_i++; i++;
		for (; llt_i != llt_e; llt_i++, i++) {
			lt_e = llt_i->end();
			lt_i = find(llt_i->begin(), lt_e, p_gt);
			if (lt_i == lt_e) break;
		}
		Tr.target = add_ba_node(p_gt->p_to, i);
	} else {
		i = 0;
		for (llt_i = llt_b; llt_i != llt_e; llt_i++, i++) {
			lt_e = llt_i->end();
			lt_i = find(llt_i->begin(), lt_e, p_gt);
			if (lt_i == lt_e) break;
		}
		Tr.target = add_ba_node(p_gt->p_to, i);
	}

	Tr.t_label = p_gt->t_label;
}

void ALT_graph_t::degeneralize()
{
	list<ALT_gba_node_t*>::const_iterator ln_b, ln_e, ln_i;
	list<ALT_gba_trans_t*>::const_iterator gt_b, gt_e, gt_i;
	list<ALT_ba_node_t*>::iterator ba_b, ba_i;
	int s, t;;
	ALT_ba_node_t *p_bN;
	ALT_ba_trans_t Tr;

	timer = 1; s = acc_set.size();
	ln_b = gba_init_nodes.begin(); ln_e = gba_init_nodes.end();
	for (ln_i = ln_b; ln_i != ln_e; ln_i++) {
		p_bN = new ALT_ba_node_t;
		p_bN->name = timer; timer++;
		p_bN->citac = 0;
		p_bN->p_old = (*ln_i);
		ba_node_list.push_back(p_bN);
		ba_init_nodes.push_back(p_bN);
	}

	ba_b = ba_node_list.begin();
	for (ba_i = ba_b; ba_i != ba_node_list.end(); ba_i++) {
		t = (*ba_i)->citac;
		if (t == s) {
			ba_accept_nodes.push_back(*ba_i);
		}
		gt_b = (*ba_i)->p_old->adj.begin();
		gt_e = (*ba_i)->p_old->adj.end();
		for (gt_i = gt_b; gt_i != gt_e; gt_i++) {
			add_ba_trans(t, *gt_i, Tr);
			(*ba_i)->adj.push_back(Tr);
		}
	}
}

void ALT_graph_t::vypis_BA()
{
	list<ALT_ba_node_t*>::const_iterator nl_b, nl_e, nl_i;
	list<ALT_ba_trans_t>::const_iterator tr_b, tr_e, tr_i;
	LTL_label_t::const_iterator tl_b, tl_e, tl_i;
	bool b, b1;
	int trans_counter = 0;

	cout << "Initial: {";
	b = false;
	nl_b = ba_init_nodes.begin(); nl_e = ba_init_nodes.end();
	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		if (b) {
			cout << ", ";
		} else {
			b = true;
		}
		cout << (*nl_i)->name;
	}
	cout << "}" << endl;

	nl_b = ba_node_list.begin(); nl_e = ba_node_list.end();
	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		cout << (*nl_i)->name;

		cout << ":";

		if ((*nl_i)->p_old != 0) {
			cout << " (" << (*nl_i)->p_old->name << ", ";
			cout << (*nl_i)->citac << ")";
		}

		cout << " -> {";
		b = false;
		tr_b = (*nl_i)->adj.begin(); tr_e = (*nl_i)->adj.end();
		for (tr_i = tr_b; tr_i != tr_e; tr_i++) {
			trans_counter++;
			if (b) {
				cout << ", ";
			} else {
				b = true;
			}
			cout << "(<";
			b1 = false;
			tl_b = tr_i->t_label.begin();
			tl_e = tr_i->t_label.end();
			for (tl_i = tl_b; tl_i != tl_e; tl_i++) {
				if (b1) {
					cout << ", ";
				} else {
					b1 = true;
				}
				cout << *tl_i;
			}
			cout << "> - " << tr_i->target->name << ")";
		}
		cout << "}" << endl;
	}

	cout << "Accept: {";
	b = false;
	nl_b = ba_accept_nodes.begin(); nl_e = ba_accept_nodes.end();
	for (nl_i = nl_b; nl_i != nl_e; nl_i++) {
		if (b) {
			cout << ", ";
		} else {
			b = true;
		}
		cout << (*nl_i)->name;
	}
	cout << "}" << endl;

	cout << "nodes: " << ba_node_list.size() << "; transitions: " <<
	trans_counter << "; initial: " << ba_init_nodes.size() <<
	"; accept: " << ba_accept_nodes.size() << endl;
}

list<ALT_ba_node_t*> ALT_graph_t::get_all_BA_nodes() const
{
	return(ba_node_list);
}

list<ALT_ba_node_t*> ALT_graph_t::get_init_BA_nodes() const
{
	return(ba_init_nodes);
}

list<ALT_ba_node_t*> ALT_graph_t::get_accept_BA_nodes() const
{
	return(ba_accept_nodes);
}

