/*
*  Optimization of Buchi automata
*    - implementation of methods
*
*  Milan Jaros - xjaros1@fi.muni.cz
*
*/

#include <iterator>
#include <set>
#include <algorithm>
#include <queue>

#include "opt_buchi.hh"

//#include "error/error.hh"
//#include "deb/deb.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
using namespace std;
#endif //DOXYGEN_PROCESSING

typedef map<int, KS_BA_node_t*> node_list_t;


BA_opt_graph_t::BA_opt_graph_t()
{ }

BA_opt_graph_t::BA_opt_graph_t(const BA_graph_t& G):BA_graph_t(G)
{ }

BA_opt_graph_t::~BA_opt_graph_t()
{ }

BA_opt_graph_t& BA_opt_graph_t::operator=(const BA_graph_t& G)
{
	BA_graph_t::operator=(G);

	return(*this);
}


void BA_opt_graph_t::to_one_initial()
{
	node_list_t::iterator n_b, n_e, n_i;
	list<BA_node_t*> init_nodes;
	list<BA_node_t*>::iterator in_b, in_e, in_i;
	list<BA_trans_t>::iterator tr_b, tr_e, tr_i, tr_j, tr_k;
	int max_name = 0;
	bool b;
	BA_node_t *p_N;

	n_b = node_list.begin(); n_e = node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if (n_i->first > max_name) max_name = n_i->first;
		if (n_i->second->initial) {
			p_N = dynamic_cast<BA_node_t*>(n_i->second);
			init_nodes.push_back(p_N);
		}
	}

	if (init_nodes.size() == 0) {
		clear(); return;
	}
	if (init_nodes.size() == 1) return;

	p_N = new BA_node_t(max_name + 1);
	node_list[max_name + 1] = p_N;
	p_N->initial = true;

	in_b = init_nodes.begin(); in_e = init_nodes.end();
	for (in_i = in_b; in_i != in_e; in_i++) {
		(*in_i)->initial = false;
		tr_b = (*in_i)->adj.begin(); tr_e = (*in_i)->adj.end();
		for (tr_i = tr_b; tr_i != tr_e; tr_i++) {
			b = true;
			tr_j = p_N->adj.begin();
			while (tr_j != p_N->adj.end()) {
				if (tr_j->target == tr_i->target) {
					if(LTL_subset_label(tr_j->t_label,
					tr_i->t_label)) {
						b = false; break;
					}
					if(LTL_subset_label(tr_i->t_label,
					tr_j->t_label)) {
						tr_k = tr_j; tr_j++;
						p_N->adj.erase(tr_k);
						continue;
					}
				}
				tr_j++;
			}
			if (b) p_N->adj.push_back(*tr_i);
		}
	}

	remove_unreachable_nodes();
}

void BA_opt_graph_t::remove_unreachable_nodes()
{
	node_list_t::iterator n_b, n_e, n_i;
	list<KS_BA_node_t*> unreach;
	list<KS_BA_node_t*>::iterator u_b, u_e, u_i;

	n_b = node_list.begin(); n_e = node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		n_i->second->col = WHITE;
	}

	for (n_i = n_b; n_i != n_e; n_i++) {
		if (n_i->second->initial && (n_i->second->col == WHITE)) {
			DFS_visit(n_i->second);
		}
	}

	for (n_i = n_b; n_i != n_e; n_i++) {
		if (n_i->second->col == WHITE)
			unreach.push_back(n_i->second);
	}

	u_b = unreach.begin(); u_e = unreach.end();
	for (u_i = u_b; u_i != u_e; u_i++) {
		node_list.erase((*u_i)->name);
		delete *u_i;
	}
}

void BA_opt_graph_t::remove_bad_accept()
{
	node_list_t::iterator n_b, n_e, n_i, n_j;

	n_b = node_list.begin(); n_e = node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if (n_i->second->accept) {
			for (n_j = n_b; n_j != n_e; n_j++) {
				n_j->second->col = WHITE;
			}
			n_i->second->accept = is_reachable(n_i->second,
				n_i->second);
		}
	}
}

void BA_opt_graph_t::set_redundand_accept()
{
	node_list_t::iterator n_b, n_e, n_i, n_j;

	n_b = node_list.begin(); n_e = node_list.end();

	for (n_i = n_b; n_i != n_e; n_i++) {
		if (!n_i->second->accept) {
			for (n_j = n_b; n_j != n_e; n_j++) {
				n_j->second->col = WHITE;
			}
			n_i->second->accept = !(is_reachable(n_i->second,
				n_i->second));
		}
	}
}

bool BA_opt_graph_t::reach_accept(KS_BA_node_t* p_N, list<KS_BA_node_t*>& L)
{
	list<KS_BA_node_t*> A;
	list<KS_BA_node_t*>::iterator a_b, a_e, a_i;

	p_N->col = GRAY;
	L.push_back(p_N);
	A = p_N->get_adj();
	a_b = A.begin(); a_e = A.end();

	for (a_i = a_b; a_i != a_e; a_i++) {
		if ((*a_i)->accept || ((*a_i)->pom == 1)) return(true);
		if ( ((*a_i)->col == WHITE) && ((*a_i)->pom == 0) ){
			if (reach_accept(*a_i, L)) return(true);
		}
	}

	p_N->col = BLACK;
	L.pop_back();
	return(false);
}

void BA_opt_graph_t::remove_never_accept()
{
	node_list_t::iterator n_b, n_e, n_i, n_j;
	list<KS_BA_node_t*> L, Nev;
	list<KS_BA_node_t*>::iterator l_b, l_e, l_i;

	remove_bad_accept();

	n_b = node_list.begin(); n_e = node_list.end();

	for (n_i = n_b; n_i != n_e; n_i++) {
		n_i->second->pom = 0;
	}

	for (n_i = n_b; n_i != n_e; n_i++) {
		if (!n_i->second->accept && (n_i->second->pom == 0)) {
			L.erase(L.begin(), L.end());
			for (n_j = n_b; n_j != n_e; n_j++) {
				n_j->second->col = WHITE;
			}
			if (reach_accept(n_i->second, L)) {
				l_b = L.begin(); l_e = L.end();
				for (l_i = l_b; l_i != l_e; l_i++) {
					(*l_i)->pom = 1;
				}
			} else {
				n_i->second->pom = 2;
				Nev.push_back(n_i->second);
			}
		}
	}

	l_b = Nev.begin(); l_e = Nev.end();
	for (l_i = l_b; l_i != l_e; l_i++) {
		del_node(*l_i);
	}
}

bool BA_opt_graph_t::is_ball(const list<int> & SCC_tree)
{
	list<int> ::const_iterator n_b, n_e, n_i, n_f;
	list<BA_trans_t>::const_iterator t_b, t_e, t_i;
	const BA_node_t *p_N;
	LTL_label_t vzor;
	bool is_accept = false;

	if (SCC_tree.size() <= 1) { // singleton
		return(false);
	}

	n_b = SCC_tree.begin(); n_e = SCC_tree.end();

	/* ex. acc. stav, zadna hrana nevede ven, hrany maji stejny label
	*/
	p_N = dynamic_cast<const BA_node_t*>(node_list[*n_b]);
	vzor = p_N->adj.begin()->t_label;
	for (n_i = n_b; n_i != n_e; n_i++) {
		p_N = dynamic_cast<const BA_node_t*>(node_list[*n_i]);
		if (p_N->accept) is_accept = true;
		t_b = p_N->adj.begin(); t_e = p_N->adj.end();
		for (t_i = t_b; t_i != t_e; t_i++) {
			if (!LTL_equiv_labels(vzor, t_i->t_label)) {
				/* nestejny label hran */
				return(false);
			}
			n_f = find(n_b, n_e, t_i->target->name);
			if (n_f == n_e) { // hrana vede ven
				return(false);
			}
		}
	}

	return(is_accept);
}

void BA_opt_graph_t::fix_form_ball_reduction(BA_opt_graph_t& G)
{
	node_list_t::iterator n_b, n_e, n_i;
	list<list<int> >  SCC_list, ball_list;
	list<list<int> > ::iterator sc_b, sc_e, sc_i, bl_b, bl_e, bl_i;
	list<int> ::iterator c_b, c_e, c_i;
	list<BA_trans_t>::iterator t_b, t_e, t_i;

	set<int> non_ball_nodes;
	set<int>::iterator nb_b, nb_e, nb_i;

	KS_BA_node_t *p_N, *p_N_new, *p_N_new1;
	BA_node_t *p_bN, *p_bN1;

	G.clear();

	SCC(SCC_list);

	timer = 1;

	sc_b = SCC_list.begin(); sc_e = SCC_list.end();
	for (sc_i = sc_b; sc_i != sc_e; sc_i++) {
		if (is_ball(*sc_i)) {
			ball_list.push_back(*sc_i);
		} else {
			c_b = sc_i->begin(); c_e = sc_i->end();
			for (c_i = c_b; c_i != c_e; c_i++) {
				non_ball_nodes.insert(*c_i);
			}
		}
	}

	if (ball_list.size() == 0) { /* Graf neobsahuje ball */
		G = *this; return;
	}

	/* Nastaveni promene BA_node_t::pom = 0 u vsech uzlu */
	/* BA_node_t::pom znamena nove jmeno uzlu */
	n_b = node_list.begin(); n_e = node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		n_i->second->pom = 0;
	}

	/* Redukce 'ball'u na jeden uzel */
	bl_b = ball_list.begin(); bl_e = ball_list.end();
	for (bl_i = bl_b; bl_i != bl_e; bl_i++) {
		p_N_new = G.add_node(timer); timer++;
		c_b = bl_i->begin(); c_e = bl_i->end();
		for (c_i = c_b; c_i != c_e; c_i++) {
			p_N = node_list[*c_i];
			p_N->pom = p_N_new->name;
			if (p_N->initial) p_N_new->initial = true;
		}
		p_bN = dynamic_cast<BA_node_t*>(p_N);
		p_N_new->accept = true;
		G.add_trans(p_N_new, p_N_new, p_bN->adj.front().t_label);
	}

	/* Vybudovani noveho grafu */
	nb_b = non_ball_nodes.begin(); nb_e = non_ball_nodes.end();
	for (nb_i = nb_b; nb_i != nb_e; nb_i++) {   
		p_bN = dynamic_cast<BA_node_t*>(node_list[*nb_i]);
		if (p_bN->pom == 0) {
			p_N_new = G.add_node(timer);
			p_bN->pom = timer; timer++;
			p_N_new->initial = p_bN->initial;
			p_N_new->accept = p_bN->accept;
		} else {
			p_N_new = G.find_node(p_bN->pom);
		}
		t_b = p_bN->adj.begin(); t_e = p_bN->adj.end();
		for (t_i = t_b; t_i != t_e; t_i++) {
			p_bN1 = t_i->target;
			if (p_bN1->pom == 0) {
				p_N_new1 = G.add_node(timer);
				p_bN1->pom = timer; timer++;
				p_N_new1->initial = p_bN1->initial;
				p_N_new1->accept = p_bN1->accept;
			} else {
				p_N_new1 = G.find_node(p_bN1->pom);
			}
			G.add_trans(p_N_new, p_N_new1, t_i->t_label);
		}
	}
}

/* Bisimulacni redukce */

typedef pair<int, LTL_label_t > pair_col;
typedef list<pair_col> set_next_col;
typedef pair<int, set_next_col> pair_set_col;

void insert_edge(set_next_col& L, const pair_col& E);

bool equiv_set_col(const set_next_col& S1, const set_next_col& S2);

int insert_color(list<pair_set_col>& L, const pair_set_col& C);

void simplify_color(pair_set_col& C);

void add_opt_trans(KS_BA_node_t* p_N, KS_BA_node_t* cil,
        const LTL_label_t& t_label);


void BA_opt_graph_t::basic_bisim_reduction(BA_opt_graph_t& G)
{
	list<KS_BA_node_t*> init_nodes = get_init_nodes();

	node_list_t::iterator n_b, n_e, n_i;
	list<BA_trans_t>::iterator t_b, t_e, t_i;

	list<pair_set_col> COL; // seznam barev
	list<pair_set_col>::iterator col_b, col_e;
	pair_set_col P_S_COL; // barva
	pair_col P_COL; // hrana
	map<int, list<KS_BA_node_t*> > color_nodes; // <barva, seznam uzlu>
        map<int, list<KS_BA_node_t*> >::iterator cn_b, cn_e, cn_i;

	list<KS_BA_node_t*>::iterator ln_b, ln_e, ln_i;

	int i;
	int max_old, max_new; // maximalni barva

	KS_BA_node_t *p_N, *p_N1;


	G.clear();

	max_old = 1; max_new = 1;
	n_b = node_list.begin(); n_e = node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if (n_i->second->accept) {
			n_i->second->pom = 2;
			color_nodes[1].push_back(n_i->second);
			max_new = 2;
		} else {
			n_i->second->pom = 1;
			color_nodes[2].push_back(n_i->second);
		}
	}

	while (max_new != max_old) {
		max_old = max_new;
		color_nodes.erase(color_nodes.begin(), color_nodes.end());
		COL.erase(COL.begin(), COL.end());
		for (n_i = n_b; n_i != n_e; n_i++) {
			P_S_COL.second.erase(P_S_COL.second.begin(),
				P_S_COL.second.end());
			P_S_COL.first = n_i->second->pom;
			t_b = get_node_adj(n_i->second).begin();
			t_e = get_node_adj(n_i->second).end();
			for (t_i = t_b; t_i != t_e; t_i++) {
				P_COL.first = t_i->target->pom;
				P_COL.second = t_i->t_label;
				insert_edge(P_S_COL.second, P_COL);
			}
			i = insert_color(COL, P_S_COL);
			color_nodes[i].push_back(n_i->second);
			if (i > max_new) max_new = i;
		}

		/* nove obarveni uzlu */
		cn_b = color_nodes.begin(); cn_e = color_nodes.end();
		for (cn_i = cn_b, i = 1; cn_i != cn_e; cn_i++, i++) {
			ln_b = cn_i->second.begin();
			ln_e = cn_i->second.end();
			for (ln_i = ln_b; ln_i != ln_e; ln_i++) {
				(*ln_i)->pom = i;
			}
		}
	}

	/* Vybudovani noveho grafu */

	col_b = COL.begin(); col_e = COL.end(); i = 1;

	cn_b = color_nodes.begin(); cn_e = color_nodes.end();
	for (cn_i = cn_b; cn_i != cn_e; cn_i++) {
		ln_b = cn_i->second.begin(); ln_e = cn_i->second.end();

		p_N1 = cn_i->second.front();
		p_N = G.add_node(cn_i->first);
		p_N->accept = p_N1->accept;

		for (ln_i = ln_b; ln_i != ln_e; ln_i++) {
			if ((*ln_i)->initial) p_N->initial = true;
			t_b = get_node_adj(*ln_i).begin();
			t_e = get_node_adj(*ln_i).end();
			for (t_i = t_b; t_i != t_e; t_i++) {
				p_N1 = G.add_node(t_i->target->pom);
				add_opt_trans(p_N, p_N1, t_i->t_label);
			}
		}
	}
}

void insert_edge(set_next_col& L, const pair_col& E)
{
	set_next_col::iterator l_b, l_e, l_i;

	l_b = L.begin(); l_e = L.end();
	for (l_i = l_b; l_i != l_e; l_i++) {
		if (l_i->first == E.first)
			if (LTL_equiv_labels(l_i->second, E.second))
				break;
	}

	if (l_i == l_e) {
		L.push_back(E);
	}
}
         
bool equiv_set_col(const set_next_col& S1, const set_next_col& S2)
{
	set_next_col::const_iterator s1_b, s1_e, s1_i, s2_b, s2_e, s2_i;

	if (S1.size() != S2.size()) return(false);

	s1_b = S1.begin(); s1_e = S1.end();
	s2_b = S2.begin(); s2_e = S2.end();

	for (s1_i = s1_b; s1_i != s1_e; s1_i++) {
		for (s2_i = s2_b; s2_i != s2_e; s2_i++) {
			if (s1_i->first == s2_i->first)
				if (LTL_equiv_labels(s1_i->second,
					s2_i->second))
					break;
		}
		if (s2_i == s2_e) return(false);
	}

	/* Z druhe strany nemusime porovnavat -
		jsou to mnoziny (de facto) */

	return(true);
}
 
int insert_color(list<pair_set_col>& L, const pair_set_col& C)
{
	list<pair_set_col>::iterator l_b, l_e, l_i;
	int i = 1;

	if (L.empty()) {
		L.push_back(C);
		return(1);
	}

	l_b = L.begin(); l_e = L.end();
	for (l_i = l_b; l_i != l_e; l_i++, i++) {
		if (l_i->first == C.first)
			if (equiv_set_col(l_i->second, C.second))
				break;
	}

	if (l_i == l_e) {
		L.push_back(C);
	}

	return(i);
}

void simplify_color(pair_set_col& C)
{
	set_next_col::iterator s_i, s_j, s_k;

	for (s_i = C.second.begin(); s_i != C.second.end(); s_i++) {
		s_j = C.second.begin();
		while (s_j != C.second.end()) {
			s_k = s_j; s_k++;
			if ((s_i != s_j) && (s_i->first == s_j->first)) {
				if (LTL_subset_label(s_i->second,
					s_j->second))
					C.second.erase(s_j);
			}
			s_j = s_k;
		}
	}
}

void add_opt_trans(KS_BA_node_t* p_N, KS_BA_node_t* cil,
        const LTL_label_t& t_label)
{
	list<BA_trans_t>::iterator t_i, t_j;
	BA_node_t *p_bN = dynamic_cast<BA_node_t*>(p_N);
	BA_node_t *p_b_cil = dynamic_cast<BA_node_t*>(cil);
	BA_trans_t T;

	/* novy prvek neni vetsi */
	for (t_i = p_bN->adj.begin(); t_i != p_bN->adj.end(); t_i++) {
		if (t_i->target->name == cil->name) {
			if (LTL_subset_label(t_i->t_label, t_label)) {
				return;
			}
		}
	}

	/* odstranime vetsi prvky */
	t_i = p_bN->adj.begin();
	while (t_i != p_bN->adj.end()) {
		t_j = t_i; t_j++;
		if (t_i->target->name == cil->name) {
			if (LTL_subset_label(t_label, t_i->t_label)) {
				p_bN->adj.erase(t_i);
			}
		}
		t_i = t_j;
	}

	/* pridame novou hranu */
	T.t_label = t_label; T.target = p_b_cil;
	p_bN->adj.push_back(T);
}

/* Simulacni redukce */

bool i_dominates(const pair_col& P1, const pair_col& P2, bool **par_ord);

void remove_non_imaximal(set_next_col& L, bool **par_ord);

bool i_dominates(const set_next_col& L1, const set_next_col& L2,
        bool **par_ord);

void add_opt_trans(KS_BA_node_t* p_N, KS_BA_node_t* cil,
        const LTL_label_t& t_label, bool **par_ord);

void show_par_ord(bool **par_ord, int par_ord_size);


void BA_opt_graph_t::strong_fair_sim_reduction(BA_opt_graph_t& G)
{
	node_list_t::iterator n_b, n_e, n_i;
	list<BA_trans_t>::iterator t_b, t_e, t_i;

	list<pair_set_col> COL; // seznam barev
	list<pair_set_col>::iterator col_b, col_e, col_i, col_j;
	pair_set_col P_S_COL; // barva
	pair_col P_COL; // hrana

	map<int, list<KS_BA_node_t*> > color_nodes; // <barva, seznam uzlu
	map<int, list<KS_BA_node_t*> >::iterator cn_b, cn_e, cn_i;
	list<KS_BA_node_t*>::iterator ln_b, ln_e, ln_i;

	int i, i1, i2;
	int size_old, size_new;
	int max_old, max_new;

	KS_BA_node_t *p_N, *p_N1;

	bool **par_ord, **par_ord_new; // castecne usporadani na barvach
	int par_ord_size, par_ord_size_new;


	G.clear();

	max_old = 1; max_new = 1;
	n_b = node_list.begin(); n_e = node_list.end();
	for (n_i = n_b; n_i != n_e; n_i++) {
		if (n_i->second->accept) {
			n_i->second->pom = 2;
			color_nodes[1].push_back(n_i->second);
			max_new = 2;
		} else {
			n_i->second->pom = 1;
			color_nodes[2].push_back(n_i->second);
		}
	}
	size_old = 1; size_new = color_nodes.size();

	/* inicializace castecneho usporadani */
	par_ord = new bool*[2];
	par_ord[0] = new bool [2]; par_ord[1] = new bool [2];

	par_ord[0][0] = true; par_ord[0][1] = true;
	par_ord[1][0] = false; par_ord[1][1] = true;
	par_ord_size = 2;


	while ((max_old != max_new) || (size_old != size_new)) {
		max_old = max_new;
		color_nodes.erase(color_nodes.begin(), color_nodes.end());
		COL.erase(COL.begin(), COL.end());
		for (n_i = n_b; n_i != n_e; n_i++) {
			P_S_COL.second.erase(P_S_COL.second.begin(),
				P_S_COL.second.end());
			P_S_COL.first = n_i->second->pom;
			t_b = get_node_adj(n_i->second).begin();
			t_e = get_node_adj(n_i->second).end();
			for (t_i = t_b; t_i != t_e; t_i++) {
				P_COL.first = t_i->target->pom;
				P_COL.second = t_i->t_label;
				insert_edge(P_S_COL.second, P_COL);
			}
			remove_non_imaximal(P_S_COL.second, par_ord);

			i = insert_color(COL, P_S_COL);
			color_nodes[i].push_back(n_i->second);
			if (i > max_new) max_new = i;
		}

		/* nove obarveni uzlu */
		cn_b = color_nodes.begin(); cn_e = color_nodes.end();
		for (cn_i = cn_b, i = 1; cn_i != cn_e; cn_i++, i++) {
			ln_b = cn_i->second.begin();
			ln_e = cn_i->second.end();
			for (ln_i = ln_b; ln_i != ln_e; ln_i++) {
				(*ln_i)->pom = i;
			}
		}

		/* uprava castecneho usporadani */
		par_ord_size_new = COL.size();
		par_ord_new = new bool*[par_ord_size_new];
		for (i = 0; i < par_ord_size_new; i++) {
			par_ord_new[i] = new bool [par_ord_size_new];
		}
		col_b = COL.begin(); col_e = COL.end();
		for (col_i = col_b, i1 = 0; col_i != col_e;
			col_i++, i1++) {
			i = col_i->first - 1;
			for (col_j = col_b, i2 = 0; col_j != col_e;
				col_j++, i2++) {
				if (par_ord[col_j->first - 1][i] &&
					i_dominates(col_i->second,
					col_j->second, par_ord)) {
					par_ord_new[i2][i1] = true;
				} else {
					par_ord_new[i2][i1] = false;
				}
			}
		}

		for (i = 0; i < par_ord_size; i++) delete[] par_ord[i];
		delete[] par_ord;
		par_ord = par_ord_new;
		par_ord_size = par_ord_size_new;

		size_old = size_new;
		size_new = color_nodes.size();
	}

	/* Vybudovani noveho grafu */
	col_b = COL.begin(); col_e = COL.end(); i = 1;

	cn_b = color_nodes.begin(); cn_e = color_nodes.end();
	for (cn_i = cn_b; cn_i != cn_e; cn_i++) {
		ln_b = cn_i->second.begin(); ln_e = cn_i->second.end();

		p_N1 = cn_i->second.front();
		p_N = G.add_node(cn_i->first);
		p_N->accept = p_N1->accept;

		for (ln_i = ln_b; ln_i != ln_e; ln_i++) {
			if ((*ln_i)->initial) p_N->initial = true;
			t_b = get_node_adj(*ln_i).begin();
			t_e = get_node_adj(*ln_i).end();
			for (t_i = t_b; t_i != t_e; t_i++) {
				p_N1 = G.add_node(t_i->target->pom);
				add_opt_trans(p_N, p_N1, t_i->t_label,
					par_ord);
			}
		}
	}

	for (i = 0; i < par_ord_size; i++) {
		delete[] par_ord[i];
	}
	delete[] par_ord;

	G.remove_unreachable_nodes();
}


bool i_dominates(const pair_col& P1, const pair_col& P2, bool **par_ord)
{ /* P1 dominuje P2 */

	return(par_ord[P2.first - 1][P1.first - 1] &&
		LTL_subset_label(P1.second, P2.second));
}
        
void remove_non_imaximal(set_next_col& L, bool **par_ord)
{
	set_next_col::iterator l_i, l_j, l_k;

	for (l_i = L.begin(); l_i != L.end(); l_i++) {
		l_j = L.begin();
		while (l_j != L.end()) {
			l_k = l_j; l_k++;
			if ((l_i != l_j) &&
				i_dominates(*l_i, *l_j, par_ord)) {
				L.erase(l_j);
			}
			l_j = l_k;
		}
	}
}

bool i_dominates(const set_next_col& L1, const set_next_col& L2,
	bool **par_ord) /* L1 dominuje L2 */
{
	set_next_col::const_iterator l1_b, l1_e, l1_i, l2_b, l2_e, l2_i;

	l1_b = L1.begin(); l1_e = L1.end();
	l2_b = L2.begin(); l2_e = L2.end();

	for (l2_i = l2_b; l2_i != l2_e; l2_i++) {
		for (l1_i = l1_b; l1_i != l1_e; l1_i++) {
			if (i_dominates(*l1_i, *l2_i, par_ord)) break;
		}
		if (l1_i == l1_e) return(false);
	}

	return(true);
}
        
void add_opt_trans(KS_BA_node_t* p_N, KS_BA_node_t* cil,
	const LTL_label_t& t_label, bool **par_ord)
{
	list<BA_trans_t>::iterator t_i, t_j;
	BA_trans_t T;
	int c, c1;
	BA_node_t *p_bN = dynamic_cast<BA_node_t*>(p_N);
	BA_node_t *p_b_cil = dynamic_cast<BA_node_t*>(cil);

	c = cil->name - 1;
	/* novy prvek neni zbytecny a je i-maximalni */
	for (t_i = p_bN->adj.begin(); t_i != p_bN->adj.end(); t_i++) {
		c1 = t_i->target->name - 1;
		if ((t_i->target == cil) &&
			LTL_subset_label(t_i->t_label, t_label)) {
			return;
		}
		if (par_ord[c][c1] &&
			LTL_subset_label(t_i->t_label, t_label)) {
			return;
		}
	}

	/* odstranime zbytecne a mensi prvky */
	t_i = p_bN->adj.begin();
	while (t_i != p_bN->adj.end()) {
		t_j = t_i; t_j++;
		c1 = t_i->target->name - 1;
		if ((t_i->target == cil) &&
			LTL_subset_label(t_label, t_i->t_label)) {
			p_bN->adj.erase(t_i);
		} else if (par_ord[c1][c] &&
			LTL_subset_label(t_label, t_i->t_label)) {
			p_bN->adj.erase(t_i);
		}
		t_i = t_j;
	}

	/* pridame novou hranu */
	T.t_label = t_label; T.target = p_b_cil;
	p_bN->adj.push_back(T);
}
        
void show_par_ord(bool **par_ord, int par_ord_size)
{
	int i, j;

	for (i = 0; i < par_ord_size; i++) {
		for (j = 0; j < par_ord_size; j++) {
			if (par_ord[i][j])
				cerr << "Y";
			else
				cerr << "N";
			cerr << "\t";
		}
		cerr << endl;
	}

	cerr << endl;
}

void BA_opt_graph_t::optimize(BA_opt_graph_t& G_opt, int stupen)
{
	BA_opt_graph_t G1, G2;
	int e_size1, n_size1, e_size2, n_size2;

	switch (stupen) {
	case 1:
		fix_form_ball_reduction(G1);
		G1.basic_bisim_reduction(G2);
		G2.remove_bad_accept();
		G2.basic_bisim_reduction(G_opt);
		break;
	case 2:
		fix_form_ball_reduction(G1);
		G1.strong_fair_sim_reduction(G2);
		G2.remove_bad_accept();
		G2.strong_fair_sim_reduction(G_opt);
		break;
	case 3:
		fix_form_ball_reduction(G1);
		G1.basic_bisim_reduction(G2);
		G2.remove_bad_accept();
		G2.basic_bisim_reduction(G1);
		G1.set_redundand_accept();
		G1.basic_bisim_reduction(G_opt);
		G_opt.remove_bad_accept();
		break;
	case 4:
		fix_form_ball_reduction(G1);
		G1.strong_fair_sim_reduction(G2);
		G2.remove_bad_accept();
		G2.strong_fair_sim_reduction(G1);
		G1.set_redundand_accept();
		G1.strong_fair_sim_reduction(G_opt);
		G_opt.remove_bad_accept();
		break;

	case 5:
		fix_form_ball_reduction(G1);
		G1.remove_never_accept();

		do {
			n_size1 = G1.n_size(); e_size1 = G1.e_size();

			G1.set_redundand_accept();
			G1.basic_bisim_reduction(G2);
			G2.remove_bad_accept();
			G2.basic_bisim_reduction(G1);

			n_size2 = G1.n_size(); e_size2 = G1.e_size();
			//cerr << ".";
		} while ((n_size2 < n_size1) || (e_size1 < e_size2));
		//cerr << endl;

		G_opt = G1;
		break;
	case 6:
		fix_form_ball_reduction(G1);
		G1.remove_never_accept();

		do {
			n_size1 = G1.n_size(); e_size1 = G1.e_size();

			G1.set_redundand_accept();
			G1.strong_fair_sim_reduction(G2);
			G2.remove_bad_accept();
			G2.strong_fair_sim_reduction(G1);

			n_size2 = G1.n_size(); e_size2 = G1.e_size();
			//cerr << ".";
		} while ((n_size2 < n_size1) || (e_size1 < e_size2));
		//cerr << endl;

		G_opt = G1;
		break;

	case 10:
		G_opt = *this;
		G_opt.remove_never_accept();
		break;
	default:
		G_opt = *this;
		G_opt.remove_bad_accept(); // ?
	}
}
