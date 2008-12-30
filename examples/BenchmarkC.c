#include <stdio.h>
#include <stdint.h>

struct state {
    int16_t a, b;
};

void get_initial_state(char *to)
{
    static struct state in = { 0, 0 };
    *(struct state *)to = in;
}

int get_successor(int handle, char *from, char *to)
{
    struct state *in = (struct state *) from,
                *out = (struct state *) to;

    if (in->a < 1024 && in->b < 1024) {
        *out = *in;
        switch (handle) {
        case 1: out->a ++; return 2;
        case 2: out->b ++; return 3;
        case 3: return 0;
        }
    } else {
        return 0;
    }
}

int get_state_size() {
    return sizeof( struct state );
}
