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

void get_initial( CESMISetup *setup, char **to )
{
    struct state *s = make( setup, to );
    s->a = s->b = 0;
}

int get_successor( CESMISetup *setup, int handle, char *from, char **to )
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

void setup( CESMISetup *s ) {
    s->state_size = sizeof( struct state );
}

char *show_node( CESMISetup *setup, char *from )
{
    struct state *in = (struct state *) (from + setup->slack + 4); //FIXME
    char * ret = (char *) malloc ( 50 );  // FIXME
    sprintf (ret, "a:%d, b:%d\n", in->a, in->b );
    return ret;
}

char *show_transition( CESMISetup *setup, char *from, char *to )
{
    struct state *in = (struct state *) (from + setup->slack + 4); //FIXME
    struct state *out = (struct state *) (to + setup->slack + 4); //FIXME
    char * ret = (char *) malloc ( 50 );  // FIXME
    if (in->a == out->a)
      sprintf (ret, "b++" );
    else
      sprintf (ret, "a++" );
    return ret;
}
