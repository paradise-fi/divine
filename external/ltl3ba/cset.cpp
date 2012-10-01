/***** ltl3ba : set.cc *****/

/* Written by Tomas Babiak, FI MU, Brno, Czech Republic                   */
/* Copyright (c) 2012  Tomas Babiak                                       */
/*                                                                        */

#include "ltl3ba.h"
#include <assert.h>

cset& cset::operator=(const cset &r) {
  if (type == r.type) {
    copy_set(r.s, s, type);
  } else {
    if (s) tfree(s); 
    type = r.type;
    s = dup_set(r.s, type);
  }
  return (*this);
}

std::ostream &operator<<(std::ostream &out, const cset &r) {
  return (print_set_out(out, r.s, r.type));
}

void cset::merge(const cset &l, const cset &r) {
  assert(l.type == r.type && this->type == l.type);
  do_merge_sets(s, l.s, r.s, type);
}

void cset::substract(const cset &r) {
  assert(this->type == 0 && r.type == 0);
  substract_set(s, s, r.s);
}

void cset::substract(int *r) {
  assert(this->type == 0);
  substract_set(s, s, r);
}
