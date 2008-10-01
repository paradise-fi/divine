#include "system/transition.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif //DOXYGEN_PROCESSING

error_vector_t *transition_t::ptrans_terr = &gerr;

bool transition_t::can_be_modified() const
 {
  if (parent_system)
    return parent_system->get_abilities().transition_can_be_modified;
  else return false;
 }

bool transition_t::can_read() const
 { if (parent_system) return parent_system->get_abilities().transition_can_read;
   else return false; }

