#define _GNU_SOURCE // asprintf
#include <stdio.h>
#include <malloc.h>

#include <cesmi.h>
#include <cesmi-ltl.h>

typedef struct {
    unsigned buchi:8;
    unsigned system:24;
} handle;

extern const int buchi_property_count;

int *buchi_state( cesmi_node state ) { return (int *)state.memory; }
cesmi_node system_state( cesmi_node state ) {
    cesmi_node n = { .handle = state.handle,
                     .memory = state.memory + sizeof( int ) };
    return n;
}
int buchi_handle( int handle ) { return handle >> 24; }
int system_handle( int handle ) { return handle & 0xffffff; }
int combine_handles( int buchi, int system ) {
    return (system & 0xffffff) | (buchi << 24);
}

cesmi_node buchi_make_node( void *meh, int size )
{
    cesmi_setup *setup = (cesmi_setup *) meh;
    cesmi_node result = setup->make_node( setup->allocation_handle, size + 4 );
    *buchi_state( result ) = 1;
    result.memory += 4;
    return result;
}

cesmi_setup override_setup( cesmi_setup *setup )
{
    cesmi_setup r = *setup;
    r.make_node = &buchi_make_node;
    r.allocation_handle = &setup;
    return r;
}

int buchi_get_initial( cesmi_setup *setup, int handle, cesmi_node *to, get_initial_t next )
{
    cesmi_setup over = override_setup( setup );
    return combine_handles( 0, next( &over, handle, to ) );
}

int buchi_get_successor( cesmi_setup *setup, int handle,
                         cesmi_node from, cesmi_node *to, get_successor_t next )
{
    cesmi_setup over = override_setup( setup );
    int bs = buchi_next( setup->property, *buchi_state( from ),
                         buchi_handle( handle ), system_state( from ) );

    if ( bs ) { /* the buchi automaton can move */
        int system = next( &over, system_handle( handle ), system_state( from ), to );

        if ( system ) {
            to->memory -= sizeof( int ); /* adjust back */
            *buchi_state( *to ) = bs;
            return combine_handles( buchi_handle( handle ), system );
        } else {
            if ( system_handle( handle ) == 1 ) { /* system was deadlocked */
                *to = setup->clone_node( setup->allocation_handle, from );
                *buchi_state( *to ) = bs;
            }
            return combine_handles( buchi_handle( handle ) + 1, 1 );
        }
    }

    return 0; /* stuck */
}

char *buchi_show_node( cesmi_setup *setup, cesmi_node from, show_node_t next )
{
    char *system = next( setup, system_state( from ) );
    char *res = 0;
    asprintf( &res, "%s [LTL: %d]", buchi_state( from ) );
    free( system );
    return res;
}

char *buchi_show_transition( cesmi_setup *setup, cesmi_node from, int handle, show_transition_t next )
{
    /* TODO LTL labels? */
    return next( setup, system_state( from ), system_handle( handle ) );
}

char *buchi_setup( cesmi_setup *setup )
{
    setup->property_count = buchi_property_count;
}
