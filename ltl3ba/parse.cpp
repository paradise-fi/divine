/***** ltl3ba : parse.c *****/

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

extern int	tl_yylex(void);
extern int	tl_verbose, tl_simp_log, tl_rew_f, tl_determinize, tl_det_m;
extern void put_uform(void);

int	tl_yychar = 0;
YYSTYPE	tl_yylval;

void reinitParse() {
    tl_yychar = 0;
    tl_yylval = NULL;
}

static Node	*tl_formula(void);
static Node	*tl_factor(void);
static Node	*tl_level(int);

static int	prec[2][4] = {
	{ U_OPER,  V_OPER, 0, 0},  /* left associative */
	{ OR, AND, IMPLIES, EQUIV, },	/* left associative */
};

static int
implies(Node *a, Node *b)
{
  return
    (isequal(a,b) ||
     b->ntyp == TRUE ||
     a->ntyp == FALSE ||
     (b->ntyp == AND && implies(a, b->lft) && implies(a, b->rgt)) ||
     (a->ntyp == OR && implies(a->lft, b) && implies(a->rgt, b)) ||
     (a->ntyp == AND && (implies(a->lft, b) || implies(a->rgt, b))) ||
     (b->ntyp == OR && (implies(a, b->lft) || implies(a, b->rgt))) ||
     (b->ntyp == U_OPER && implies(a, b->rgt)) ||
     (a->ntyp == V_OPER && implies(a->rgt, b)) ||
     (a->ntyp == U_OPER && implies(a->lft, b) && implies(a->rgt, b)) ||
     (b->ntyp == V_OPER && implies(a, b->lft) && implies(a, b->rgt)) ||
     ((a->ntyp == U_OPER || a->ntyp == V_OPER) && a->ntyp == b->ntyp && 
         implies(a->lft, b->lft) && implies(a->rgt, b->rgt))
#ifdef NXT
     || (b->ntyp == NEXT && is_G(a) && implies(a, b->lft))
     || (a->ntyp == NEXT && is_F(b) && implies(a->lft, b))
     || (a->ntyp == NEXT && b->ntyp == NEXT && implies(a->lft, b->lft)));
     
#else
     );
#endif
}

static Node *
bin_simpler(Node *ptr)
{	Node *a, *b;

	if (ptr)
	switch (ptr->ntyp) {
	case U_OPER:
		if (ptr->rgt->ntyp == TRUE
		||  ptr->rgt->ntyp == FALSE
		||  ptr->lft->ntyp == FALSE)
		{	ptr = ptr->rgt;
			break;
		}
		if (implies(ptr->lft, ptr->rgt)) /* NEW */
		{	ptr = ptr->rgt;
		        break;
		}
		if (ptr->lft->ntyp == U_OPER
		&&  isequal(ptr->lft->lft, ptr->rgt))
		{	/* (p U q) U p = (q U p) */
			ptr->lft = ptr->lft->rgt;
			break;
		}
		
		/** NEW **/
		if (ptr->rgt->ntyp == V_OPER &&
		    ptr->rgt->rgt->ntyp == U_OPER &&
    		implies(ptr->lft, ptr->rgt->rgt->lft))
		{	/* if p'=>p then p' U (q R (p U r) = (q R (p U p)) */
			ptr = ptr->rgt;
			break;
		}
		
		if (ptr->rgt->ntyp == U_OPER
		&&  implies(ptr->lft, ptr->rgt->lft))
		{	/* NEW */
			ptr = ptr->rgt;
			break;
		}
		if (ptr->rgt->ntyp == U_OPER
		&&  implies(ptr->rgt->lft, ptr->lft))
		{	/** NEW **/
			ptr->rgt = ptr->rgt->rgt;
			break;
		}
#ifdef NXT
		/* X p U X q == X (p U q) */
		if (ptr->rgt->ntyp == NEXT
		&&  ptr->lft->ntyp == NEXT)
		{	ptr = tl_nn(NEXT,
				tl_nn(U_OPER,
					ptr->lft->lft,
					ptr->rgt->lft), ZN);
					/** NEW **/
					ptr = bin_simpler(ptr);
	        break;
		}

		/* NEW : F X p == X F p */
		if (ptr->lft->ntyp == TRUE &&
		    ptr->rgt->ntyp == NEXT) {
		  ptr = tl_nn(NEXT, tl_nn(U_OPER, True, ptr->rgt->lft), ZN);
		  /** NEW **/
		  ptr = bin_simpler(ptr);
		  break;
		}

#endif

		/* NEW : F G F p == G F p */
		if (ptr->lft->ntyp == TRUE &&
		    ptr->rgt->ntyp == V_OPER &&
		    ptr->rgt->lft->ntyp == FALSE &&
		    ptr->rgt->rgt->ntyp == U_OPER &&
		    ptr->rgt->rgt->lft->ntyp == TRUE) {
		  ptr = ptr->rgt;
		  break;
		}

		/* NEW */
		if (ptr->lft->ntyp != TRUE && 
		    implies(push_negation(tl_nn(NOT, dupnode(ptr->rgt), ZN)), 
			    ptr->lft))
		{       ptr->lft = True;
		        break;
		}
		
		/** NEW **/
		if (is_EVE(ptr->rgt) || is_INFp(ptr->rgt))
		  ptr = ptr->rgt;
		break;
	case V_OPER:
		if (ptr->rgt->ntyp == FALSE
		||  ptr->rgt->ntyp == TRUE
		||  ptr->lft->ntyp == TRUE)
		{	ptr = ptr->rgt;
			break;
		}
		if (implies(ptr->rgt, ptr->lft))
		{	/* p V p = p */	
			ptr = ptr->rgt;
			break;
		}
		/* F V (p V q) == F V q */
		if (ptr->lft->ntyp == FALSE
		&&  ptr->rgt->ntyp == V_OPER)
		{	ptr->rgt = ptr->rgt->rgt;
			break;
		}
		
		/** NEW **/
		if (ptr->lft->ntyp == V_OPER &&
		    implies(ptr->rgt, ptr->lft->lft))
		{	/* if r=>p then ((p V q) V r) = (q V r) */
		  ptr->lft = ptr->lft->rgt;
			break;
		}
#ifdef NXT
		/* X p V X q == X (p V q) */
		if (ptr->rgt->ntyp == NEXT
		&&  ptr->lft->ntyp == NEXT)
		{	ptr = tl_nn(NEXT,
				tl_nn(V_OPER,
					ptr->lft->lft,
					ptr->rgt->lft), ZN);
					ptr = bin_simpler(ptr);
	        break;
		}

		/* NEW : G X p == X G p */
		if (ptr->lft->ntyp == FALSE &&
		    ptr->rgt->ntyp == NEXT) {
		  ptr = tl_nn(NEXT, tl_nn(V_OPER, False, ptr->rgt->lft), ZN);
		  /** NEW **/
		  ptr = bin_simpler(ptr);
		  break;
		}
#endif
		/* NEW : G F G p == F G p */
		if (ptr->lft->ntyp == FALSE &&
		    ptr->rgt->ntyp == U_OPER &&
		    ptr->rgt->lft->ntyp == TRUE &&
		    ptr->rgt->rgt->ntyp == V_OPER &&
		    ptr->rgt->rgt->lft->ntyp == FALSE) {
		  ptr = ptr->rgt;
		  break;
		}

		/* NEW */
		if (ptr->rgt->ntyp == V_OPER
		&&  implies(ptr->rgt->lft, ptr->lft))
		{	ptr = ptr->rgt;
			break;
		}

		/* NEW */
		if (ptr->lft->ntyp != FALSE && 
		    implies(ptr->lft, 
			    push_negation(tl_nn(NOT, dupnode(ptr->rgt), ZN))))
		{       ptr->lft = False;
		        break;
		}
		
		/** NEW **/
		if (is_UNI(ptr->rgt) || is_INFp(ptr->rgt))
		  ptr = ptr->rgt;		
		break;
#ifdef NXT
	case NEXT:
		/* NEW : X G F p == G F p */
		if (ptr->lft->ntyp == V_OPER &&
		    ptr->lft->lft->ntyp == FALSE &&
		    ptr->lft->rgt->ntyp == U_OPER &&
		    ptr->lft->rgt->lft->ntyp == TRUE) {
		  ptr = ptr->lft;
		  break;
		}
		/* NEW : X F G p == F G p */
		if (ptr->lft->ntyp == U_OPER &&
		    ptr->lft->lft->ntyp == TRUE &&
		    ptr->lft->rgt->ntyp == V_OPER &&
		    ptr->lft->rgt->lft->ntyp == FALSE) {
		  ptr = ptr->lft;
		  break;
		}
		/** NEW **/
		if (is_INFp(ptr->lft))
		  ptr = ptr->lft;
		  
		/** NEW **/
		ptr->lft = bin_simpler(ptr->lft);
		break;
#endif
	case IMPLIES:
		if (implies(ptr->lft, ptr->rgt))
		  {	ptr = True;
			break;
		}
		ptr = tl_nn(OR, Not(ptr->lft), ptr->rgt);
		ptr = rewrite(ptr);
		break;
	case EQUIV:
		if (implies(ptr->lft, ptr->rgt) &&
		    implies(ptr->rgt, ptr->lft))
		  {	ptr = True;
			break;
		}
		a = rewrite(tl_nn(AND,
			dupnode(ptr->lft),
			dupnode(ptr->rgt)));
		b = rewrite(tl_nn(AND,
			Not(ptr->lft),
			Not(ptr->rgt)));
		ptr = tl_nn(OR, a, b);
		ptr = rewrite(ptr);
		break;
	case AND:
		/* p && (q U p) = p */
		if (ptr->rgt->ntyp == U_OPER
		&&  isequal(ptr->rgt->rgt, ptr->lft))
		{	ptr = ptr->lft;
			break;
		}
		if (ptr->lft->ntyp == U_OPER
		&&  isequal(ptr->lft->rgt, ptr->rgt))
		{	ptr = ptr->rgt;
			break;
		}

		/* p && (q V p) == q V p */
		if (ptr->rgt->ntyp == V_OPER
		&&  isequal(ptr->rgt->rgt, ptr->lft))
		{	ptr = ptr->rgt;
			break;
		}
		if (ptr->lft->ntyp == V_OPER
		&&  isequal(ptr->lft->rgt, ptr->rgt))
		{	ptr = ptr->lft;
			break;
		}

		/* (p U q) && (r U q) = (p && r) U q*/
		if (ptr->rgt->ntyp == U_OPER
		&&  ptr->lft->ntyp == U_OPER
		&&  isequal(ptr->rgt->rgt, ptr->lft->rgt))
		{	ptr = tl_nn(U_OPER,
				tl_nn(AND, ptr->lft->lft, ptr->rgt->lft),
				ptr->lft->rgt);
			/** NEW **/
			ptr->lft = bin_simpler(ptr->lft);
			ptr = bin_simpler(ptr);
			break;
		}

		/* (p V q) && (p V r) = p V (q && r) */
		if (ptr->rgt->ntyp == V_OPER
		&&  ptr->lft->ntyp == V_OPER
		&&  isequal(ptr->rgt->lft, ptr->lft->lft))
		{	ptr = tl_nn(V_OPER,
				ptr->rgt->lft,
				tl_nn(AND, ptr->lft->rgt, ptr->rgt->rgt));
			/** NEW **/
			ptr->rgt = bin_simpler(ptr->rgt);
			ptr = bin_simpler(ptr);
			break;
		}
#ifdef NXT
		/* X p && X q == X (p && q) */
		if (ptr->rgt->ntyp == NEXT
		&&  ptr->lft->ntyp == NEXT)
		{	ptr = tl_nn(NEXT,
				tl_nn(AND,
					ptr->rgt->lft,
					ptr->lft->lft), ZN);
			/** NEW **/
			ptr = bin_simpler(ptr);
			break;
		}
		/** NEW **/
		if (ptr->lft->ntyp == AND && ptr->rgt->ntyp == NEXT) {
		  if (ptr->lft->rgt->ntyp == NEXT) {
		    ptr = tl_nn(AND, ptr->lft->lft,
		      tl_nn(NEXT,
				  tl_nn(AND,
					  ptr->lft->rgt->lft,
					  ptr->rgt->lft), ZN));
			  ptr->rgt->lft = bin_simpler(ptr->rgt->lft);
			  break;
		  }
		  if (ptr->lft->lft->ntyp == NEXT) {
		    ptr = tl_nn(AND, ptr->lft->rgt,
		      tl_nn(NEXT,
				  tl_nn(AND,
					  ptr->lft->lft->lft,
					  ptr->rgt->lft), ZN));
			  ptr->rgt->lft = bin_simpler(ptr->rgt->lft);
			  break;
		  }
		}
		/** NEW **/
  	if (ptr->rgt->ntyp == AND && ptr->lft->ntyp == NEXT) {
		  if (ptr->rgt->rgt->ntyp == NEXT) {
		    ptr = tl_nn(AND,
		      tl_nn(NEXT,
				  tl_nn(AND,
					  ptr->lft->lft,
					  ptr->rgt->rgt->lft), ZN), ptr->rgt->lft);
			  ptr->lft->lft = bin_simpler(ptr->lft->lft);
			  break;
		  }
		  if (ptr->rgt->lft->ntyp == NEXT) {
		    ptr = tl_nn(AND,
		      tl_nn(NEXT,
				  tl_nn(AND,
					  ptr->lft->lft,
					  ptr->rgt->lft->lft), ZN), ptr->rgt->rgt);
			  ptr->lft->lft = bin_simpler(ptr->lft->lft);
			  break;
		  }
		}

#endif

		/* (p V q) && (r U q) == p V q */
		if (ptr->rgt->ntyp == U_OPER
		&&  ptr->lft->ntyp == V_OPER
		&&  isequal(ptr->lft->rgt, ptr->rgt->rgt))
		{	ptr = ptr->lft;
			/** NEW **/
			break;
		}

		if (isequal(ptr->lft, ptr->rgt)	/* (p && p) == p */
		||  ptr->rgt->ntyp == FALSE	/* (p && F) == F */
		||  ptr->lft->ntyp == TRUE	/* (T && p) == p */
		||  implies(ptr->rgt, ptr->lft))/* NEW */
		{	ptr = ptr->rgt;
			break;
		}	
		if (ptr->rgt->ntyp == TRUE	/* (p && T) == p */
		||  ptr->lft->ntyp == FALSE	/* (F && p) == F */
		||  implies(ptr->lft, ptr->rgt))/* NEW */
		{	ptr = ptr->lft;
			break;
		}
		
		/** NEW **/
    if (ptr->lft->ntyp == AND) {
      if (implies(ptr->rgt, ptr->lft->lft)) {
        ptr->lft = ptr->lft->rgt;
        break;
      }
      if (implies(ptr->rgt, ptr->lft->rgt)) {
        ptr->lft = ptr->lft->lft;
        break;
      }
    }
    if (ptr->rgt->ntyp == AND) {
      if (implies(ptr->lft, ptr->rgt->lft)) {
        ptr->rgt = ptr->rgt->rgt;
        break;
      }
      if (implies(ptr->lft, ptr->rgt->rgt)) {
        ptr->rgt = ptr->rgt->lft;
        break;
      }
    }
		
		/* NEW : F G p && F G q == F G (p && q) */
		if (ptr->lft->ntyp == U_OPER &&
		    ptr->lft->lft->ntyp == TRUE &&
		    ptr->lft->rgt->ntyp == V_OPER &&
		    ptr->lft->rgt->lft->ntyp == FALSE &&
		    ptr->rgt->ntyp == U_OPER &&
		    ptr->rgt->lft->ntyp == TRUE &&
		    ptr->rgt->rgt->ntyp == V_OPER &&
		    ptr->rgt->rgt->lft->ntyp == FALSE)
		  {
		    ptr = tl_nn(U_OPER, True,
				tl_nn(V_OPER, False,
				      tl_nn(AND, ptr->lft->rgt->rgt,
					    ptr->rgt->rgt->rgt)));
		    /** NEW **/
		    ptr = bin_simpler(ptr);
		    break;
		  }

		/* NEW */
		if (implies(ptr->lft, 
			    push_negation_simple(tl_nn(NOT, dupnode(ptr->rgt), ZN)))
		 || implies(ptr->rgt, 
			    push_negation_simple(tl_nn(NOT, dupnode(ptr->lft), ZN))))
		{       ptr = False;
		        break;
		}
		break;

	case OR:
		/* p || (q U p) == q U p */
		if (ptr->rgt->ntyp == U_OPER
		&&  isequal(ptr->rgt->rgt, ptr->lft))
		{	ptr = ptr->rgt;
			break;
		}

		/* p || (q V p) == p */
		if (ptr->rgt->ntyp == V_OPER
		&&  isequal(ptr->rgt->rgt, ptr->lft))
		{	ptr = ptr->lft;
			break;
		}

		/* (p U q) || (p U r) = p U (q || r) */
		if (ptr->rgt->ntyp == U_OPER
		&&  ptr->lft->ntyp == U_OPER
		&&  isequal(ptr->rgt->lft, ptr->lft->lft))
		{	ptr = tl_nn(U_OPER,
				ptr->rgt->lft,
				tl_nn(OR, ptr->lft->rgt, ptr->rgt->rgt));
			/** NEW **/
			ptr->rgt = bin_simpler(ptr->rgt);
			ptr = bin_simpler(ptr);
			break;
		}

		if (isequal(ptr->lft, ptr->rgt)	/* (p || p) == p */
		||  ptr->rgt->ntyp == FALSE	/* (p || F) == p */
		||  ptr->lft->ntyp == TRUE	/* (T || p) == T */
		||  implies(ptr->rgt, ptr->lft))/* NEW */
		{	ptr = ptr->lft;
			break;
		}	
		if (ptr->rgt->ntyp == TRUE	/* (p || T) == T */
		||  ptr->lft->ntyp == FALSE	/* (F || p) == p */
		||  implies(ptr->lft, ptr->rgt))/* NEW */
		{	ptr = ptr->rgt;
			break;
		}

    /** NEW **/
    if (ptr->lft->ntyp == OR) {
      if (implies(ptr->lft->lft, ptr->rgt)) {
        ptr->lft = ptr->lft->rgt;
        break;
      }
      if (implies(ptr->lft->rgt, ptr->rgt)) {
        ptr->lft = ptr->lft->lft;
        break;
      }
    }
    if (ptr->rgt->ntyp == OR) {
      if (implies(ptr->rgt->lft, ptr->lft)) {
        ptr->rgt = ptr->rgt->rgt;
        break;
      }
      if (implies(ptr->rgt->rgt, ptr->lft)) {
        ptr->rgt = ptr->rgt->lft;
        break;
      }
    }      

#ifdef NXT
    /** NEW **/
		/* X p || X q == X (p || q) */
		if (ptr->rgt->ntyp == NEXT
		&&  ptr->lft->ntyp == NEXT)
		{	ptr = tl_nn(NEXT,
				tl_nn(OR,
					ptr->rgt->lft,
					ptr->lft->lft), ZN);
			ptr = bin_simpler(ptr);
			break;
		}
#endif

		/* (p V q) || (r V q) = (p || r) V q */
		if (ptr->rgt->ntyp == V_OPER
		&&  ptr->lft->ntyp == V_OPER
		&&  isequal(ptr->lft->rgt, ptr->rgt->rgt))
		{	ptr = tl_nn(V_OPER,
				tl_nn(OR, ptr->lft->lft, ptr->rgt->lft),
				ptr->rgt->rgt);
			/** NEW **/
			ptr->lft = bin_simpler(ptr->lft);
			ptr = bin_simpler(ptr);
			break;
		}

		/* (p V q) || (r U q) == r U q */
		if (ptr->rgt->ntyp == U_OPER
		&&  ptr->lft->ntyp == V_OPER
		&&  isequal(ptr->lft->rgt, ptr->rgt->rgt))
		{	ptr = ptr->rgt;
			break;
		}		
		
		/* NEW : G F p || G F q == G F (p || q) */
		if (ptr->lft->ntyp == V_OPER &&
		    ptr->lft->lft->ntyp == FALSE &&
		    ptr->lft->rgt->ntyp == U_OPER &&
		    ptr->lft->rgt->lft->ntyp == TRUE &&
		    ptr->rgt->ntyp == V_OPER &&
		    ptr->rgt->lft->ntyp == FALSE &&
		    ptr->rgt->rgt->ntyp == U_OPER &&
		    ptr->rgt->rgt->lft->ntyp == TRUE)
		  {
		    ptr = tl_nn(V_OPER, False,
				tl_nn(U_OPER, True,
				      tl_nn(OR, ptr->lft->rgt->rgt,
					    ptr->rgt->rgt->rgt)));
		    break;
		  }
		break;

		/* NEW */
		if (implies(push_negation_simple(tl_nn(NOT, dupnode(ptr->rgt), ZN)),
			    ptr->lft)
		 || implies(push_negation_simple(tl_nn(NOT, dupnode(ptr->lft), ZN)),
			    ptr->rgt))
		{       ptr = True;
		        break;
		}
	}
	return ptr;
}

static Node *
bin_minimal(Node *ptr)
{       if (ptr)
	switch (ptr->ntyp) {
	case IMPLIES:
		return tl_nn(OR, Not(ptr->lft), ptr->rgt);
	case EQUIV:
		return tl_nn(OR, 
			     tl_nn(AND,dupnode(ptr->lft),dupnode(ptr->rgt)),
			     tl_nn(AND,Not(ptr->lft),Not(ptr->rgt)));
	}
	return ptr;
}

static Node *
tl_factor(void)
{	Node *ptr = ZN;

	switch (tl_yychar) {
	case '(':
		ptr = tl_formula();
		if (tl_yychar != ')')
			tl_yyerror("expected ')'");
		tl_yychar = tl_yylex();
		goto simpl;
	case NOT:
		ptr = tl_yylval;
		tl_yychar = tl_yylex();
		ptr->lft = tl_factor();
		ptr = push_negation(ptr);
		goto simpl;
	case ALWAYS:
		tl_yychar = tl_yylex();

		ptr = tl_factor();

		if(tl_simp_log) {
		  if (ptr->ntyp == FALSE
		      ||  ptr->ntyp == TRUE)
		    break;	/* [] false == false */
		  
		  if (ptr->ntyp == V_OPER)
		    {	if (ptr->lft->ntyp == FALSE)
		      break;	/* [][]p = []p */
		    
		    ptr = ptr->rgt;	/* [] (p V q) = [] q */
		    }
		}

		ptr = tl_nn(V_OPER, False, ptr);
		goto simpl;
#ifdef NXT
	case NEXT:
		tl_yychar = tl_yylex();

		ptr = tl_factor();

		if ((ptr->ntyp == TRUE || ptr->ntyp == FALSE)&& tl_simp_log)
			break;	/* X true = true , X false = false */

		ptr = tl_nn(NEXT, ptr, ZN);
		goto simpl;
#endif
	case EVENTUALLY:
		tl_yychar = tl_yylex();

		ptr = tl_factor();

		if(tl_simp_log) {
		  if (ptr->ntyp == TRUE
		      ||  ptr->ntyp == FALSE)
		    break;	/* <> true == true */

		  if (ptr->ntyp == U_OPER
		      &&  ptr->lft->ntyp == TRUE)
		    break;	/* <><>p = <>p */

		  if (ptr->ntyp == U_OPER)
		    {	/* <> (p U q) = <> q */
		      ptr = ptr->rgt;
		      /* fall thru */
		    }
		}

		ptr = tl_nn(U_OPER, True, ptr);
	simpl:
		if (tl_simp_log)
		  ptr = bin_simpler(ptr);
		break;
	case PREDICATE:
		ptr = tl_yylval;
		tl_yychar = tl_yylex();
		break;
	case TRUE:
	case FALSE:
		ptr = tl_yylval;
		tl_yychar = tl_yylex();
		break;
	}
	if (!ptr) tl_yyerror("expected predicate");
#if 0
	printf("factor:	");
	tl_explain(ptr->ntyp);
	printf("\n");
#endif
	return ptr;
}

static Node *
tl_level(int nr)
{	int i; Node *ptr = ZN;

	if (nr < 0)
		return tl_factor();

	ptr = tl_level(nr-1);
again:
	for (i = 0; i < 4; i++)
		if (tl_yychar == prec[nr][i])
		{	tl_yychar = tl_yylex();
			ptr = tl_nn(prec[nr][i],
				ptr, tl_level(nr-1));
			if(tl_simp_log) ptr = bin_simpler(ptr);
			else ptr = bin_minimal(ptr);
			goto again;
		}
	if (!ptr) tl_yyerror("syntax error");
#if 0
	printf("level %d:	", nr);
	tl_explain(ptr->ntyp);
	printf("\n");
#endif
	return ptr;
}

static Node *
tl_formula(void)
{	tl_yychar = tl_yylex();
	return tl_level(1);	/* 2 precedence levels, 1 and 0 */	
}

static Node* remove_V(Node *ptr) {
  Node *l, *r;

  if (ptr)
	switch(ptr->ntyp) {
	case V_OPER:
	  if (is_G(ptr)) {
	    ptr->rgt = remove_V(ptr->rgt);  
	  } else {
      l = ptr->lft;
      r = ptr->rgt;
      ptr = tl_nn(OR,
          tl_nn(V_OPER, tl_nn(FALSE, ZN, ZN), dupnode(r)), 
				  tl_nn(U_OPER, dupnode(r), tl_nn(AND, l, r)));
		}
	case OR:	
	case AND:
	case U_OPER:
	  ptr->lft = remove_V(ptr->lft);
	  ptr->rgt = remove_V(ptr->rgt);
	  break;
#ifdef NXT
	case NEXT:
#endif
	case NOT:
	  ptr->lft = remove_V(ptr->lft);
    break;
	case FALSE:
	case TRUE:
	case PREDICATE:
		break;
	default:
		printf("Unknown token: ");
		tl_explain(ptr->ntyp);
		break;
	}
  
  return ptr;
}

static Node* rewrite_U(Node *ptr) {
  Node *l, *r;

  if (ptr)
	switch(ptr->ntyp) {
	case U_OPER:
	  if (is_INFp(ptr->lft)) {
      l = rewrite_U(ptr->lft);
      r = rewrite_U(ptr->rgt);
      ptr = tl_nn(OR, r, 
  				  tl_nn(AND, l,
      		  tl_nn(U_OPER, tl_nn(TRUE, ZN, ZN), dupnode(r))));
		}
	case OR:	
	case AND:
	case V_OPER:
	  ptr->lft = rewrite_U(ptr->lft);
	  ptr->rgt = rewrite_U(ptr->rgt);
	  break;
#ifdef NXT
	case NEXT:
#endif
	case NOT:
	  ptr->lft = rewrite_U(ptr->lft);
    break;
	case FALSE:
	case TRUE:
	case PREDICATE:
		break;
	default:
		printf("Unknown token: ");
		tl_explain(ptr->ntyp);
		break;
	}
  
  return ptr;
}

static Node* rewrite_V(Node *ptr) {
  Node *l, *r;

  if (ptr)
	switch(ptr->ntyp) {
	case V_OPER:
	  if (is_INFp(ptr->lft)) {
      l = rewrite_V(ptr->lft);
      r = rewrite_V(ptr->rgt);
      ptr = tl_nn(OR,
          tl_nn(V_OPER, tl_nn(FALSE, ZN, ZN), r),
          tl_nn(AND, l, dupnode(r)));
		}
	case OR:	
	case AND:
	case U_OPER:
	  ptr->lft = rewrite_V(ptr->lft);
	  ptr->rgt = rewrite_V(ptr->rgt);
	  break;
#ifdef NXT
	case NEXT:
#endif
	case NOT:
	  ptr->lft = rewrite_V(ptr->lft);
    break;
	case FALSE:
	case TRUE:
	case PREDICATE:
		break;
	default:
		printf("Unknown token: ");
		tl_explain(ptr->ntyp);
		break;
	}
  
  return ptr;
}

static Node* negate(Node *ptr) {
  Node *l, *r;

  if (ptr)
	switch(ptr->ntyp) {

	case OR:
      l = negate(ptr->lft);
      r = negate(ptr->rgt);
	    ptr = tl_nn(AND, l, r);
    break;
	case AND:
      l = negate(ptr->lft);
      r = negate(ptr->rgt);
	    ptr = tl_nn(OR, l, r);
    break;
	case V_OPER:
      l = negate(ptr->lft);
      r = negate(ptr->rgt);
	    ptr = tl_nn(U_OPER, l, r);
    break;
	case U_OPER:
      l = negate(ptr->lft);
      r = negate(ptr->rgt);
	    ptr = tl_nn(V_OPER, l, r);
	  break;
#ifdef NXT
	case NEXT:
  	  l = negate(ptr->lft);
      ptr = tl_nn(NEXT, l, ZN);
	  break;
#endif
	case NOT:
	  ptr = dupnode(ptr->lft);
    break;
	case FALSE:
	  ptr = tl_nn(TRUE, ZN, ZN);
    break;
	case TRUE:
	  ptr = tl_nn(FALSE, ZN, ZN);
	  break;
	case PREDICATE:
	  ptr = tl_nn(NOT, dupnode(ptr), ZN);
		break;
	default:
		printf("Unknown token: ");
		tl_explain(ptr->ntyp);
		break;
	}
  
  return ptr;
}

static Node* determ(Node *ptr) {
  Node *neg, *l, *r;

  if (ptr)
	switch(ptr->ntyp) {
	case OR:
	  	if (is_LTL(ptr->lft) && !is_LTL(ptr->rgt)) {
	  	  r = determ(ptr->rgt);
	  	  neg = negate(ptr->lft);
	  	  ptr->rgt = tl_nn(AND, ptr->rgt, neg);
	  	  break;
	  	}
	  	if (!is_LTL(ptr->lft) && is_LTL(ptr->rgt)) {
	  	  l = determ(ptr->lft);
	  	  neg = negate(ptr->rgt);
	  	  ptr->lft = tl_nn(AND, ptr->lft, neg); 
	  	  break;
	  	}
  	  ptr->lft = determ(ptr->lft);
  	  ptr->rgt = determ(ptr->rgt);
  	  break;
	case AND:
	case V_OPER:
	case U_OPER:
	  break;
#ifdef NXT
	case NEXT:
#endif
	case NOT:
	case FALSE:
	case TRUE:
	case PREDICATE:
		break;
	default:
		printf("Unknown token: ");
		tl_explain(ptr->ntyp);
		break;
	}
  
  return ptr;
}

void
tl_parse(void)
{       Node *n = tl_formula();
        if (tl_verbose)
	{	printf("formula: ");
		put_uform();
		printf("\n");
	}
	if (tl_rew_f) {
	  n = rewrite_V(n);
	  if (tl_simp_log)
  	  n = bin_simpler(n);
	}
	trans(n);
}
