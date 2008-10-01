/* NIPS VM - New Implementation of Promela Semantics Virtual Machine
 * Copyright (C) 2005: Stefan Schuermans <stefan@schuermans.info>
 *                     Michael Weber <michaelw@i2.informatik.rwth-aachen.de>
 *                     Lehrstuhl fuer Informatik II, RWTH Aachen
 * Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html
 */

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "state.h"
#include "tools.h"

#include "hashtab.h"

#define HASH_MIN_ENTRIES 65536 // 64k entries (because of calculation of hash_rest)

// type for hash values
typedef unsigned long t_hash;

typedef st_global_state_header *t_hashtab_value;

typedef struct t_hashtab_bucket
{
  t_hashtab_value value;
  unsigned short hash_rest; // part of hash value that is lost in "entry = hash_value % entries"
                            // ---> "hash_rest = hash_value / entries"
} PACKED st_hashtab_bucket;


// hashtable pointer type
typedef struct t_hashtab_header
{
  unsigned long entries; // number of entries in the hash table
  unsigned long retries; // maximum allowed number of retries by re-hashing
  unsigned long memory_size; // size of entire hash table in bytes
  unsigned int  max_retry_count; // maximum number of retries needed so far
  unsigned long conflict_count; // number of conflicts found so far
  st_hashtab_bucket *bucket; // entries of hashtable
} st_hashtab_header;


// HASH FUNCTION TAKEN FROM http://burtleburtle.net/bob/hash/doobs.html - begin


typedef  uint32_t  ub4;   /* unsigned 4-byte quantities */
typedef  uint8_t   ub1;   /* unsigned 1-byte quantities */

#define hashsize(n) ((ub4)1<<(n))
#define hashmask(n) (hashsize(n)-1)

/*
 * --------------------------------------------------------------------
 *  mix -- mix 3 32-bit values reversibly.
 *  For every delta with one or two bits set, and the deltas of all three
 *    high bits or all three low bits, whether the original value of a,b,c
 *    is almost all zero or is uniformly distributed,
 *  * If mix() is run forward or backward, at least 32 bits in a,b,c
 *    have at least 1/4 probability of changing.
 *  * If mix() is run forward, every bit of c will change between 1/3 and
 *    2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
 *    mix() was built out of 36 single-cycle latency instructions in a 
 *    structure that could supported 2x parallelism, like so:
 *      a -= b; 
 *      a -= c; x = (c>>13);
 *      b -= c; a ^= x;
 *      b -= a; x = (a<<8);
 *      c -= a; b ^= x;
 *      c -= b; x = (b>>13);
 *      ...
 *  Unfortunately, superscalar Pentiums and Sparcs can't take advantage 
 *  of that parallelism.  They've also turned some of those single-cycle
 *  latency instructions into multi-cycle latency instructions.  Still,
 *  this is the fastest good hash I could find.  There were about 2^^68
 *  to choose from.  I only looked at a billion or so.
 * --------------------------------------------------------------------
 */
#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

/*
 * --------------------------------------------------------------------
 *  hash() -- hash a variable-length key into a 32-bit value
 *    k       : the key (the unaligned variable-length array of bytes)
 *    len     : the length of the key, counting by bytes
 *    initval : can be any 4-byte value
 * Returns a 32-bit value.  Every bit of the key affects every bit of
 * the return value.  Every 1-bit and 2-bit delta achieves avalanche.
 * About 6*len+35 instructions.
 *
 * The best hash table sizes are powers of 2.  There is no need to do
 * mod a prime (mod is sooo slow!).  If you need less than 32 bits,
 * use a bitmask.  For example, if you need only 10 bits, do
 *   h = (h & hashmask(10));
 * In which case, the hash table should have hashsize(10) elements.
 *
 * If you are hashing n strings (ub1 **)k, do it like this:
 *   for (i=0, h=0; i<n; ++i) h = hash( k[i], len[i], h);
 *
 * By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use this
 * code any way you wish, private, educational, or commercial.  It's free.
 *
 * See http://burtleburtle.net/bob/hash/evahash.html
 * Use for hash table lookup, or anything where one collision in 2^^32 is
 * acceptable.  Do NOT use for cryptographic purposes.
 * --------------------------------------------------------------------
 */

static inline ub4 hash( k, length, initval)
register ub1 *k;        /* the key */
register ub4  length;   /* the length of the key */
register ub4  initval;  /* the previous hash, or an arbitrary value */
{
   register ub4 a,b,c,len;

   /* Set up the internal state */
   len = length;
   a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
   c = initval;         /* the previous hash value */

   /*---------------------------------------- handle most of the key */
   while (len >= 12)
   {
      a += (k[0] +((ub4)k[1]<<8) +((ub4)k[2]<<16) +((ub4)k[3]<<24));
      b += (k[4] +((ub4)k[5]<<8) +((ub4)k[6]<<16) +((ub4)k[7]<<24));
      c += (k[8] +((ub4)k[9]<<8) +((ub4)k[10]<<16)+((ub4)k[11]<<24));
      mix(a,b,c);
      k += 12; len -= 12;
   }

   /*------------------------------------- handle the last 11 bytes */
   c += length;
   switch(len)              /* all the case statements fall through */
   {
   case 11: c+=((ub4)k[10]<<24);
   case 10: c+=((ub4)k[9]<<16);
   case 9 : c+=((ub4)k[8]<<8);
      /* the first byte of c is reserved for the length */
   case 8 : b+=((ub4)k[7]<<24);
   case 7 : b+=((ub4)k[6]<<16);
   case 6 : b+=((ub4)k[5]<<8);
   case 5 : b+=k[4];
   case 4 : a+=((ub4)k[3]<<24);
   case 3 : a+=((ub4)k[2]<<16);
   case 2 : a+=((ub4)k[1]<<8);
   case 1 : a+=k[0];
     /* case 0: nothing left to add */
   }
   mix(a,b,c);
   /*-------------------------------------------- report the result */
   return c;
}


// HASH FUNCTION TAKEN FROM http://burtleburtle.net/bob/hash/doobs.html - end


// a hash function for binary data
static t_hash hashtab_hash( unsigned char * p_data, unsigned long size, unsigned long initval )
{
  return hash( p_data, size, initval );
}

// create a new hash table
t_hashtab hashtab_new( unsigned long entries, unsigned long retries ) // extern
{
  unsigned long size;
  t_hashtab hashtab;
  char * ptr;

  // correct parameters
  entries = max( entries, HASH_MIN_ENTRIES );
  retries = max( retries, 1 );

  // calculate size
  size = sizeof( st_hashtab_header ); // header
  size += entries * sizeof( st_hashtab_bucket ); // entries
  
  // allocate memory
  hashtab = (t_hashtab)calloc( 1, size );
  if( hashtab == NULL )
    return NULL;

  // initialize hashtab header
  hashtab->entries = entries;
  hashtab->retries = retries;
  hashtab->memory_size = size;

  // initialize hashtable
  ptr = (char *)hashtab + sizeof( st_hashtab_header ); // pointer to array with entries
  hashtab->bucket = (st_hashtab_bucket *)ptr;

  return hashtab;
}


// free a hash table
void hashtab_free( t_hashtab hashtab ) // extern
{
  free( hashtab );
}


// get index for/of an entry in a hash table
//  - returns:  1 if state can be inserted into hash table
//              0 if state is already in hash table
//             -1 if state did not fit into hash table (hash conflict that could not be resolved)
//  - fills *p_pos with pointer to position for/of entry (not for return value -1)
extern int
hashtab_get_pos (t_hashtab hashtab, size_t size, nipsvm_state_t *state,
				 nipsvm_state_t ***p_pos)
{
  unsigned long entry, i;
  t_hash hash, k;
  
  // get hashtable entry
  hash = hashtab_hash( (unsigned char *)state, size, 0 );
  k = 1 + (hash % (hashtab->entries - 1)); // naive double hashing
  entry = hash % hashtab->entries;
  for( i = 0; i < hashtab->retries; i++ )
  {
    if( hashtab->bucket[entry].value == NULL )
    {
      hashtab->bucket[entry].hash_rest = hash / hashtab->entries;
      *p_pos = &hashtab->bucket[entry].value; // return pointer to position
      return 1; // can be inserted into hash table
    }
    else if( hashtab->bucket[entry].hash_rest != hash / hashtab->entries )
    {
      // no match, try next
    }
    else if( nipsvm_state_compare( hashtab->bucket[entry].value, state, size ) == 0 )
    {
      *p_pos = &hashtab->bucket[entry].value; // return pointer to position
      return 0;
    }
    hashtab->conflict_count++;
    hashtab->max_retry_count = max( hashtab->max_retry_count, i + 1 );
	entry = (entry + k) % hashtab->entries;  // considers indexes h, h + k, ..., h + i*k (mod entries)
  } // for( i ...
  
  // no more space ---> unresolvable hash conflict
  *p_pos = NULL;
  return -1;
}


// put a state into a hash table
//  - only the pointer is stored
//  - returns:  1 if state was added to hash table
//              0 if state was already in hash table
//             -1 if state did not fit into hash table (hash conflict that could not be resolved)
extern int
hashtab_insert( t_hashtab hashtab, size_t size, nipsvm_state_t *state )
{
  unsigned long ret_val;
  t_hashtab_value* pos;

  // get indices where to place entry
  ret_val = hashtab_get_pos( hashtab, size, state, &pos );

  // put pointer to state into hash table if it is a new one
  if( ret_val > 0 )
    *pos = state;

  return ret_val;
}


// output statistical information about hashtable
void hashtab_print_statistics( FILE * stream, t_hashtab hashtab )
{
  unsigned int i;
  unsigned int entries = 0;

  // count entries in hash table
  for( i = 0; i < hashtab->entries; i++ )
    if( hashtab->bucket[i].value != NULL )
      entries++;

  double fill_ratio = 100.0 * (double)entries / (double)hashtab->entries;

  fprintf( stream, "hashtable statistics:\n"
                   "            memory usage: %0.2fMB\n", hashtab->memory_size / 1048576.0 );
  fprintf( stream, "  buckets used/available: %u/%lu (%0.1f%%)\n", entries, hashtab->entries, fill_ratio );
  fprintf( stream, "     max. no. of retries: %u\n", hashtab->max_retry_count );
  fprintf( stream, "    conflicts (resolved): %lu\n", hashtab->conflict_count );
}

// output statistical information about hashtable
extern void
table_statistics (t_hashtab table, table_statistics_t *stats)
{
        unsigned int i;
        unsigned int n_entries = 0;
        
        assert (table != NULL);
        assert (stats != NULL);
    
        // count entries in hash table
		for( i = 0; i < table->entries; i++ ) {
			if (table->bucket[i].value != NULL) { n_entries++; }
		}

        memset (stats, sizeof stats, 0);
        stats->memory_size = table->memory_size;
        stats->entries_used = n_entries;
        stats->entries_available = table->entries;
        stats->conflicts = table->conflict_count;
        stats->max_retries = table->max_retry_count;
}
