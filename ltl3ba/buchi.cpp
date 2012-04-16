/***** ltl3ba : buchi.c *****/

/* Written by Denis Oddoux, LIAFA, France                                 */
/* Copyright (c) 2001  Denis Oddoux                                       */
/* Modified by Paul Gastin, LSV, France                                   */
/* Copyright (c) 2007  Paul Gastin                                        */
/* Modified by Tomas Babiak, FI MU, Brno, Czech Republic                  */
/* Copyright (c) 2012  Tomas Babiak                                       */
/*                                                                        */
/* This program is free software; you can redistribute it and/or modify   */
/* it under the terms of the GNU General Public License as published by   */
/* the Free Software Foundation; either version 2 of the License, or      */
/* (at your option) any later version.                                    */
/*                                                                        */
/* This program is distributed in the hope that it will be useful,        */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of         */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          */
/* GNU General Public License for more details.                           */
/*                                                                        */
/* You should have received a copy of the GNU General Public License      */
/* along with this program; if not, write to the Free Software            */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA*/
/*                                                                        */
/* Based on the translation algorithm by Gastin and Oddoux,               */
/* presented at the 13th International Conference on Computer Aided       */
/* Verification, CAV 2001, Paris, France.                                 */
/* Proceedings - LNCS 2102, pp. 53-65                                     */
/*                                                                        */
/* Modifications based on paper by                                        */
/* T. Babiak, M. Kretinsky, V. Rehak, and J. Strejcek,                    */
/* LTL to Buchi Automata Translation: Fast and More Deterministic         */
/* presented at the 18th International Conference on Tools and            */
/* Algorithms for the Construction and Analysis of Systems (TACAS 2012)   */
/*                                                                        */

#include "ltl3ba.h"
#include <bdd.h>
#include <map>
#include <list>
#include <set>

/* When defined, auxiliary dictionaries are used to speed up searching */
/* among existing states.             Comment to disable.              */
#define DICT

using namespace std;

/********************************************************************\
|*              Structures and shared variables                     *|
\********************************************************************/

extern GState **init, *gstates;
#ifdef STATS
extern struct rusage tr_debut, tr_fin;
extern struct timeval t_diff;
#endif
extern int tl_verbose, tl_stats, tl_simp_diff, tl_simp_fly, tl_simp_scc, tl_ltl3ba,
  tl_bisim, tl_bisim_r, tl_sim, tl_sim_r, init_size, *final;
extern void put_uform(void);

extern int sym_size, mod, predicates, scc_id, gstate_id;
extern char **sym_table;

extern FILE *tl_out;
BState *bstack, *bstates, *bremoved;
BScc *bscc_stack;
#ifdef DICT
map<GState*, BState*>** bsDict;
#endif
int accept, bstate_count = 0, btrans_count = 0, b_rank;

void reinitBuchi() {
    bstack = bstates = bremoved = NULL;
    bscc_stack = NULL;
#ifdef DICT
    bsDict = NULL;
#endif
    accept = bstate_count = btrans_count = b_rank = 0;
}

/********************************************************************\
|*              Simplification of the Buchi automaton               *|
\********************************************************************/

void decrement_incoming(map<BState*, bdd> *trans) {
  map<BState*, bdd>::iterator t;
  for(t = trans->begin(); t != trans->end(); t++)
      t->first->incoming--;
}

void free_bstate(BState *s) /* frees a state and its transitions */
{
  // free_btrans
  decrement_incoming(s->trans);
  delete s->trans;
  tfree(s);
}

BState *remove_bstate(BState *s, BState *s1) /* removes a state */
{
  BState *prv = s->prv;
  s->prv->nxt = s->nxt;
  s->nxt->prv = s->prv;
  if (s->trans)
    delete s->trans;
  s->trans = (map<BState*, bdd> *)0;
  s->nxt = bremoved->nxt;
  bremoved->nxt = s;
  s->prv = s1;
  for(s1 = bremoved->nxt; s1 != bremoved; s1 = s1->nxt)
    if(s1->prv == s)
      s1->prv = s->prv;
  return prv;
} 

void retarget_all_btrans()
{             /* redirects transitions before removing a state from the automaton */
  BState *s;
  map<BState*, bdd>::iterator t, tx;
  for (s = bstates->nxt; s != bstates; s = s->nxt)
    for (t = s->trans->begin(); t != s->trans->end(); )
      if (!t->first->trans) { /* t->to has been removed */
        if(t->first->prv) { /* t->to->prv have some transitions - retarget there*/
          bdd *l = &((*s->trans)[t->first->prv]);
          if (*l == bdd_false()) {
            *l = t->second;
          } else {
            *l |= t->second;
          }
        }
        tx = t++;
        s->trans->erase(tx);
        if (s->trans->empty()) {
          break;
        }
      } else {
        t++;
      }
  while(bremoved->nxt != bremoved) { /* clean the 'removed' list */
    s = bremoved->nxt;
    bremoved->nxt = bremoved->nxt->nxt;
    tfree(s);
  }
}

int all_btrans_match(BState *a, BState *b) /* decides if the states are equivalent */
{
  map<BState*, bdd>::iterator s, t;
  bdd loop_a, loop_b;
  if (((a->final == accept) || (b->final == accept)) &&
      (a->final + b->final != 2 * accept) && 
      a->incoming >=0 && b->incoming >=0)
    return 0; /* the states have to be both final or both non final */

  // Chceck whether they have same number of transitions
  if (a->trans->size() != b->trans->size())
    return 0;
  
  // Check wheter all transitions match (note, they are ordered according to target node)
  for (s = a->trans->begin(), t = b->trans->begin(); s != a->trans->end() && t != b->trans->end(); ) { 
    /* Transitions differ */
    if (s->first != t->first || s->second != t->second) {
      /* If none of them is selfloop - return 0 */
      if (!tl_ltl3ba || (s->first != a && t->first != b)) {
        return 0;
      } else { /* One of them is selfloop */
        /* Skip the loops and save labels of loop transitions*/
        if (s->first == a && loop_a == bdd_false()) {
          loop_a = s->second;
          s++;
        }
        if (t->first == b && loop_b == bdd_false()) {
          loop_b = t->second;
          t++;
        }
      }
    } else {
      s++, t++;
    }
  }
  /* Last transition of state a is loop - save it */
  if (s != a->trans->end() && s->first == a && loop_a == bdd_false()) {
    loop_a = s->second;
    s++;
  }
  /* Last transition of state b is loop - save it */
  if (t != b->trans->end() && t->first == b && loop_b == bdd_false()) {
    loop_b = t->second;
    t++;
  }
  /* All transitions match or both states have exactly one selfloop */
  if (s == a->trans->end() && t == b->trans->end() && loop_a == loop_b)
    return 1;
  else
    return 0;
}

int simplify_bstates() /* eliminates redundant states */
{
  BState *s, *s1, *s2;
  int changed = 0;

#ifdef STATS
  if(tl_stats) getrusage(RUSAGE_SELF, &tr_debut);
#endif

  for (s = bstates->nxt; s != bstates; s = s->nxt) {
    if(s->trans->empty()) { /* s has no transitions */
      s = remove_bstate(s, (BState *)0);
      changed++;
      continue;
    }
    bstates->trans = s->trans;
    bstates->final = s->final;
    s1 = s->nxt;
    while(!all_btrans_match(s, s1))
      s1 = s1->nxt;
    if(s1 != bstates) { /* s and s1 are equivalent */
      if(s1->incoming == -1) {
        s1->final = s->final; /* get the good final condition */
        s1->incoming = s->incoming;
      }
      s = remove_bstate(s, s1);
      changed++;
    }
  }
  retarget_all_btrans();

  for (s = bstates->nxt; s != bstates; s = s->nxt) {
    for (s2 = s->nxt; s2 != bstates; s2 = s2->nxt) {
      if(s->final == s2->final && s->id == s2->id) {
        s->id = ++gstate_id;
      }
    }
  }

#ifdef STATS
  if(tl_stats) {
    getrusage(RUSAGE_SELF, &tr_fin);
    timeval_subtract (&t_diff, &tr_fin.ru_utime, &tr_debut.ru_utime);
    fprintf(tl_out, "\nSimplification of the Buchi automaton - states: %li.%06lis",
                t_diff.tv_sec, t_diff.tv_usec);
    fprintf(tl_out, "\n%i states removed\n", changed);
  }
#endif

  return changed;
}

int bdfs(BState *s) {
  map<BState*, bdd>::iterator t;
  BScc *c;
  BScc *scc = (BScc *)tl_emalloc(sizeof(BScc));
  scc->bstate = s;
  scc->rank = b_rank;
  scc->theta = b_rank++;
  scc->nxt = bscc_stack;
  bscc_stack = scc;

  s->incoming = 1;

  for (t = s->trans->begin(); t != s->trans->end(); t++) {
    if (t->first->incoming == 0) {
      int result = bdfs(t->first);
      scc->theta = min(scc->theta, result);
    }
    else {
      for(c = bscc_stack->nxt; c != 0; c = c->nxt)
        if(c->bstate == t->first) {
          scc->theta = min(scc->theta, c->rank);
          break;
        }
    }
  }
  if(scc->rank == scc->theta) {
    if(bscc_stack == scc) { /* s is alone in a scc */
      s->incoming = -1;
      for (t = s->trans->begin(); t != s->trans->end(); t++)
        if (t->first == s)
          s->incoming = 1;
    }
    bscc_stack = scc->nxt;
  }
  return scc->theta;
}

void simplify_bscc() {
  BState *s;
  b_rank = 1;
  bscc_stack = 0;

  if(bstates == bstates->nxt) return;

  for(s = bstates->nxt; s != bstates; s = s->nxt)
    s->incoming = 0; /* state color = white */

  bdfs(bstates->prv);

  for(s = bstates->nxt; s != bstates; s = s->nxt)
    if(s->incoming == 0)
      s = remove_bstate(s, 0);
}

typedef pair<int, bdd> pair_col;
typedef list<pair_col> set_next_col;
typedef pair<int, set_next_col> pair_set_col;

void insert_edge(set_next_col& L, const pair_col& E);
int insert_color(list<pair_set_col>& L, const pair_set_col& C);
bool equiv_set_col(const set_next_col& S1, const set_next_col& S2);
BState* add_state(int col, map<int, BState*>& color_dict);
void delete_bstack(BState* stack);

void basic_bisim_reduction() {
  BState *s, *stack, *to;
  pair_col P_COL; // transition
  pair_set_col P_S_COL; // collor
  list<pair_set_col> COL; // list of collors
  map<int, list<BState*> > color_nodes; // <color, list of nodes>
  map<int, list<BState*> >::iterator cn_i;
  list<BState*>::iterator ln_i;
  map<int, BState*> color_dict;
  map<BState*, bdd>::iterator t;
  int max_old = 0, max_new = 0;
  int i;
  
  if(bstates == bstates->nxt) return;
  
  // initialization
  for(s = bstates->nxt; s != bstates; s = s->nxt) {
    if (s->final == accept) {
      s->incoming = 1;
//      color_nodes[1].push_back(s);
      max_new = 1;
    } else {
      s->incoming = 2;
//      color_nodes[2].push_back(s);
    }
  }
  
  while (max_new != max_old) {
    max_old = max_new;
    color_nodes.clear();
    COL.clear();
    for(s = bstates->nxt; s != bstates; s = s->nxt) {
      P_S_COL.second.clear();
      P_S_COL.first = s->incoming;
      for(t = s->trans->begin(); t != s->trans->end(); t++) {
        P_COL.first = t->first->incoming;
        P_COL.second = t->second;
        insert_edge(P_S_COL.second, P_COL);
      }
      i = insert_color(COL, P_S_COL);
      color_nodes[i].push_back(s);
      if (i > max_new) max_new = i;      
    }
    
    /* Save new collors to nodes */

    for (cn_i = color_nodes.begin(), i = 1; cn_i != color_nodes.end(); cn_i++, i++) {
      for (ln_i = cn_i->second.begin(); ln_i != cn_i->second.end(); ln_i++) {
        (*ln_i)->incoming = i;
      }
    }
  }
  
  // Build a new Buchi automaton
  bstate_count = 0;
  btrans_count = 0;
  stack = (BState *) tl_emalloc(sizeof(BState)); /* sentinel */
  stack->nxt = stack;
  stack->prv = stack;
  for (cn_i = color_nodes.begin(); cn_i != color_nodes.end(); cn_i++) {
    s = add_state(cn_i->first, color_dict);
    s->nxt = stack->nxt;
    s->prv = stack;
    s->nxt->prv = s;  
    stack->nxt = s;
    // set acceptance according the first node in the list
    to = cn_i->second.front();
    s->final = to->final;

    for (ln_i = cn_i->second.begin(); ln_i != cn_i->second.end(); ln_i++) {
      if ((*ln_i)->id == -1) {
        s->id = -1;
        // s is initial node -> move it at the bottom of the stack
        s->prv->nxt = s->nxt;
        s->nxt->prv = s->prv;
        s->prv = stack->prv;
        s->nxt = stack;
        stack->prv->nxt = s;
        stack->prv = s;

      }
      if ((*ln_i)->id == 0) s->id = 0;
      for (t = (*ln_i)->trans->begin(); t != (*ln_i)->trans->end(); t++) {
        to = add_state(t->first->incoming, color_dict);
        if ((*s->trans)[to] == bdd_false()) {
          (*s->trans)[to] = t->second;
        } else {
          (*s->trans)[to] |= t->second;
        }
      }
    }
    btrans_count += s->trans->size();
  }

  delete_bstack(bstates);
  tfree(bstates);
  bstates = stack;
}

BState* add_state(int col, map<int, BState*>& color_dict) {
  map<int, BState*>::iterator it = color_dict.find(col);
  
  if (it !=  color_dict.end()) {
    return color_dict[col];
  }
  
  BState* s = (BState *) tl_emalloc(sizeof(BState)); /* creates a new state */
  s->gstate = (GState*) 0;
  s->id = col;
  s->incoming = col;
  s->final = -1;
  s->trans = new map<BState*, bdd>();
  color_dict[col] = s;
  bstate_count++;
  return s;
}

void delete_bstack(BState* stack) {
  BState *s;
  while (stack != stack->nxt) {
    s = stack->nxt;
    stack->nxt = stack->nxt->nxt;
    delete s->trans;
    tfree(s);
  }
}

void insert_edge(set_next_col& L, const pair_col& E) {
  set_next_col::iterator l_i;

  for (l_i = L.begin(); l_i != L.end(); l_i++) {
    if (l_i->first == E.first)
      if (l_i->second == E.second)
        break;
  }

  if (l_i == L.end()) {
    L.push_back(E);
  }
}

int insert_color(list<pair_set_col>& L, const pair_set_col& C)
{
  list<pair_set_col>::iterator l_i, l_e;
  int i = 1;

  if (L.empty()) {
    L.push_back(C);
    return 1;
  }

  l_e = L.end();

  for (l_i = L.begin(); l_i != l_e; l_i++, i++) {
    if (l_i->first == C.first)
    if (equiv_set_col(l_i->second, C.second))
      break;
  }

  if (l_i == l_e) {
  L.push_back(C);
  }

  return i;
}

bool equiv_set_col(const set_next_col& S1, const set_next_col& S2)
{
  set_next_col::const_iterator s1_i, s1_e, s2_i, s2_e;

  if (S1.size() != S2.size()) return false;

  s1_e = S1.end();
  s2_e = S2.end();

  for (s1_i = S1.begin(); s1_i != s1_e; s1_i++) {
    for (s2_i = S2.begin(); s2_i != s2_e; s2_i++) {
      if (s1_i->first == s2_i->first && s1_i->second ==	s2_i->second)
      break;
    }
    if (s2_i == s2_e) return false;
  }

  return true;
}

bool i_dominates(const pair_col& P1, const pair_col& P2, bool **par_ord);
bool i_dominates(const set_next_col& L1, const set_next_col& L2, bool **par_ord);
void remove_non_imaximal(set_next_col& L, bool **par_ord);
void add_opt_trans(BState *s, BState *to, bdd& label, bool **par_ord);

void strong_fair_sim_reduction() {
  BState *s, *stack, *to;
  pair_col P_COL; // transition
  pair_set_col P_S_COL; // collor
  list<pair_set_col> COL; // list of collors
  list<pair_set_col>::iterator col_i, col_j;
  map<int, list<BState*> > color_nodes; // <color, list of nodes>
  map<int, list<BState*> >::iterator cn_i;
  list<BState*>::iterator ln_i;
  map<BState*, bdd>::iterator t;
  map<int, BState*> color_dict;
  int max_old = 0, max_new = 0;
  int size_old = 1, size_new = 1;
  int i, i1, i2;
  
  bool **par_ord, **par_ord_new; // castecne usporadani na barvach
  int par_ord_size, par_ord_size_new;
  
  if(bstates == bstates->nxt) return;
  
  // initialization
  for(s = bstates->nxt; s != bstates; s = s->nxt) {
    if (s->final == accept) {
      s->incoming = 1;
      max_new = 1;
//      color_nodes[1].push_back(s);
    } else {
      s->incoming = 2;
//      color_nodes[2].push_back(s);
    }
  }

  // Inicialization of partial ordering
  par_ord = new bool* [2];
  par_ord[0] = new bool [2]; par_ord[1] = new bool [2];

  par_ord[0][0] = true; par_ord[0][1] = false;
  par_ord[1][0] = true; par_ord[1][1] = true;
  par_ord_size = 2;

  while (max_new != max_old || (size_old != size_new)) {
    max_old = max_new;
    color_nodes.clear();
    COL.clear();
    for(s = bstates->nxt; s != bstates; s = s->nxt) {
      P_S_COL.second.clear();
      P_S_COL.first = s->incoming;
      for(t = s->trans->begin(); t != s->trans->end(); t++) {
        P_COL.first = t->first->incoming;
        P_COL.second = t->second;
        insert_edge(P_S_COL.second, P_COL);
      }
      remove_non_imaximal(P_S_COL.second, par_ord); // upravit insert_edge aby nebolo treba odoberat nemaximalne
      i = insert_color(COL, P_S_COL);
      color_nodes[i].push_back(s);
      if (i > max_new) max_new = i;      
    }
    
    /* Save new collors to nodes */
    for (cn_i = color_nodes.begin(), i = 1; cn_i != color_nodes.end(); cn_i++, i++) {
      for (ln_i = cn_i->second.begin(); ln_i != cn_i->second.end(); ln_i++) {
        (*ln_i)->incoming = i;
      }
    }
    
    /* New partial ordering */
    size_old = size_new;
    size_new = 0;
    par_ord_size_new = COL.size();
    par_ord_new = new bool* [par_ord_size_new];
    for (i = 0; i < par_ord_size_new; i++) {
      par_ord_new[i] = new bool [par_ord_size_new];
    }
    for (col_i = COL.begin(), i1 = 0; col_i != COL.end(); col_i++, i1++) {
      i = col_i->first - 1;
      for (col_j = COL.begin(), i2 = 0; col_j != COL.end();	col_j++, i2++) {
        if (par_ord[col_j->first - 1][i] &&
            i_dominates(col_i->second, col_j->second, par_ord)) {
          par_ord_new[i2][i1] = true;
          size_new++;
        } else {
          par_ord_new[i2][i1] = false;
        }
      }
    }

    for (i = 0; i < par_ord_size; i++) delete[] par_ord[i];
    delete[] par_ord;
    par_ord = par_ord_new;
    par_ord_size = par_ord_size_new;
  }
  
  // Build a new Buchi automaton
  stack = (BState *) tl_emalloc(sizeof(BState)); /* sentinel */
  stack->nxt = stack;
  stack->prv = stack;
  bstate_count = 0;
  btrans_count = 0;
  for (cn_i = color_nodes.begin(); cn_i != color_nodes.end(); cn_i++) {
    s = add_state(cn_i->first, color_dict);
    s->nxt = stack->nxt;
    s->prv = stack;
    s->nxt->prv = s;  
    stack->nxt = s;
    // set acceptance according the first node in the list
    to = cn_i->second.front();
    s->final = to->final;

    for (ln_i = cn_i->second.begin(); ln_i != cn_i->second.end(); ln_i++) {
      if ((*ln_i)->id == -1) {
        s->id = -1;
        // s is initial node -> move it at the bottom of the stack
        s->prv->nxt = s->nxt;
        s->nxt->prv = s->prv;
        s->prv = stack->prv;
        s->nxt = stack;
        stack->prv->nxt = s;
        stack->prv = s;
      }
      if ((*ln_i)->id == 0) s->id = 0;
      for (t = (*ln_i)->trans->begin(); t != (*ln_i)->trans->end(); t++) {
        to = add_state(t->first->incoming, color_dict);
        add_opt_trans(s, to, t->second, par_ord);
      }
    }
    btrans_count += s->trans->size();
  }

  delete_bstack(bstates);
  tfree(bstates);
  bstates = stack;
}

bool i_dominates(const pair_col& P1, const pair_col& P2, bool **par_ord) { 
  return(par_ord[P2.first - 1][P1.first - 1] &&
    ((P1.second << P2.second) == bdd_true()));
}

bool i_dominates(const set_next_col& L1, const set_next_col& L2, bool **par_ord) {
  set_next_col::const_iterator l1_e, l1_i, l2_e, l2_i;

  l1_e = L1.end();
  l2_e = L2.end();

  for (l2_i = L2.begin(); l2_i != l2_e; l2_i++) {
    for (l1_i = L1.begin(); l1_i != l1_e; l1_i++) {
      if (i_dominates(*l1_i, *l2_i, par_ord)) break;
    }
    if (l1_i == l1_e) return(false);
  }

  return(true);
}
       
void remove_non_imaximal(set_next_col& L, bool **par_ord) {
  set_next_col::iterator l_i, l_j, l_k;

  for (l_i = L.begin(); l_i != L.end(); l_i++) {
    l_j = L.begin();
    while (l_j != L.end()) {
      l_k = l_j; l_k++;
      if ((l_i != l_j) && i_dominates(*l_i, *l_j, par_ord)) {
        L.erase(l_j);
      }
      l_j = l_k;
    }
  }
}

void add_opt_trans(BState *s, BState *to, bdd& label, bool **par_ord) {
  map<BState*, bdd>::iterator t_i, t_j;
  int c, c1;
  c = to->incoming - 1;
  
  // Check whether the new transition is not dominated
  for (t_i = s->trans->begin(); t_i != s->trans->end(); t_i++) {
    c1 = (t_i->first)->incoming - 1;
    if (par_ord[c][c1] && ((t_i->second << label) == bdd_true()))
      return;
  }
  
  // Erase transition which are dominated
  for (t_i = s->trans->begin(); t_i != s->trans->end();) {
    t_j = t_i; t_j++;
    c1 = (t_i->first)->incoming - 1;
    if (par_ord[c1][c] && ((label << t_i->second) == bdd_true()))
      s->trans->erase(t_i);
    t_i = t_j;
  }
  
  // Add the new transition
  if ((*s->trans)[to] == bdd_false()) {
    (*s->trans)[to] = label;
  } else {
    (*s->trans)[to] |= label;
  }
}

void cout_states_and_trans() {
  BState *s;
  bstate_count = 0;
  btrans_count = 0;
  for(s = bstates->nxt; s != bstates; s = s->nxt) {
    bstate_count++;
    btrans_count += s->trans->size();
  }

}

void remove_redundand_accept() {
  BState *s;
  b_rank = 1;
  bscc_stack = 0;

  if(bstates == bstates->nxt) return;

  for(s = bstates->nxt; s != bstates; s = s->nxt)
    s->incoming = 0; /* state color = white */

  bdfs(bstates->prv);

  for(s = bstates->nxt; s != bstates; s = s->nxt) {
    if(s->incoming == -1)
      s->final = accept+1;
    if(s->incoming == 0)
      s = remove_bstate(s, 0);
  }
}

void set_redundand_accept() {
  BState *s;
  b_rank = 1;
  bscc_stack = 0;

  if(bstates == bstates->nxt) return;

  for(s = bstates->nxt; s != bstates; s = s->nxt)
    s->incoming = 0; /* state color = white */

  bdfs(bstates->prv);

  for(s = bstates->nxt; s != bstates; s = s->nxt) {
    if(s->incoming == -1)
      s->final = accept;
    if(s->incoming == 0)
      s = remove_bstate(s, 0);
  }
}

/********************************************************************\
|*              Generation of the Buchi automaton                   *|
\********************************************************************/

BState *find_bstate(GState *state, int final, BState *s)
{                       /* finds the corresponding state, or creates it */
  if((s->gstate == state) && (s->final == final)) return s; /* same state */

#ifdef DICT
  // find the state
  BState** st_temp = &((*bsDict[final])[state]);
  if (*st_temp != NULL) {
    return *st_temp;
  }
#else
  s = bstack->nxt; /* in the stack */
  bstack->gstate = state;
  bstack->final = final;
  while(!(s->gstate == state) || !(s->final == final))
    s = s->nxt;
  if(s != bstack) return s;

  s = bstates->nxt; /* in the solved states */
  bstates->gstate = state;
  bstates->final = final;
  while(!(s->gstate == state) || !(s->final == final))
    s = s->nxt;
  if(s != bstates) return s;

  s = bremoved->nxt; /* in the removed states */
  bremoved->gstate = state;
  bremoved->final = final;
  while(!(s->gstate == state) || !(s->final == final))
    s = s->nxt;
  if(s != bremoved) return s;
#endif

  s = (BState *)tl_emalloc(sizeof(BState)); /* creates a new state */
  s->gstate = state;
  s->id = (state)->id;
  s->incoming = 0;
  s->final = final;
  s->trans = new map<BState*, bdd>();
  s->nxt = bstack->nxt;
  bstack->nxt = s;

#ifdef DICT
  // Insert a new state into dictionary
  *st_temp = s;
#endif
  return s;
}

int next_final(const cset &set, int fin) /* computes the 'final' value */
{
  if((fin != accept) && set.is_elem(final[fin + 1]))
    return next_final(set, fin + 1);
  return fin;
}

void make_btrans(BState *s) /* creates all the transitions from a state */
{
  int state_trans = 0;
  map<GState*, map<cset, bdd> >::iterator gt;
  map<cset, bdd>::iterator gt2;
  map<BState*, bdd>::iterator t1, tx;
  BState *s1;
  if(s->gstate->trans)
    for(gt = s->gstate->trans->begin(); gt != s->gstate->trans->end(); gt++) {
      for(gt2 = gt->second.begin(); gt2 != gt->second.end(); gt2++) {
        int fin = next_final(gt2->first, (s->final == accept) ? 0 : s->final);
        BState *to = find_bstate(gt->first, fin, s);

// on-the-fly optimalization has no sense as there is always only one transition to the state 'to'
//        for(t1 = s->trans->begin(); t1 != s->trans->end();) {
//          if(tl_simp_fly && 
//             (to == t1->first) &&
//             ((gt2->second << t1->second) == bdd_true())) { /* old t1 is redundant */
//            t1->first->incoming--;
//            tx = t1++;
//            s->trans->erase(tx);
//            state_trans--;
//          }
//          else if(tl_simp_fly &&
//                  (t1->first == to ) &&
//                  ((t1->second << gt2->second) == bdd_true())) /* new t is redundant */
//            break;
//          else
//           t1++;
//        }
//        if(t1 == s->trans->end()) {
          if ((*s->trans)[to] == bdd_false()) {
            to->incoming++;
            (*s->trans)[to] = gt2->second;
            state_trans++;
          } else {
            (*s->trans)[to] |= gt2->second;
          }
//        }
      }
    }
  
  if(tl_simp_fly) {
    if(s->trans->empty()) { /* s has no transitions */
      delete s->trans;
      s->trans = (map<BState*, bdd> *)0;
      s->prv = (BState *)0;
      s->nxt = bremoved->nxt;
      bremoved->nxt = s;
      for(s1 = bremoved->nxt; s1 != bremoved; s1 = s1->nxt)
        if(s1->prv == s)
          s1->prv = (BState *)0;
      return;
    }
    bstates->trans = s->trans;
    bstates->final = s->final;
    s1 = bstates->nxt;
    while(!all_btrans_match(s, s1))
      s1 = s1->nxt;
    if(s1 != bstates) { /* s and s1 are equivalent */
      decrement_incoming(s->trans);
      delete s->trans;
      s->trans = (map<BState*, bdd> *)0;
      s->prv = s1;
      s->nxt = bremoved->nxt;
      bremoved->nxt = s;
      for(s1 = bremoved->nxt; s1 != bremoved; s1 = s1->nxt)
        if(s1->prv == s)
          s1->prv = s->prv;
      return;
    } 
  }
  s->nxt = bstates->nxt; /* adds the current state to 'bstates' */
  s->prv = bstates;
  s->nxt->prv = s;
  bstates->nxt = s;
  btrans_count += state_trans;
  bstate_count++;
}

/********************************************************************\
|*                  Display of the Buchi automaton                  *|
\********************************************************************/

extern int print_or;

void print_buchi(BState *s) /* dumps the Buchi automaton */
{
  map<BState*, bdd>::iterator t;
  if(s == bstates) return;

  print_buchi(s->nxt); /* begins with the last state */

  fprintf(tl_out, "state ");
  if(s->id == -1)
    if(s->final == accept)
      fprintf(tl_out, "accept_init");
    else
      fprintf(tl_out, "init");
  else {
    if(s->final == accept)
      fprintf(tl_out, "accept");
    else
      fprintf(tl_out, "T%i", s->final);
    fprintf(tl_out, "_%i", s->id);
  }
  fprintf(tl_out, "\t %d", s->incoming);
  fprintf(tl_out, "\n");
  for(t = s->trans->begin(); t != s->trans->end(); t++) {
    if (t->second == bdd_true()) {
      fprintf(tl_out, "(1)");
    } else {
      print_or = 0;
      bdd_allsat(t->second, allsatPrintHandler);
    }
    fprintf(tl_out, " -> ");
    if(t->first->id == -1) 
      fprintf(tl_out, "init\n");
    else {
      if(t->first->final == accept)
        fprintf(tl_out, "accept");
      else
        fprintf(tl_out, "T%i", t->first->final);
      fprintf(tl_out, "_%i\n", t->first->id);
    }
  }
}

void print_spin_buchi() {
  map<BState*, bdd>::iterator t;
  BState *s;
  int accept_all = 0, init_count = 0;
  if(bstates->nxt == bstates) { /* empty automaton */
    fprintf(tl_out, "never {    /* ");
    put_uform();
    fprintf(tl_out, " */\n");
    fprintf(tl_out, "T0_init:\n");
    fprintf(tl_out, "\tfalse;\n");
    fprintf(tl_out, "}\n");
    return;
  }
  if(bstates->nxt->nxt == bstates && bstates->nxt->id == 0) { /* true */
    fprintf(tl_out, "never {    /* ");
    put_uform();
    fprintf(tl_out, " */\n");
    fprintf(tl_out, "accept_init:\n");
    fprintf(tl_out, "\tif\n");
    fprintf(tl_out, "\t:: (1) -> goto accept_init\n");
    fprintf(tl_out, "\tfi;\n");
    fprintf(tl_out, "}\n");
    return;
  }

  fprintf(tl_out, "never { /* ");
  put_uform();
  fprintf(tl_out, " */\n");
  for(s = bstates->prv; s != bstates; s = s->prv) {
    if(s->id == 0) { /* accept_all at the end */
      accept_all = 1;
      continue;
    }
    if(s->final == accept)
      fprintf(tl_out, "accept_");
    else fprintf(tl_out, "T%i_", s->final);
    if(s->id == -1)
      fprintf(tl_out, "init:\n");
    else fprintf(tl_out, "S%i:\n", s->id);
    if(s->trans->empty()) {
      fprintf(tl_out, "\tfalse;\n");
      continue;
    }
    fprintf(tl_out, "\tif\n");
     for(t = s->trans->begin(); t != s->trans->end(); t++) {
      fprintf(tl_out, "\t:: ");
      if (t->second == bdd_true()) {
        fprintf(tl_out, "(1)");
      } else {
        print_or = 0;
        bdd_allsat(t->second, allsatPrintHandler);
      }
      fprintf(tl_out, " -> goto ");
      if(t->first->final == accept)
        fprintf(tl_out, "accept_");
      else fprintf(tl_out, "T%i_", t->first->final);
      if(t->first->id == 0)
        fprintf(tl_out, "all\n");
      else if(t->first->id == -1)
        fprintf(tl_out, "init\n");
      else fprintf(tl_out, "S%i\n", t->first->id);
    }
    fprintf(tl_out, "\tfi;\n");
  }
  if(accept_all) {
    fprintf(tl_out, "accept_all:\n");
    fprintf(tl_out, "\tskip\n");
  }
  fprintf(tl_out, "}\n");
}

static std::ostream* where_os;
void allsatPrintHandlerDve(char* varset, int size)
{
  int print_and = 0;
  
  if (print_or) *where_os << " || ";
  *where_os << "(";
  for (int v=0; v<size; v++)
  {
    if (varset[v] < 0) continue;       
    if (print_and) *where_os << " && ";
    if (varset[v] == 0)
      *where_os << "not " << sym_table[v];
    else
      *where_os << sym_table[v];
    print_and = 1;
  }
  *where_os << ")";
  print_or = 1;
}

/********************************************************************\
|*                       Main method                                *|
\********************************************************************/

void mk_buchi() 
{/* generates a Buchi automaton from the generalized Buchi automaton */
  int i;
  BState *s = (BState *)tl_emalloc(sizeof(BState));
  map<GState*, map<cset, bdd> >::iterator gt;
  map<cset, bdd>::iterator gt2;
  map<BState*, bdd>::iterator t1, tx;
  accept = final[0] - 1;
#ifdef DICT
  bsDict = (map<GState*, BState*> **) tl_emalloc(final[0]*sizeof(map<GState*, BState*>*));
  for (i = 0; i < final[0]; i++) {
    bsDict[i] = new map<GState*, BState*>();
  }
#endif
  
#ifdef STATS
  if(tl_stats) getrusage(RUSAGE_SELF, &tr_debut);
#endif

  bstack        = (BState *)tl_emalloc(sizeof(BState)); /* sentinel */
  bstack->nxt   = bstack;
  bremoved      = (BState *)tl_emalloc(sizeof(BState)); /* sentinel */
  bremoved->nxt = bremoved;
  bstates       = (BState *)tl_emalloc(sizeof(BState)); /* sentinel */
  bstates->nxt  = s;
  bstates->prv  = s;

  s->nxt        = bstates; /* creates (unique) inital state */
  s->prv        = bstates;
  s->id = -1;
  s->incoming = 1;
  s->final = 0;
  s->gstate = 0;
  s->trans = new map<BState*, bdd>();
#ifdef DICT
  bsDict[0]->insert(pair<GState*, BState*>(0, s));
#endif
  for(i = 0; i < init_size; i++) 
    if(init[i])
      for(gt = init[i]->trans->begin(); gt != init[i]->trans->end(); gt++) {
        for(gt2 = gt->second.begin(); gt2 != gt->second.end(); gt2++) {
          int fin = next_final(gt2->first, 0);
          BState *to = find_bstate(gt->first, fin, s);

            if ((*s->trans)[to] == bdd_false()) {
              to->incoming++;
              (*s->trans)[to] = gt2->second;
            } else {
              (*s->trans)[to] |= gt2->second;
            }
        }
      }
  
  while(bstack->nxt != bstack) { /* solves all states in the stack until it is empty */
    s = bstack->nxt;
    bstack->nxt = bstack->nxt->nxt;
    if(!s->incoming) {
#ifdef DICT
      bsDict[s->final]->erase(s->gstate);
#endif
      free_bstate(s);
      continue;
    }
    make_btrans(s);
  }

  retarget_all_btrans();

#ifdef DICT
  for (i = 0; i < final[0]; i++) {
    delete bsDict[i];
  }
  tfree(bsDict);
#endif

#ifdef STATS
  if(tl_stats) {
    getrusage(RUSAGE_SELF, &tr_fin);
    timeval_subtract (&t_diff, &tr_fin.ru_utime, &tr_debut.ru_utime);
    fprintf(tl_out, "\nBuilding the Buchi automaton : %li.%06lis",
                t_diff.tv_sec, t_diff.tv_usec);
    fprintf(tl_out, "\n%i states, %i transitions\n", bstate_count, btrans_count);
  }
#endif

  if(tl_verbose) {
    fprintf(tl_out, "\nBuchi automaton before simplification\n");
    print_buchi(bstates->nxt);
    if(bstates == bstates->nxt) 
      fprintf(tl_out, "empty automaton, refuses all words\n");  
  }

  if(tl_simp_diff) {
//    simplify_btrans(); - it is done implicitly thanks to the representation of transitions
    if(tl_simp_scc) simplify_bscc();
    while(simplify_bstates()) { /* simplifies as much as possible */
      if(tl_simp_scc) simplify_bscc();
    }
    
    if(tl_verbose) {
      fprintf(tl_out, "\nBuchi automaton after simplification\n");
      print_buchi(bstates->nxt);
      if(bstates == bstates->nxt) 
        fprintf(tl_out, "empty automaton, refuses all words\n");
      fprintf(tl_out, "\n");
    }
  }

  if (tl_bisim)
    basic_bisim_reduction();
  if (tl_bisim_r) {
    int states, trans;
    basic_bisim_reduction();
    do {
      states = bstate_count;
      trans = btrans_count;
      
      set_redundand_accept();
      basic_bisim_reduction();
      remove_redundand_accept();
      basic_bisim_reduction();
    } while (bstate_count < states || btrans_count < trans);
  }
  if (tl_sim)
    strong_fair_sim_reduction();
  if (tl_sim_r) {
    int states, trans;
    strong_fair_sim_reduction();
    do {
      states = bstate_count;
      trans = btrans_count;
      
      set_redundand_accept();
      strong_fair_sim_reduction();
      remove_redundand_accept();
      strong_fair_sim_reduction();
    } while (bstate_count < states || btrans_count < trans);
  }

  print_spin_buchi();
}
