#ifndef COIN_SEQ_HASHTABLE_H
#define COIN_SEQ_HASHTABLE_H

#include <divine/coin/state.h>
#include <cstring>
#include <iostream>

/* A very simple hashtable. */

namespace coin_seq {

struct hashtable_t {
  static const int HASH_SIZE = 64 * 1024;

  int mc; // = 0;

  struct hash_elem_t {
    state_t *ps;
    hash_elem_t *next;
  };

  hash_elem_t *hash_table[HASH_SIZE];

  hashtable_t () {
    for (int i = 0; i < HASH_SIZE; i++)
      hash_table[i] = NULL;
    mc = 0;
  }

  ~hashtable_t () {
    for (int i = 0; i < HASH_SIZE; i++)
      if (hash_table[i]) {
	hash_elem_t * he = hash_table[i];
	while (he) {
	  hash_elem_t * oldhe = he;
	  he = he->next;
	  delete oldhe->ps;
	  delete oldhe;
	}
      }
  }

  state_t * get_if_stored(const state_t & s) {
    hash64_t hash = s.hash();
    hash_elem_t *he = hash_table[hash % HASH_SIZE];
    state_t *  ret = NULL;

    while (he && !ret) {
      if (*(he->ps) == s)
	ret = (he->ps);

      he = he-> next;
    }

    return ret;
  }

  state_t * get_if_stored_or_store(const state_t & s) {
    hash64_t hash = s.hash();
    hash_elem_t *he = hash_table[hash % HASH_SIZE], *last_he = NULL;
    state_t * ret = NULL;

    int this_mc = 0;

    while (he && !ret) {
      if (*(he->ps) == s)
	ret = (he->ps);

      last_he = he;
      he = he-> next;
      this_mc++;
    }

    if (this_mc > mc) mc = this_mc;

    if (!ret) {
      he = new hash_elem_t;
      char * tmpdata = new char[s.size];
      memcpy(tmpdata, s.data, s.size);
      he->ps = new state_t(tmpdata, s.size);
      he->next = NULL;
//      *he->ps = s;
      if (last_he) {
	last_he->next = he;
      } else {
	hash_table[hash % HASH_SIZE] = he;
      }
      ret = he->ps;
    }
      
    return ret;
    
  }

  friend std::ostream& operator<< (std::ostream & os, const hashtable_t & ht);

};
  
std::ostream& operator<< (std::ostream & os, const hashtable_t & ht) {
/*  for (int i = 0; i < hashtable_t::HASH_SIZE; i++) {
    if (ht.hash_table[i]) {
      os << "[" << i << "]";
      hashtable_t::hash_elem_t *  he = ht.hash_table[i];
      while (he) {
	os << " -> " << *(he->ps) << std::endl;
	he = he->next;
      }
    }
  }*/
  os << "max collisions: " << ht.mc;
  return os;
}

}

#endif
