/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#ifndef INC_hashtab
#define INC_hashtab


#include "nipsvm.h"


#ifdef __cplusplus
extern "C"
{
#endif


// hashtable pointer type
typedef struct t_hashtab_header *t_hashtab;
typedef struct table_statistics_t table_statistics_t;

struct table_statistics_t {
        size_t memory_size;
        unsigned long entries_used;
        unsigned long entries_available;
        unsigned long conflicts;
        unsigned long max_retries;
};


// create a new hash table
extern t_hashtab hashtab_new( unsigned long entries, unsigned long retries );


// free a hash table
extern void hashtab_free( t_hashtab hashtab );


// get index for/of an entry in a hash table
//  - returns:  1 if state can be inserted into hash table
//              0 if state is already in hash table
//             -1 if state did not fit into hash table (hash conflict that could not be resolved)
//  - fills *p_pos with pointer to position for/of entry (not for return value -1)
extern int
hashtab_get_pos (t_hashtab hashtab, size_t size, nipsvm_state_t *state, nipsvm_state_t ***p_pos);


// put a state into a hash table
//  - only the pointer is stored
//  - returns:  1 if state was added to hash table
//              0 if state was already in hash table
//             -1 if state did not fit into hash table (hash conflict that could not be resolved)
extern int
hashtab_insert (t_hashtab hashtab, size_t size, nipsvm_state_t *state);


// retrieve statistical information about hashtable
extern void
table_statistics (t_hashtab hashtab, table_statistics_t *stats);


#ifdef __cplusplus
} // extern "C"
#endif


#endif // #ifndef INC_hashtab
