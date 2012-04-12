/***** ltl3ba : mem.c *****/

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
/* Send bug-reports and/or questions to Paul Gastin                       */
/* http://www.lsv.ens-cachan.fr/~gastin                                   */
/*                                                                        */
/* Some of the code in this file was taken from the Spin software         */
/* Written by Gerard J. Holzmann, Bell Laboratories, U.S.A.               */

#include "ltl3ba.h"

#if 1
#define log(e, u, d)	event[e][(int) u] += (long) d;
#else
#define log(e, u, d)
#endif

#define A_LARGE		80
#define A_USER		0x55000000
#define NOTOOBIG	32768

#define POOL		0
#define ALLOC		1
#define FREE		2
#define NREVENT		3

extern	unsigned long All_Mem;
extern	int tl_verbose;

ATrans *atrans_list = (ATrans *)0;
GTrans *gtrans_list = (GTrans *)0;

int aallocs = 0, afrees = 0, apool = 0;
int gallocs = 0, gfrees = 0, gpool = 0;
int ballocs = 0, bfrees = 0, bpool = 0;

union M {
	long size;
	union M *link;
};

static union M *freelist[A_LARGE];
static long	req[A_LARGE];
static long	event[NREVENT][A_LARGE];

void *
tl_emalloc(int U)
{	union M *m;
  	long r, u;
	void *rp;

	u = (long) ((U-1)/sizeof(union M) + 2);

	if (u >= A_LARGE)
	{	log(ALLOC, 0, 1);
		if (tl_verbose)
		printf("tl_spin: memalloc %ld bytes\n", u);
		m = (union M *) emalloc((int) u*sizeof(union M));
		All_Mem += (unsigned long) u*sizeof(union M);
	} else
	{	if (!freelist[u])
		{	r = req[u] += req[u] ? req[u] : 1;
			if (r >= NOTOOBIG)
				r = req[u] = NOTOOBIG;
			log(POOL, u, r);
			freelist[u] = (union M *)
				emalloc((int) r*u*sizeof(union M));
			All_Mem += (unsigned long) r*u*sizeof(union M);
			m = freelist[u] + (r-2)*u;
			for ( ; m >= freelist[u]; m -= u)
				m->link = m+u;
		}
		log(ALLOC, u, 1);
		m = freelist[u];
		freelist[u] = m->link;
	}
	m->size = (u|A_USER);

	for (r = 1; r < u; )
		(&m->size)[r++] = 0;

	rp = (void *) (m+1);
	memset(rp, 0, U);
	return rp;
}

void
tfree(void *v)
{	union M *m = (union M *) v;
	long u;

	--m;
	if ((m->size&0xFF000000) != A_USER)
		Fatal("releasing a free block", (char *)0);

	u = (m->size &= 0xFFFFFF);
	if (u >= A_LARGE)
	{	log(FREE, 0, 1);
		/* free(m); */
	} else
	{	log(FREE, u, 1);
		m->link = freelist[u];
		freelist[u] = m;
	}
}

ATrans* emalloc_atrans() {
  ATrans *result;
  if(!atrans_list) {
    result = (ATrans *)tl_emalloc(sizeof(ATrans));
    result->bad_nodes = new_set(0);
    apool++;
  }
  else {
    result = atrans_list;
    atrans_list = atrans_list->nxt;
    result->nxt = (ATrans *)0;
    clear_set(result->bad_nodes, 0);
  }
  aallocs++;
  return result;
}

void free_atrans(ATrans *t, int rec) {
  if(!t) return;
  if(rec) free_atrans(t->nxt, rec);
  t->nxt = atrans_list;
  atrans_list = t;
  t->label = bdd_false();
  afrees++;
}

void free_atrans_map(std::map<cset, ATrans*> *t) {
  if(!t) return;
  std::map<cset, ATrans*>::iterator t1;
  for(t1 = t->begin(); t1 != t->end(); t1++)
      free_atrans(t1->second, 0);
  delete t;
}

void free_all_atrans() {
  ATrans *t;
  while(atrans_list) {
    t = atrans_list;
    atrans_list = t->nxt;
    tfree(t->bad_nodes);
    tfree(t);
  }
}

GTrans* emalloc_gtrans() {
  GTrans *result;
  if(!gtrans_list) {
    result = (GTrans *)tl_emalloc(sizeof(GTrans));
    result->final = new cset(0);
    gpool++;
  }
  else {
    result = gtrans_list;
    gtrans_list = gtrans_list->nxt;
    result->final->clear();
  }
  gallocs++;
  return result;
}

void free_gtrans(GTrans *t, GTrans *sentinel, int fly) {
  gfrees++;
  if(sentinel && (t != sentinel)) {
    free_gtrans(t->nxt, sentinel, fly);
    if(fly) t->to->incoming--;
  }
  t->label == bdd_false(); // Zamysliet sa
  t->nxt = gtrans_list;
  gtrans_list = t;
}

void
a_stats(void)
{	long	p, a, f;
	int	i;

	printf(" size\t  pool\tallocs\t frees\n");

	for (i = 0; i < A_LARGE; i++)
	{	p = event[POOL][i];
		a = event[ALLOC][i];
		f = event[FREE][i];

		if(p|a|f)
		printf("%5d\t%6ld\t%6ld\t%6ld\n",
			i, p, a, f);
	}

	printf("atrans\t%6d\t%6d\t%6d\n", 
	       apool, aallocs, afrees);
	printf("gtrans\t%6d\t%6d\t%6d\n", 
	       gpool, gallocs, gfrees);
	printf("btrans\t%6d\t%6d\t%6d\n", 
	       bpool, ballocs, bfrees);
}
