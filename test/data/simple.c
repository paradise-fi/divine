/*
 * This is an example of usage of the CESMI interface.
 * This file is processed by the DiVinE model checker as follows:
 *
 * 1) First we compile BenchmarkC.c into BenchmarkC.so:
 *
 *	$ divine compile --cesmi BenchmarkC.c
 *
 * 2) BenchmarkC.so can be used as input model to the DiVinE model checker:
 *
 *      $ divine info BenchmarkC.so
 * 	$ divine draw -l BenchmarkC.so
 * 	$ divine metrics BenchmarkC.so
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> //malloc
#include <string.h> //memset

#include <cesmi.h>

struct state
{
    int16_t a, b;
};

int get_initial( const cesmi_setup *setup, int handle, cesmi_node *to )
{
    if ( handle > 1 )
        return 0;

    *to = setup->make_node( setup, sizeof( struct state ) );
    struct state *s = (struct state *) to->memory;
    s->a = s->b = 0;
    return 2;
}

int get_successor( const cesmi_setup *setup, int handle, cesmi_node from, cesmi_node *to )
{
    struct state *in = (struct state *) from.memory;

    if (in->a < 4 && in->b < 4 && handle < 3) {
        *to = setup->make_node( setup, sizeof( struct state ) );
        struct state *out = (struct state *) to->memory;
        *out = *in;
        switch (handle) {
        case 1: out->a ++; return 2;
        case 2: out->b ++; return 3;
        }
    }
    return 0;
}

void setup( cesmi_setup *s )
{
    s->add_property( s, NULL, NULL, cesmi_pt_goal );
    s->add_property( s, NULL, NULL, cesmi_pt_deadlock );
}

char *show_node( const cesmi_setup *setup, cesmi_node from )
{
    struct state *in = (struct state *) from.memory;
    char *result;
    asprintf( &result, "a:%d, b:%d\n", in->a, in->b );
    return result;
}

char *show_transition( const cesmi_setup *setup, cesmi_node from, int handle )
{
    struct state *in = (struct state *) from.memory;
    switch (handle) {
    case 1: return strdup( "a++" );
    case 2: return strdup( "b++" );
    }
    return NULL;
}
