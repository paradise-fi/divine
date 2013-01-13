#include "cesmi.h"

struct buchi_setup {
    int property_count;
    void *instance;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Code for buchi_next is generated from the LTL formula by divine compile. */
int buchi_next( int property, int from, int transition, cesmi_setup *setup, cesmi_node evalstate );
int buchi_accepting( int property, int id );

typedef int (*get_initial_t)( cesmi_setup *setup, int handle, cesmi_node *to );
typedef int (*get_successor_t)( cesmi_setup *setup, int handle, cesmi_node from, cesmi_node *to );
typedef char *(*show_node_t)( cesmi_setup *setup, cesmi_node from );
typedef char *(*show_transition_t)( cesmi_setup *setup, cesmi_node from, int handle );

uint64_t buchi_flags( cesmi_setup *setup, cesmi_node n );
int buchi_get_initial( cesmi_setup *setup, int handle, cesmi_node *to, get_initial_t next );
int buchi_get_successor( cesmi_setup *setup, int handle, cesmi_node from,
                         cesmi_node *to, get_successor_t next );
char *buchi_show_node( cesmi_setup *setup, cesmi_node from, show_node_t next );
char *buchi_show_transition( cesmi_setup *setup, cesmi_node from, int handle, show_transition_t next );

char *buchi_show_property( cesmi_setup *setup, int property );
void buchi_setup( cesmi_setup *setup );

#ifdef __cplusplus
}
#endif
