#include <stdio.h>
#include <stdint.h>

#include "../divine/generator/custom-api.h"

struct state {
    int16_t a, b;
};

static inline struct state *make( CustomSetup *setup, char **to ) {
    int size = sizeof( struct state ) + setup->slack;
    *to = pool_allocate_blob( setup->pool, size );
    return (struct state *) ((*to) + setup->slack + 4); // FIXME
}

void get_initial( CustomSetup *setup, char **to )
{
    struct state *s = make( setup, to );
    s->a = s->b = 0;
}

int get_successor( CustomSetup *setup, int handle, char *from, char **to )
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

void setup( CustomSetup *s ) {
    s->state_size = sizeof( struct state );
}
