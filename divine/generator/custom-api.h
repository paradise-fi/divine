// -*- C++ -*- (c) 2010 Petr Rockai <me@mornfall.net>

// Not part of the DiVinE *library* headers.

#ifndef DIVINE_GENERATOR_CUSTOM_API_H
#define DIVINE_GENERATOR_CUSTOM_API_H

#define HINTCOUNT 128

#ifndef __cplusplus
typedef char *CBlob;

typedef struct CPoolGroup
{
    size_t item, used, total;
    char **free; // reuse allocation
    char *current; // tail allocation
    char *last; // bumper
} CPoolGroup;

typedef struct CPool {
    size_t group_count;
    size_t group_struct_size;
    char *group_array;
} CPool;

static inline CPoolGroup *pool_group( CPool *pool, int n ) {
    return (CPoolGroup *)(pool->group_array + n * pool->group_struct_size);
}

CPoolGroup *pool_extend( CPool *pool, int sz );
void *pool_allocate( CPool *pool, int sz );
void *pool_allocate_blob( CPool *pool, int sz );

#else
typedef divine::Blob CBlob;
typedef divine::Pool CPool;
#endif


/**
 * The C-based interface for cached state generation. Caching in this context
 * means obtaining successor lists from more than one state in a single
 * batch. This is useful since even C-based call into a shared object is
 * relatively costly -- too costly to be done for each state in the state
 * space, if generation of a single state is very cheap otherwise.
 *
 * There are 2 circular buffers involved: one for inputs (the states for which
 * we need successors) and one for outputs (the successors themselves). A third
 * buffer, which has a single number for each of the input states, is used to
 * keep the count of output states corresponding to each of the input states.
 *
 * Additionally, a couple of pointers into the buffer are maintained. 3 point
 * into the input buffer, partitioning it into 3 blocks:
 *
 * - done: <first_done, last_done>
 * - active: (last_done, last_in>
 * - inactive (empty): (last_in, first_done>
 *
 * The output buffer is partitioned into just 2 areas, active and inactive. At
 * the index given by first_out, a first successor of the state at in[last_done
 * + 1] is given (assuming count[last_done + 1] > 0), and so on.
 */
struct SuccessorCache {
    int size; // = HINTCOUNT

    // indices for "in"
    int first_done;
    int last_done; // + 1 = first_in
    int last_in;

    // indices for "out"
    int first_out; // corresponds to first_done
    int last_out; // corresponds to last_done

    int handle; // the handle to continue generation at last_done

    int count[HINTCOUNT * 2];

    CBlob in[HINTCOUNT * 2];
    CBlob out[HINTCOUNT * 4];
};

typedef struct CustomSetup {
    CPool *pool; // the pool to obtain memory for new states from
    void *custom; // per-instance data; never touched by DiVinE

    int slack; // state data starts at this offset. Set by DiVinE!
    int state_size; // The shared object fills this in. Use 0 for variable.
    int has_property; // set to 1 if this is a Buchi automaton (has accepting
                      // states and we are looking for cycles)
} CustomSetup;

#endif
