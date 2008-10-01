/*!\file
 * The main contribution of this file is the class bymoc_process_t.
 */
#ifndef DIVINE_BYMOC_PROCESS_HH
#define DIVINE_BYMOC_PROCESS_HH

#ifndef DOXYGEN_PROCESSING
#include <iostream>
#include <string>
#include <sstream>
#include "system/process.hh"
#include "common/error.hh"
#include "common/array.hh"
#include "system/bymoc/bymoc_transition.hh"
#include "common/deb.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

class bymoc_transition_t;
class bymoc_expression_t;

//!Class representing a process in BYMOC \sys
/*!BYMOC \sys does not support processes. This class is here only
 * for abstract interface compatibility reasons.
 */
class bymoc_process_t: public process_t
 {
  public:
  ///// IMPLEMENTATION OF VIRTUAL INTERFACE /////
  bymoc_process_t();
  bymoc_process_t(system_t * const system);
  //!Not implemented in BYMOC \sys - throws error message
  virtual ~bymoc_process_t();
  //!Not implemented in BYMOC \sys - throws error message
  virtual transition_t * get_transition(const size_int_t id);
  //!Not implemented in BYMOC \sys - throws error message
  virtual const transition_t * get_transition(const size_int_t id) const;
  //!Not implemented in BYMOC \sys - throws error message
  virtual size_int_t get_trans_count() const;
  virtual void add_transition(transition_t * const transition);
  //!Not implemented in BYMOC \sys - throws error message
  virtual void remove_transition(const size_int_t transition_gid);
  //!Not implemented in BYMOC \sys - throws error message
  virtual std::string to_string() const;
  //!Not implemented in BYMOC \sys - throws error message
  virtual int from_string(std::string & proc_str);
  //!Not implemented in BYMOC \sys - throws error message
  virtual void write(std::ostream & ostr) const;
  //!Not implemented in BYMOC \sys - throws error message
  virtual int read(std::istream & istr);
 };


#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif



