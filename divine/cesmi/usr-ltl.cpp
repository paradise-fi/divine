#ifndef _GNU_SOURCE
#define _GNU_SOURCE // asprintf
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cesmi.h>
#include <cesmi-ltl.h>

typedef struct {
    unsigned buchi:8;
    unsigned system:24;
} handle;

extern int buchi_property_count;

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
int buchi_property( const cesmi_setup *cs ) {
    struct buchi_setup *s = (struct buchi_setup *) cs->instance;
    return cs->property - s->property_count;
}

cesmi_node buchi_make_node( const cesmi_setup *setup, int size )
{
    cesmi_setup *orig = (cesmi_setup *) setup->loader;
    cesmi_node result = orig->make_node( orig, size + sizeof( int ) );
    result.memory += sizeof( int );
    return result;
}

cesmi_setup system_setup( const cesmi_setup *setup )
{
    cesmi_setup r = *setup;
    r.instance = ( (struct buchi_setup *) setup->instance )->instance;
    return r;
}

cesmi_setup override_setup( const cesmi_setup *setup )
{
    cesmi_setup r = *setup;
    r.make_node = &buchi_make_node;
    r.loader = (void *) setup;
    return r;
}

int buchi_get_initial( const cesmi_setup *setup, int handle, cesmi_node *to, get_initial_t next )
{
    cesmi_setup sys_setup = system_setup( setup );

    if ( buchi_property( setup ) < 0 )
        return next( &sys_setup, handle, to );

    cesmi_setup over = override_setup( &sys_setup );
    int new_handle = combine_handles( 0, next( &over, handle, to ) );

    if ( new_handle ) { /* to has not been created otherwise */
        to->memory -= sizeof( int ); /* adjust back */
        *buchi_state( *to ) = buchi_initial( buchi_property( setup ) );
    }
    return new_handle;
}

int buchi_get_successor( const cesmi_setup *setup, int handle,
                         cesmi_node from, cesmi_node *to, get_successor_t next )
{
    cesmi_setup sys_setup = system_setup( setup );

    if ( buchi_property( setup ) < 0 )
        return next( &sys_setup, handle, from, to );

    cesmi_setup over = override_setup( &sys_setup );
    int bh = buchi_handle( handle ), bs = 0;

    do {
        bs = buchi_next( buchi_property( setup ), *buchi_state( from ),
                         bh, setup, system_state( from ) );
        if ( !bs )
            ++ bh;
    } while ( bs == 0 );

    if ( bs > 0 ) { /* the buchi automaton can move */
        int system = next( &over, system_handle( handle ), system_state( from ), to );

        if ( system ) {
            to->memory -= sizeof( int ); /* adjust back */
            *buchi_state( *to ) = bs;
            return combine_handles( bh, system );
        } else {
            if ( system_handle( handle ) == 1 ) { /* system was deadlocked */
                *to = setup->clone_node( setup, from );
                *buchi_state( *to ) = bs;
                return combine_handles( bh + 1, 1 );
            }
            return buchi_get_successor( setup, combine_handles( bh + 1, 1 ), from, to, next );
        }
    }

    return 0; /* stuck */
}

uint64_t buchi_flags( const cesmi_setup *setup, cesmi_node n ) {
    if ( buchi_property( setup ) < 0 )
        return 0;

    if ( buchi_accepting( buchi_property( setup ), *buchi_state( n ) ) )
        return cesmi_accepting;
    return 0;
}

char *buchi_show_node( const cesmi_setup *setup, cesmi_node from, show_node_t next )
{
    cesmi_setup sys_setup = system_setup( setup );

    if ( buchi_property( setup ) < 0 )
        return next( &sys_setup, from );

    char *system = next( &sys_setup, system_state( from ) );
    char *res = 0;
    int result = asprintf( &res, "%s [LTL: %d]", system, *buchi_state( from ) );
    if ( result == -1 ) // asprintf failed
        res = system;   // this is better than nothing
    else
        free( system );
    return res;
}

char *buchi_show_transition( const cesmi_setup *setup, cesmi_node from, int handle, show_transition_t next )
{
    cesmi_setup sys_setup = system_setup( setup );

    if ( buchi_property( setup ) < 0 )
        return next( &sys_setup, from, handle );

    /* TODO LTL labels? */
    return next( &sys_setup, system_state( from ), system_handle( handle ) );
}

void buchi_setup( cesmi_setup *setup )
{
    struct buchi_setup *s = (struct buchi_setup *) malloc( sizeof( struct buchi_setup ) );
    int id = 0;
    s->instance = setup->instance;
    s->property_count = setup->property_count;
    setup->instance = s;
    while ( setup->property_count < s->property_count + buchi_property_count )
        setup->add_property( setup, NULL, strdup( buchi_formula( id++ ) ), cesmi_pt_buchi );
}
