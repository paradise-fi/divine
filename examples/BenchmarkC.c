/*
 * This is an example of usage of the CESMI interface.
 * This file is processed by the DiVinE model checker as follows:
 *
 * 1) First we compile BenchmarkC.c into BenchmarkC.so and BenchmarkC.o
 *
 *	$ divine compile --cesmi BenchmarkC.c
 *
 * 2) BenchmarkC.so can be used as input model to the DiVinE model checker,
 *    should anybody be interested in the state space graph of it, see e.g.
 *
 * 	$ divine draw -l BenchmarkC.so
 * 	$ divine metrics BenchmarkC.so
 *
 * 3) BenchmarkC.o is used for model checking purposes. First, the file must be combined
 *    with LTL formulas as listed in BenchmarkC.ltl:
 *
 *	$ divine combine -f BenchmarkC.ltl BenchmarkC.o
 *
 *    The previous command creates two new files in the working directory, BenchmarkC.prop1.so
 *    and BenchmarkC.prop2.so, which can be used as inputs to the model checker.
 *
 * 	$ divine verify BenchmarkC.prop1.so
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> //malloc

#include "../divine/generator/cesmi-client.h"

struct state {
    int16_t a, b;
};

static inline struct state *make( CESMISetup *setup, char **to ) {
    int size = sizeof( struct state ) + setup->slack;
    *to = pool_allocate_blob( setup->cpool, size );
    return (struct state *) ((*to) + setup->slack + 4); // FIXME
}

void get_system_initial( CESMISetup *setup, char **to )
{
    struct state *s = make( setup, to );
    s->a = s->b = 0;
}

int get_system_successor( CESMISetup *setup, int handle, char *from, char **to )
{
    struct state *in = (struct state *) (from + setup->slack + 4);

    if (in->a < 1024 && in->b < 1024 && handle < 3) {
        struct state *out = make( setup, to );
        *out = *in;
        switch (handle) {
        case 1: out->a ++; return 2;
        case 2: out->b ++; return 3;
        }
    }
    return 0;
}

void system_setup( CESMISetup *s ) {
    s->state_size = sizeof( struct state );
}

char *show_system_node( CESMISetup *setup, char *from )
{
    struct state *in = (struct state *) (from + setup->slack + 4); //FIXME
    char * ret = (char *) malloc ( 50 );  // FIXME
    sprintf (ret, "a:%d, b:%d\n", in->a, in->b );
    return ret;
}

char *show_system_transition( CESMISetup *setup, char *from, char *to )
{
    struct state *in = (struct state *) (from + setup->slack + 4); //FIXME
    struct state *out = (struct state *) (to + setup->slack + 4); //FIXME
    char * ret = (char *) malloc ( 50 );  // FIXME
    if (in->a != out->a)
      sprintf (ret, "a++" );
    else
      sprintf (ret, "b++" );
    return ret;
}

//Atomic proposition a: (a>b)
int prop_a( CESMISetup *setup, char *from )
{
    struct state *in = (struct state *) (from + setup->slack + 4);  //FIXME
    return (in->a > in->b);
}
