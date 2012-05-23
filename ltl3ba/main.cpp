/***** ltl3ba : main.c *****/

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
/* Some of the code in this file was taken from the Spin software         */
/* Written by Gerard J. Holzmann, Bell Laboratories, U.S.A.               */

#include <iostream>
#include <fstream>
#include "ltl3ba.h"
#include "unistd.h"

FILE	*tl_out;

#ifdef STATS
int	tl_stats     = 0; /* time and size stats */	
#endif
int tl_simp_log  = 1; /* logical simplification */
int tl_simp_diff = 1; /* automata simplification */
int tl_simp_fly  = 1; /* on the fly simplification */
int tl_simp_scc  = 1; /* use scc simplification */
int tl_fjtofj    = 1; /* 2eme fj */
int tl_postpone  = 1; /* use suspension for TGBA construction */
int tl_f_components = 1; /* use direct building of final TGBA components corresponding to GFp_1 && .. && GFp_n*/
int tl_rem_scc   = 1; /* enable removing non-accepting strongly connected components */
int tl_alt       = 1; /* use suspension for VWAA construction */
int tl_rew_f     = 1; /* rewrite R formulae with alternating subformulae */
int tl_det_m     = 0; /* construct more deterministic automaton */
int tl_determinize  = 0; /* old (not so efficient) determinization */
int tl_bisim     = 0; /* enable basic bisimulatin reduction of BA */
int tl_bisim_r   = 0; /* enable basic bisimulatin reduction of BA and repeat until there is no reduction */
int tl_sim       = 0; /* enable strong fair simulation reduction of BA */
int tl_sim_r     = 0; /* enable strong fair simulation reduction of BA and repeat until there is no reduction */
int tl_ltl3ba    = 1; /* enable some LTL3BA specific improvements */
int tl_print     = 1;
int tl_errs      = 0;
int tl_verbose   = 0;
int tl_terse     = 0;
unsigned long	All_Mem = 0;

std::string uform;
static int	hasuform=0, cnt=0;
std::string ltl_file;
std::string add_ltl;

void reinitParse();
void reinitCache();
void reinitMem();
void reinitWrt();
void reinitSet();
void reinitTrans();
void reinitGeneralized();
void reinitLex();
void reinitBuchi();
void reinitAlternating();

void reinit() {
    reinitParse();
    reinitCache();
    reinitMem();
    reinitWrt();
    reinitSet();
    reinitTrans();
    reinitGeneralized();
    reinitLex();
    reinitBuchi();
    reinitAlternating();

#ifdef STATS
    tl_stats = 0;
#endif
    tl_simp_log  = 1;
    tl_simp_diff = 1;
    tl_simp_fly  = 1;
    tl_simp_scc  = 1;
    tl_fjtofj    = 1;
    tl_postpone  = 1;
    tl_f_components = 1;
    tl_rem_scc   = 1;
    tl_alt       = 1;
    tl_rew_f     = 1;
    tl_det_m     = 0;
    tl_determinize  = 0;
    tl_bisim     = 0;
    tl_bisim_r   = 0;
    tl_sim       = 0;
    tl_sim_r     = 0;
    tl_ltl3ba    = 1;
    tl_print     = 1;
    tl_errs      = 0;
    tl_verbose   = 0;
    tl_terse     = 0;
    All_Mem      = 0;

    uform.clear();
    hasuform=0, cnt=0;
    ltl_file.clear();
    add_ltl.clear();

    bdd_done();
}

static void	tl_endstats(void);
static void	non_fatal(char *, char *);

inline void
alldone(int estatus)
{
        exit(estatus);
}

char *
emalloc(int n)
{       char *tmp;

        if (!(tmp = (char *) malloc(n)))
                fatal("not enough memory", (char *)0);
        memset(tmp, 0, n);
        return tmp;
}

int
tl_Getchar(void)
{
	if (cnt < hasuform)
		return uform[cnt++];
	cnt++;
	return -1;
}

void
put_uform(void)
{
	fprintf(tl_out, "%s", uform.c_str());
}

void
tl_UnGetchar(void)
{
	if (cnt > 0) cnt--;
}

void
usage(void)
{
        printf("usage: ltl3ba [-flag] -f formula\n");
        printf("                   or -F file\n");
        printf(" -f \"formula\"\ttranslate LTL ");
        printf("into never claim\n");
        printf(" -F file\tlike -f, but with the LTL ");
        printf("formula stored in a 1-line file\n");
        printf(" -d\t\tdisplay automata (D)escription at each step\n");
#ifdef STATS
        printf(" -s\t\tcomputing time and automata sizes S(t)atistics\n");
#endif
        printf(" -l\t\tdisable (L)ogic formula simplification\n");
        printf(" -p\t\tdisable a-(P)osteriori simplification\n");
        printf(" -o\t\tdisable (O)n-the-fly simplification\n");
        printf(" -c\t\tdisable strongly (C)onnected components simplification\n");
        printf(" -a\t\tdisable trick in (A)ccepting conditions\n");
        printf(" \n  LTL3BA specific options:\n");
        printf(" -P\t\tdisable (P)ostponing/suspension in TGBA construction\n");
        printf(" -D\t\tdisable (D)irect building of final components\n");
        printf(" -C\t\tdisable removing non-accepting strongly (C)onnected components\n");
        printf(" -A\t\tdisable suspension in (A)lternating automaton construction\n");
        printf(" -R\t\tdisable rewriting R formulae with alternating subformulae\n");
        printf(" -M\t\ttry to produce more deter(M)inistic automaton (recomended)\n");
        printf(" -B\t\tenable basic (B)isimulation reduction of BA\n");
        printf(" -S\t\tenable strong fair (S)imulation reduction of BA\n");
        printf(" -x\t\tdisable all LTL3BA specific improvements (act like LTL2BA)\n");
	
        alldone(1);
}

void init_buddy() {
  // Buddy might have been initialized by a third-party library.
  if (!bdd_isrunning()) {
    bdd_init(100000, 10000);
    // Disable the default GC handler.
    bdd_gbc_hook(0);
  }
}

int
tl_main(std::string &argv)
{ 
  int i;
	for (i = 0; i < argv.length(); i++)
	{	if (argv[i] == '\t'
		||  argv[i] == '\"'
		||  argv[i] == '\n')
			argv[i] = ' ';
	}
  uform = argv;
	hasuform = uform.length();
	if (hasuform == 0) usage();
	init_buddy();
	tl_parse();
	
#ifdef STATS
	if (tl_stats) tl_endstats();
#endif
	return tl_errs;
}

int
main_function(int argc, char *argv[], FILE* outFile)
{	int i;
  if (outFile)
    tl_out = outFile;
  else
  	tl_out = stdout;

	while (argc > 1 && argv[1][0] == '-')
        {       switch (argv[1][1]) {
                case 'F': if (*(argv+2)) ltl_file = *(argv+2);
                          argc--; argv++; break;
                case 'f': if (*(argv+2)) add_ltl = *(argv+2);
                          argc--; argv++; break;
                case 'a': tl_fjtofj = 0; break;
                case 'c': tl_simp_scc = 0; break;
                case 'o': tl_simp_fly = 0; break;
                case 'p': tl_simp_diff = 0; break;
                case 'l': tl_simp_log = 0; break;
                case 'd': tl_verbose = 1; break;
#ifdef STATS
                case 't': tl_stats = 1; break;
#endif
                case 'M': tl_det_m = 1; break;
                case 'P': tl_postpone = 0; break;
                case 'D': tl_f_components = 0; break;
                case 'C': tl_rem_scc = 0; break;
                case 'A': tl_alt = 0; break;
                case 'R': tl_rew_f = 0; break;
                case 'm': tl_determinize   = 1; break;
                case 'b': tl_bisim = 1; break;
                case 'B': tl_bisim_r = 1; break;
                case 's': tl_sim = 1; break;
                case 'S': tl_sim_r = 1; break;
                case 'x': tl_postpone = 0; tl_f_components = 0; tl_ltl3ba = 0; tl_rem_scc = 0; tl_alt = 0; tl_rew_f = 0; break;
                default : usage(); break;
                }
                argc--, argv++;
        }
  
        if(ltl_file.empty() && add_ltl.empty()) usage();

        if (!ltl_file.empty())
        {       std::ifstream in_file(ltl_file.c_str(), std::ifstream::in);
                if (!in_file.is_open())
                {       printf("ltl2ba: cannot open %s\n", ltl_file.c_str());
                        alldone(1);
                }
                std::getline(in_file, add_ltl, '\0');
                in_file.close();
        }
        if (argc > 1)
        {       usage();
        }
        else if (argc > 0)
        {
                if (outFile)
                {       tl_out = outFile;
                        tl_main(add_ltl);
                        fclose(outFile);
                }
                else
                {       return(tl_main(add_ltl));
                }
        }
        else
        {       usage();
        }
}

int
main_ltl3ba(int argc, char *argv[], std::ostream& out)
{
      reinit();
      /* disable the default promela output do std */
      tl_print = 0;
      
      int i = main_function(argc, argv, NULL);
      print_dve_buchi(out);
      return i;
}

int
main_ltl3ba(int argc, char *argv[], FILE* outFile)
{
      reinit();
      /* use the option to produce more deterinistic automaton by default */
      tl_det_m = 1;
      
      return main_function(argc, argv, outFile);
}

//int
//main(int argc, char *argv[])
//{
//      return main_function(argc, argv, NULL);
//}

#ifdef STATS
/* Subtract the `struct timeval' values X and Y, storing the result X-Y in RESULT.
   Return 1 if the difference is negative, otherwise 0.  */
 
/*int
timeval_subtract (result, x, y)
struct timeval *result, *x, *y;*/
int timeval_subtract (timeval *result, timeval *x, timeval *y)
{ 
	if (x->tv_usec < y->tv_usec) {
		x->tv_usec += 1000000;
		x->tv_sec--;
	}
	
	/* Compute the time remaining to wait. tv_usec is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;
	
	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}

static void
tl_endstats(void)
{	extern int Stack_mx;
	printf("\ntotal memory used: %9ld\n", All_Mem);
	/*printf("largest stack sze: %9d\n", Stack_mx);*/
	/*cache_stats();*/
	a_stats();
}
#endif

#define Binop(a)		\
		fprintf(tl_out, "(");	\
		dump(n->lft);		\
		fprintf(tl_out, a);	\
		dump(n->rgt);		\
		fprintf(tl_out, ")")

void
dump(Node *n)
{
	if (!n) return;

	switch(n->ntyp) {
	case OR:	Binop(" || "); break;
	case AND:	Binop(" && "); break;
	case U_OPER:	Binop(" U ");  break;
	case V_OPER:	Binop(" V ");  break;
#ifdef NXT
	case NEXT:
		fprintf(tl_out, "X");
		fprintf(tl_out, " (");
		dump(n->lft);
		fprintf(tl_out, ")");
		break;
#endif
	case NOT:
		fprintf(tl_out, "!");
		fprintf(tl_out, " (");
		dump(n->lft);
		fprintf(tl_out, ")");
		break;
	case FALSE:
		fprintf(tl_out, "false");
		break;
	case TRUE:
		fprintf(tl_out, "true");
		break;
	case PREDICATE:
		fprintf(tl_out, "(%s)", n->sym->name);
		break;
	case -1:
		fprintf(tl_out, " D ");
		break;
	default:
		printf("Unknown token: ");
		tl_explain(n->ntyp);
		break;
	}
}

void
tl_explain(int n)
{
	switch (n) {
	case ALWAYS:	printf("[]"); break;
	case EVENTUALLY: printf("<>"); break;
	case IMPLIES:	printf("->"); break;
	case EQUIV:	printf("<->"); break;
	case PREDICATE:	printf("predicate"); break;
	case OR:	printf("||"); break;
	case AND:	printf("&&"); break;
	case NOT:	printf("!"); break;
	case U_OPER:	printf("U"); break;
	case V_OPER:	printf("V"); break;
#ifdef NXT
	case NEXT:	printf("X"); break;
#endif
	case TRUE:	printf("true"); break;
	case FALSE:	printf("false"); break;
	case ';':	printf("end of formula"); break;
	default:	printf("%c", n); break;
	}
}

static void
non_fatal(const char *s1, char *s2)
{	extern int tl_yychar;
	int i;

	printf("ltl2ba: ");
	if (s2)
		printf(s1, s2);
	else
		printf("%s",s1);
	if (tl_yychar != -1 && tl_yychar != 0)
	{	printf(", saw '");
		tl_explain(tl_yychar);
		printf("'");
	}
	printf("\nltl2ba: %s\n-------", uform.c_str());
	for (i = 0; i < cnt; i++)
		printf("-");
	printf("^\n");
	fflush(stdout);
	tl_errs++;
}

void
tl_yyerror(const char *s1)
{
	Fatal(s1, (char *) 0);
}

void
Fatal(const char *s1, char *s2)
{
  non_fatal(s1, s2);
  alldone(1);
}

void
fatal(const char *s1, char *s2)
{
        non_fatal(s1, s2);
        alldone(1);
}


