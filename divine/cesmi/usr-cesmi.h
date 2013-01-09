// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

#ifndef DIVINE_GENERATOR_CESMI_CLIENT_H
#define DIVINE_GENERATOR_CESMI_CLIENT_H

enum cesmi_property_type { cesmi_pt_goal, cesmi_pt_deadlock, cesmi_pt_buchi };
enum cesmi_flags { cesmi_goal = 1, cesmi_accepting = 2 };

typedef struct {
    void *handle;
    char *memory;
} cesmi_node;

typedef struct {
    void *allocation_handle;
    cesmi_node (*make_node)( void *handle, int size );
    void *instance; // per-instance data; never touched by DiVinE
    int property_count;
    int instance_initialised;
    /* extensions at the end are ABI-compatible */
} cesmi_setup;

/* prototypes that CESMI modules need to implement */
#ifdef __cplusplus
extern "C" {
#endif

      void  setup             ( cesmi_setup * );
       int  get_initial       ( cesmi_setup *, int, cesmi_node * );
       int  get_successor     ( cesmi_setup *, int, char *, cesmi_node * );
  uint64_t  get_flags         ( cesmi_setup *, char * );
        int get_property_type ( cesmi_setup *, int );
      char *show_node         ( cesmi_setup *, char * );
      char *show_transition   ( cesmi_setup *, char *, int );
const char *show_property     ( cesmi_setup *, int );

#ifdef __cplusplus
}
#endif

#endif
