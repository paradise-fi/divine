#include "system/process.hh"
#include "common/deb.hh"

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif //DOXYGEN_PROCESSING

error_vector_t *process_t::pproc_terr = &gerr;

using std::string;

bool process_t::can_transitions() const
 {
  if (parent_system)
    return parent_system->get_abilities().process_can_transitions;
  else return false;
 }

bool process_t::can_be_modified() const
 {
  if (parent_system)
    return parent_system->get_abilities().process_can_be_modified;
  else return false;
 }
 
bool process_t::can_read() const
 {
  if (parent_system)
    return parent_system->get_abilities().process_can_read;
  else return false;
 }

