/***** ltl3ba : alternating.c *****/

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
#define NO 0
#define YES 1
/* Define whether to use supplementary outputs or not */
#define SUPP_OUT NO

using namespace std;

/********************************************************************\
|*              Structures and shared variables                     *|
\********************************************************************/

extern FILE *tl_out;
extern int tl_verbose, tl_stats, tl_simp_diff, tl_alt, tl_determinize, tl_det_m, tl_f_components;

Node **label;
char **sym_table;
map<cset, ATrans*> **transition;
#ifdef STATS
struct rusage tr_debut, tr_fin;
struct timeval t_diff;
#endif
int *final_set, node_id = 1, sym_id = 0, node_size, sym_size, nodes_num;
int astate_count = 0, atrans_count = 0;

int *INFp_nodes, *UXp_nodes, *GFcomp_nodes, *Falpha_nodes, **predecessors, *tecky, *V_nodes,
    *is_Gs, *is_UG, *UG_succ, *UG_pred;

int predicates;

map<cset, ATrans*> *build_alternating(Node *p);

/********************************************************************\
|*              Generation of the alternating automaton             *|
\********************************************************************/

int calculate_node_size(Node *p) /* returns the number of temporal nodes */
{
  switch(p->ntyp) {
  case AND:
  case OR:
  case U_OPER:
  case V_OPER:
    return(calculate_node_size(p->lft) + calculate_node_size(p->rgt) + 1);
#ifdef NXT
  case NEXT:
    return(calculate_node_size(p->lft) + 1);
#endif
  default:
    return 1;
    break;
  }
}

int calculate_sym_size(Node *p) /* returns the number of predicates */
{
  switch(p->ntyp) {
  case AND:
  case OR:
  case U_OPER:
  case V_OPER:
    return(calculate_sym_size(p->lft) + calculate_sym_size(p->rgt));
#ifdef NXT
  case NEXT:
    return(calculate_sym_size(p->lft));
#endif
  case NOT:
  case PREDICATE:
    return 1;
  default:
    return 0;
  }
}

ATrans *dup_trans(ATrans *trans)  /* returns the copy of a transition */
{
  ATrans *result;
  if(!trans) return trans;
  result = emalloc_atrans();
  result->label = trans->label;
  return result;
}

void do_merge_trans(ATrans **result, ATrans *trans1, ATrans *trans2) 
{ /* merges two transitions */
  if(!trans1 || !trans2) {
    free_atrans(*result, 0);
    *result = (ATrans *)0;
    return;
  }
  if(!*result)
    *result = emalloc_atrans();
  (*result)->label = trans1->label & trans2->label;
  if((*result)->label == bdd_false()) {
    free_atrans(*result, 0);
    *result = (ATrans *)0;
  }
}

ATrans *merge_trans(ATrans *trans1, ATrans *trans2) /* merges two transitions */
{
  ATrans *result = emalloc_atrans();
  do_merge_trans(&result, trans1, trans2);
  return result;
}

int already_done(Node *p) /* finds the id of the node, if already explored */
{
  int i;
  for(i = 1; i<node_id; i++) 
    if (isequal(p, label[i])) 
      return i;
  return -1;
}

int get_sym_id(char *s) /* finds the id of a predicate, or atttributes one */
{
  int i;
  for(i=0; i<sym_id; i++) 
    if (!strcmp(s, sym_table[i])) 
      return i;
  sym_table[sym_id] = s;
  return sym_id++;
}

map<cset, ATrans*> *boolean(Node *p) /* computes the transitions to boolean nodes -> next & init */
{
  ATrans *t;
  map<cset, ATrans*>::iterator t1, t2;
  map<cset, ATrans*> *lft, *rgt, *result = (map<cset, ATrans*> *)0;
  int id;
  switch(p->ntyp) {
  case TRUE:
    result = new map<cset, ATrans*>();
    t = emalloc_atrans();
    t->label = bdd_true();
    result->insert(pair<cset, ATrans*>(cset(0), t));
  case FALSE:
    break;
  case AND:
    lft = boolean(p->lft);
    rgt = boolean(p->rgt);
    if (lft && rgt)
      for(t1 = lft->begin(); t1 != lft->end(); t1++) {
        for(t2 = rgt->begin(); t2 != rgt->end(); t2++) {
          ATrans *tmp = merge_trans(t1->second, t2->second);
          if(tmp) {
            if (!result)
              result = new map<cset, ATrans*>();
            cset tmp_set(0);
            tmp_set.merge(t1->first, t2->first);
            result->insert(pair<cset, ATrans*>(tmp_set, tmp));
          }
        }
    }
    free_atrans_map(lft);
    free_atrans_map(rgt);
    break;
  case OR:
    lft = boolean(p->lft);
    result = new map<cset, ATrans*>();
    if (lft)
      for(t1 = lft->begin(); t1 != lft->end(); t1++) {
        ATrans *tmp = dup_trans(t1->second);
        result->insert(pair<cset, ATrans*>(cset(t1->first), tmp));
      }
    free_atrans_map(lft);
    rgt = boolean(p->rgt);
    if (rgt)
      for(t1 = rgt->begin(); t1 != rgt->end(); t1++) {
        ATrans **tmp = &(*result)[t1->first];
        if (*tmp) {
          (*tmp)->label |= t1->second->label;
        } else {
          *tmp = dup_trans(t1->second);
        }
      }
    free_atrans_map(rgt);
    break;
  default:
    build_alternating(p);
    result = new map<cset, ATrans*>();
    t = emalloc_atrans();
    t->label = bdd_true();
    result->insert(pair<cset, ATrans*>(cset(already_done(p), 0), t));
  }
  return result;
}

map<cset, ATrans*> *build_alternating(Node *p) /* builds an alternating automaton for p */
{
  ATrans *t;
  map<cset, ATrans*>::iterator t1, t2;
  map<cset, ATrans*> *lft, *rgt, *result = (map<cset, ATrans*> *)0;
  cset to(0);
  int node = already_done(p);
  if(node >= 0) return transition[node];
  int determ = 0, clear_lft = 0, clear_rgt = 0;

  switch (p->ntyp) {

  case TRUE:
    result = new map<cset, ATrans*>();
    t = emalloc_atrans();
    t->label = bdd_true();
    result->insert(pair<cset, ATrans*>(to, t));
  case FALSE:
    break;

  case PREDICATE:
    result = new map<cset, ATrans*>();
    t = emalloc_atrans();
    t->label = bdd_ithvar(get_sym_id(p->sym->name));
    result->insert(pair<cset, ATrans*>(to, t));
    break;

  case NOT:
    result = new map<cset, ATrans*>();
    t = emalloc_atrans();
    t->label = bdd_nithvar(get_sym_id(p->lft->sym->name));
    result->insert(pair<cset, ATrans*>(to, t));
    break;

#ifdef NXT
  case NEXT:
    if (!tl_determinize && !tl_det_m) {
      result = boolean(p->lft);
    } else {
      result = new map<cset, ATrans*>();
      build_alternating(p->lft);
      t = emalloc_atrans();
      t->label = bdd_true();
      result->insert(pair<cset, ATrans*>(cset(already_done(p->lft), 0), t));
    }
    break;
#endif

  case U_OPER:    /* p U q <-> q || (p && X (p U q)) */
    result = new map<cset, ATrans*>();
    if (tl_determinize && is_LTL(p->rgt))
      determ = 1;
    rgt = build_alternating(p->rgt);
    if (rgt)
      for(t2 = rgt->begin(); t2 != rgt->end(); t2++) {
        ATrans *tmp = dup_trans(t2->second); /* q */
        result->insert(pair<cset, ATrans*>(cset(t2->first), tmp));
      }
    
    lft = build_alternating(p->lft);
    node = already_done(p->lft);
    /* Modified construction on && p is alternating */
    if (tl_alt && in_set(INFp_nodes, node)) {
      ATrans *tmp = emalloc_atrans();
      tmp->label = bdd_true();
      add_set(tmp->bad_nodes, node_id); /* Mark the transition */
      to.insert(node_id);  /* X (p U q) */
      to.insert(node); /* X p */
      if (determ) {
        if (rgt)
          for(t2 = rgt->begin(); t2 != rgt->end(); t2++) { /* Adds !q */
            tmp->label &= ! t2->second->label;
          }
        if (tmp->label != bdd_false())
          result->insert(pair<cset, ATrans*>(to, tmp));
      } else
        result->insert(pair<cset, ATrans*>(to, tmp));
    } else {
      /* Deterministic construction */
      if (determ) {
         if (lft)
          for(t1 = lft->begin(); t1 != lft->end(); t1++) {
            ATrans *tmp = dup_trans(t1->second);  /* p */
            add_set(tmp->bad_nodes, node_id); /* Mark the transition */
            to = t1->first;
            to.insert(node_id);  /* X (p U q) */
            if (rgt)
              for(t2 = rgt->begin(); t2 != rgt->end(); t2++) { /* Adds !q */
                tmp->label &= ! t2->second->label;
              }
              if (tmp->label != bdd_false())
                result->insert(pair<cset, ATrans*>(to, tmp));
          }
      } else {
        /* Original construction */
        if (lft)
          for(t1 = lft->begin(); t1 != lft->end(); t1++) {
            ATrans *tmp = dup_trans(t1->second);  /* p */
            add_set(tmp->bad_nodes, node_id); /* Mark the transition */
            to = t1->first;
            to.insert(node_id);  /* X (p U q) */
            result->insert(pair<cset, ATrans*>(to, tmp));
          }
      }
    }
    add_set(final_set, node_id);
    if (is_G(p->rgt)) {
      if (!UG_succ) {
        UG_succ = (int *) tl_emalloc(nodes_num * sizeof(int));
        UG_pred = (int *) tl_emalloc(nodes_num * sizeof(int));
        is_Gs = make_set(-1, 0);
        is_UG = make_set(-1, 0);
      }
    
      add_set(is_UG, node_id);
      UG_succ[node_id - 1] = already_done(p->rgt);
      add_set(is_Gs, UG_succ[node_id - 1]);
/*      if (p->lft->ntyp != TRUE)*/
        UG_pred[UG_succ[node_id - 1] - 1] = node_id;
    }
    break;

  case V_OPER:    /* p V q <-> (p && q) || (q && X (p V q)) */
    if (tl_determinize && is_LTL(p->lft))
      determ = 1;

    rgt = build_alternating(p->rgt);
    lft = build_alternating(p->lft);
    node = already_done(p->lft);    
    if (rgt)
      for(t1 = rgt->begin(); t1 != rgt->end(); t1++) {
        ATrans *tmp;
        if (!result)
          result = new map<cset, ATrans*>();

/*        if (!tl_alt || !in_set(INFp_nodes, node)) {*/
          if (lft) {
            for(t2 = lft->begin(); t2 != lft->end(); t2++) {
              tmp = merge_trans(t1->second, t2->second);  /* p && q */
              if(tmp) {
                to.merge(t1->first, t2->first);
                result->insert(pair<cset, ATrans*>(to, tmp));
              }
            }
          }
/*        } else {
          tmp = dup_trans(t1->second);  /* q */
/*          to = t1->first;
          to.insert(node);  /* Xp && q */
/*          result->insert(pair<cset, ATrans*>(to, tmp));  
        }*/

      tmp = dup_trans(t1->second);  /* q */
      to = t1->first;
      to.insert(node_id);  /* X (p V q) */

      if (determ) { /* add !p so it will be ((q && !p) && X (p V q))*/
        if (lft)
          for(t2 = lft->begin(); t2 != lft->end(); t2++) {
            tmp->label &= ! t2->second->label;
          }
        if (tmp->label != bdd_false())
          result->insert(pair<cset, ATrans*>(to, tmp));
      } else {
        result->insert(pair<cset, ATrans*>(to, tmp));
      }
    }
    break;

  case AND:
    lft = build_alternating(p->lft);
    rgt = build_alternating(p->rgt);
    if (tl_alt && (p->lft->ntyp == V_OPER || p->lft->ntyp == U_OPER) && 
        is_INFp(p->lft)) {
      lft = new map<cset, ATrans*>();
      t = emalloc_atrans();
      t->label = bdd_true();
      lft->insert(pair<cset, ATrans*>(cset(already_done(p->lft), 0), t));
      clear_lft = 1;
    }
    if (tl_alt && (p->rgt->ntyp == V_OPER || p->rgt->ntyp == U_OPER) &&
        is_INFp(p->rgt)) {
      rgt = new map<cset, ATrans*>();
      t = emalloc_atrans();
      t->label = bdd_true();
      rgt->insert(pair<cset, ATrans*>(cset(already_done(p->rgt), 0), t));
      clear_rgt = 1;
    }
    if (lft && rgt)
      for(t1 = lft->begin(); t1 != lft->end(); t1++) {
        for(t2 = rgt->begin(); t2 != rgt->end(); t2++) {
          ATrans *tmp = merge_trans(t1->second, t2->second);
          if(tmp) {
            if (!result)
              result = new map<cset, ATrans*>();
            cset to(0);
            to.merge(t1->first, t2->first);
            result->insert(pair<cset, ATrans*>(to, tmp));
          }
        }
      }
    if (clear_lft)
      free_atrans_map(lft);
    if (clear_rgt)
      free_atrans_map(rgt);
    break;

  case OR:
    result = new map<cset, ATrans*>();
    lft = build_alternating(p->lft);
    rgt = build_alternating(p->rgt);
    if (tl_alt && (p->lft->ntyp == V_OPER || p->lft->ntyp == U_OPER) && 
        is_INFp(p->lft)) {
      lft = new map<cset, ATrans*>();
      t = emalloc_atrans();
      t->label = bdd_true();
      lft->insert(pair<cset, ATrans*>(cset(already_done(p->lft), 0), t));
      clear_lft = 1;
    }
    if (tl_alt && (p->rgt->ntyp == V_OPER || p->rgt->ntyp == U_OPER) &&
        is_INFp(p->rgt)) {
      rgt = new map<cset, ATrans*>();
      t = emalloc_atrans();
      t->label = bdd_true();
      rgt->insert(pair<cset, ATrans*>(cset(already_done(p->rgt), 0), t));
      clear_rgt = 1;
    }
    if (tl_determinize && is_LTL(p->lft) && !is_LTL(p->rgt)) {
      if (rgt)
        for(t1 = rgt->begin(); t1 != rgt->end(); t1++) {
          ATrans *tmp = dup_trans(t1->second);
          if (lft)
            for(t2 = lft->begin(); t2 != lft->end(); t2++) {
              tmp->label &= ! t2->second->label;
            }
          if (tmp->label != bdd_false()) {
            result->insert(pair<cset, ATrans*>(cset(t1->first), tmp));
          } else
            free_atrans(tmp, 0);
        }
      if (lft)
        for(t1 = lft->begin(); t1 != lft->end(); t1++) {
            ATrans** tmp = &(*result)[t1->first];
            if (*tmp) {
              (*tmp)->label |= t1->second->label;
            } else {
              *tmp = dup_trans(t1->second);
            }
        }
    } else if (tl_determinize && !is_LTL(p->lft) && is_LTL(p->rgt)) {
      if (lft)
        for(t1 = lft->begin(); t1 != lft->end(); t1++) {
          ATrans *tmp = dup_trans(t1->second);
          if (rgt)
            for(t2 = rgt->begin(); t2 != rgt->end(); t2++) {
              tmp->label &= ! t2->second->label;
            }
          if (tmp->label != bdd_false())
            result->insert(pair<cset, ATrans*>(cset(t1->first), tmp));
          else
            free_atrans(tmp, 0);
        }
      if (rgt)
        for(t1 = rgt->begin(); t1 != rgt->end(); t1++) {
          ATrans **tmp = &(*result)[t1->first];
          if (*tmp) {
            (*tmp)->label |= t1->second->label;
          } else {
            *tmp = dup_trans(t1->second);
          }
        }
    } else {
      if (lft)
        for(t1 = lft->begin(); t1 != lft->end(); t1++) {
          ATrans *tmp = dup_trans(t1->second);
          result->insert(pair<cset, ATrans*>(cset(t1->first), tmp));
        }
      if (rgt)
        for(t1 = rgt->begin(); t1 != rgt->end(); t1++) {
          ATrans **tmp = &(*result)[t1->first];
          if (*tmp) {
            (*tmp)->label |= t1->second->label;
          } else {
            *tmp = dup_trans(t1->second);
          }
        }
    }
    if (clear_lft)
      free_atrans_map(lft);
    if (clear_rgt)
      free_atrans_map(rgt);
    break;

  default:
    break;
  }

  if (tl_det_m) {   
    if(result)
      for(t1 = result->begin(); t1 != result->end(); t1++) {
        for(t2 = result->begin(); t2 != result->end(); ) {
          if (t1 != t2 && included_set(t1->first.get_set(), t2->first.get_set(), 0)) {
            t2->second->label &= ! t1->second->label;
            
            if (t2->second->label == bdd_false()) {
              map<cset, ATrans*>::iterator tx = t2++;
              free_atrans(tx->second, 0);
              result->erase(tx);
            } else {
              t2++;
            }
          } else {
            t2++;
          }
          
        }
      }
  }

#if SUPP_OUT == YES
          printf("\n");
#endif

  transition[node_id] = result;
  if (is_INFp(p)) add_set(INFp_nodes, node_id);
  if (is_UXp(p)) add_set(UXp_nodes, node_id);
  if (is_GF_component(p)) add_set(GFcomp_nodes, node_id);
  if (is_Falpha(p)) add_set(Falpha_nodes, node_id);
  if (p->ntyp == V_OPER) add_set(V_nodes, node_id);
  label[node_id++] = p;
  return(result);
}

/********************************************************************\
|*        Simplification of the alternating automaton               *|
\********************************************************************/
void allsatPrintHandler(char* varset, int size);

void simplify_atrans(map<cset, ATrans*> *trans) /* simplifies the transitions */
{
  map<cset, ATrans*>::iterator t1, t2, tx;
  if(trans)
    for(t1 = trans->begin(); t1 != trans->end(); t1++) {
      for(t2 = t1, t2++; t2 != trans->end(); ) {
        if(t1->first.is_subset_of(t2->first) &&
           ((t1->second->label << t2->second->label) == bdd_true())) {
          tx = t2++;
          free_atrans(tx->second, 0);
          trans->erase(tx);
          atrans_count++;
        } else {
          t2++;
        }
      }
    }
}

void simplify_astates() /* simplifies the alternating automaton */
{
  map<cset, ATrans*>::iterator t;
  int i, *acc = make_set(-1, 0); /* no state is accessible initially */

  if (transition[0])
    for(t = transition[0]->begin(); t != transition[0]->end(); t++)
      merge_sets(acc, t->first.get_set(), 0); /* all initial states are accessible */

  for(i = node_id - 1; i > 0; i--) {
    if (!in_set(acc, i)) { /* frees unaccessible states */
      label[i] = ZN;
      free_atrans_map(transition[i]);
      transition[i] = (map<cset, ATrans*> *)0;
      continue;
    }
    astate_count++;
    if (!tl_f_components || !in_set(GFcomp_nodes, i))
      simplify_atrans(transition[i]);
    if (transition[i])
      for(t = transition[i]->begin(); t != transition[i]->end(); t++)
        merge_sets(acc, t->first.get_set(), 0);
  }

  tfree(acc);
}

/********************************************************************\
|*            Display of the alternating automaton                  *|
\********************************************************************/

int print_or;

void allsatPrintHandler(char* varset, int size)
{
  int print_and = 0;
  
  if (print_or) fprintf(tl_out, " || ");
  fprintf(tl_out, "(");
  for (int v=0; v<size; v++)
  {
    if (varset[v] < 0) continue;       
    if (print_and) fprintf(tl_out, " && ");
    if (varset[v] == 0)
      fprintf(tl_out, "!%s", sym_table[v]);
    else
      fprintf(tl_out, "%s", sym_table[v]);
    print_and = 1;
  }
  fprintf(tl_out, ")");
  print_or = 1;
}

void print_alternating() /* dumps the alternating automaton */
{
  int i;
  map<cset, ATrans*>::iterator t;

  fprintf(tl_out, "init :\n");
  if (transition[0])
    for(t = transition[0]->begin(); t != transition[0]->end(); t++) {
      t->first.print();
      fprintf(tl_out, "\n");
    }
  
  for(i = node_id - 1; i > 0; i--) {
    if(!label[i])
      continue;
    if (in_set(tecky, i))
      fprintf(tl_out, "* state %i : ", i);
    else
      fprintf(tl_out, "state %i : ", i);
    dump(label[i]);
    fprintf(tl_out, "\n");
    if (transition[i])
      for(t = transition[i]->begin(); t != transition[i]->end(); t++) {
        if (t->second->label == bdd_true()) {
          fprintf(tl_out, "(1)");
        } else {
          print_or = 0;
          bdd_allsat(t->second->label, allsatPrintHandler);
        }
        fprintf(tl_out, " -> ");
        t->first.print();
        fprintf(tl_out, "\t\t");
        print_set(t->second->bad_nodes, 0);
        fprintf(tl_out, "\n");
      }
  }
}

void count_predecessors_sets(int nodes_num) {
  map<cset, ATrans*>::iterator t;
  int x, y, i, j, mod = 8 * sizeof(int);

  for(i=0; i<nodes_num; i++) {
    predecessors[i] = make_set(-1, 0);
    add_set(predecessors[i], i);
  }

  for(y = node_id - 1; y > 0; y--) {
    for(x = node_id - 1; x > 0; x--) { /* pre kazdu hranu */
      if (transition[x])
        for(t = transition[x]->begin(); t != transition[x]->end(); t++) {
          for(i = 0; i < node_size; i++)
            for(j = 0; j < mod; j++)
              if((t->first.get_set())[i] & (1 << j))
                merge_sets(predecessors[mod * i + j], predecessors[x], 0);
        }
    }
  }
  
  for(i=0; i<nodes_num; i++) {
 	  rem_set(predecessors[i], i);
  }
}

void print_predecessors_sets() {
  int i;
  
  fprintf(tl_out, "\nPredecessors sets:\n");
	
  for (i = node_id - 1; i > 0; i--) {
    fprintf(tl_out, "%i -> ", i);
    print_set(predecessors[i], 0);
    fprintf(tl_out, "\n");
  }
  fprintf(tl_out, "\n");
}

void oteckuj(int nodes_num) {
  int i, ii, jj, node, mod = 8 * sizeof(int);
  Node *n;
  map<cset, ATrans*>::iterator t;
  Queue *q = create_queue(nodes_num);
  int *in_queue = make_set(-1, 0);
  
  /* Add initial states to queue*/
  if (transition[0])
    for(t = transition[0]->begin(); t != transition[0]->end(); t++) {
      for(ii = 0; ii < node_size; ii++) {
        for(jj = 0; jj < mod; jj++) {
          if((t->first.get_set())[ii] & (1 << jj)) {
            if(!in_set(in_queue, (mod * ii + jj))) {
            	add_set(in_queue, (mod * ii + jj));
	            push(q, (mod * ii + jj));
            }
          }
        }
      }
    }
  
  while (!is_empty(q)) {
    node = pop(q);
    rem_set(in_queue, node);
    n = label[node];

    if (transition[node])
      for(t = transition[node]->begin(); t != transition[node]->end(); t++) {
        switch(n->ntyp) {
        case V_OPER:
          /* Has loop */
          if (in_set(t->first.get_set(), node)) {
            for(ii = 0; ii < node_size; ii++) {
              for(jj = 0; jj < mod; jj++) {
                if((t->first.get_set())[ii] & (1 << jj)) {
                  if(node != (mod * ii + jj) && !in_set(tecky, (mod * ii + jj))) {
                    add_set(tecky, (mod * ii + jj));
                    if(!in_set(in_queue, (mod * ii + jj))) {
                      add_set(in_queue, (mod * ii + jj));
                      push(q, (mod * ii + jj));
                    }
                  }
                }
              }
            }
            break;
          }
        case OR:
        case AND:
        case U_OPER:
  #ifdef NXT
        case NEXT:
  #endif
        case NOT:
        case FALSE:
        case TRUE:
        case PREDICATE:
          for(ii = 0; ii < node_size; ii++) {
            for(jj = 0; jj < mod; jj++) {
              if((t->first.get_set())[ii] & (1 << jj)) {
                if(node != (mod * ii + jj)) {
                  if (in_set(tecky, node)) {
                    if (in_set(tecky, (mod * ii + jj)))
                      continue;
                    else
                      add_set(tecky, (mod * ii + jj));
                  }
                  if(!in_set(in_queue, (mod * ii + jj))) {
                    add_set(in_queue, (mod * ii + jj));
                    push(q, (mod * ii + jj));
                  }
                }
              }
            }
          }
          break;
        default:
          printf("Unknown token: ");
          tl_explain(n->ntyp);
          break;
        }
      }
  }

  free_queue(q);
  tfree(in_queue);
}

/********************************************************************\
|*                       Main method                                *|
\********************************************************************/

void mk_alternating(Node *p) /* generates an alternating automaton for p */
{
#ifdef STATS
  if(tl_stats) getrusage(RUSAGE_SELF, &tr_debut);
#endif

  node_size = calculate_node_size(p) + 1; /* number of states in the automaton */
  label = (Node **) tl_emalloc(node_size * sizeof(Node *));
  transition = (map<cset, ATrans*> **) tl_emalloc(node_size * sizeof(map<cset, ATrans*> *));
  predecessors = (int **) tl_emalloc(node_size * sizeof(int *));
  nodes_num = node_size;
  node_size = node_size / (8 * sizeof(int)) + 1;
  INFp_nodes = make_set(-1, 0);
  UXp_nodes = make_set(-1, 0);
  GFcomp_nodes = make_set(-1, 0);
  Falpha_nodes = make_set(-1, 0);
  V_nodes = make_set(-1, 0);
  tecky = make_set(-1, 0);
  is_Gs = make_set(-1, 0);
  is_UG = make_set(-1, 0);


  sym_size = calculate_sym_size(p); /* number of predicates */
  predicates = sym_size;
  if(sym_size) sym_table = (char **) tl_emalloc(sym_size * sizeof(char *));
  sym_size = sym_size / (8 * sizeof(int)) + 1;

  if (predicates > 2)
    bdd_setvarnum(predicates);
  else
    bdd_setvarnum(2);
  
  final_set = make_set(-1, 0);
  if (!tl_determinize && !tl_det_m) {
    transition[0] = boolean(p); /* generates the alternating automaton */
  } else {
    build_alternating(p); /* generates the alternating automaton */
    transition[0] = new map<cset, ATrans*>();
    ATrans *t = emalloc_atrans();
    t->label = bdd_true();
    transition[0]->insert(pair<cset, ATrans*>(cset(already_done(p), 0), t));
  }

  if(tl_verbose) {
    fprintf(tl_out, "\nAlternating automaton before simplification\n");
    print_alternating();
  }

  if(tl_simp_diff) {
    simplify_astates(); /* keeps only accessible states */
    oteckuj(nodes_num);
    if(tl_verbose) {
      fprintf(tl_out, "\nAlternating automaton after simplification\n");
      print_alternating();
    }
  } else {
    oteckuj(nodes_num);
  }
  count_predecessors_sets(nodes_num);
/*  print_predecessors_sets();*/

#ifdef STATS
  if(tl_stats) {
    getrusage(RUSAGE_SELF, &tr_fin);
    timeval_subtract (&t_diff, &tr_fin.ru_utime, &tr_debut.ru_utime);
    fprintf(tl_out, "\nBuilding and simplification of the alternating automaton: %li.%06lis",
		t_diff.tv_sec, t_diff.tv_usec);
    fprintf(tl_out, "\n%i states, %i transitions\n", astate_count, atrans_count);
  }
#endif

  releasenode(1, p);
  tfree(label);
}
