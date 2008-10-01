/*!\file
 * This file contains definitions of classes bymoc_system_trans_t and
 * bymoc_enabled_trans_t.*/
#ifndef DIVINE_BYMOC_SYSTEM_TRANS_HH
#define DIVINE_BYMOC_SYSTEM_TRANS_HH

#ifndef DOXYGEN_PROCESSING
#include "system/dve/dve_system_trans.hh"
#include "system/dve/dve_explicit_system.hh"
#include "system/dve/dve_transition.hh"
#include "system/system_trans.hh"
#include "common/array.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

class explicit_system_t;


//!Class implementing system trasition in BYMOC \sys
/*!BYMOC \sys does not support system transitions and enabled transitions.
 * This class is here only for abstract interface compatibility reasons.
 */
class bymoc_system_trans_t: virtual public system_trans_t
 {
  public:
   virtual ~bymoc_system_trans_t();
   //!Not implemented in BYMOC \sys - throws error message
   virtual system_trans_t & operator=(const system_trans_t & second);
   //!Not implemented in BYMOC \sys - throws error message
   virtual std::string to_string() const;
   //!Not implemented in BYMOC \sys - throws error message
   virtual void write(std::ostream & ostr) const;
   //!Not implemented in BYMOC \sys - throws error message
   virtual void set_count(const size_int_t new_count);
   //!Not implemented in BYMOC \sys - throws error message
   virtual size_int_t get_count() const;
   //!Not implemented in BYMOC \sys - throws error message
   virtual transition_t *& operator[](const int i);
   //!Not implemented in BYMOC \sys - throws error message
   virtual transition_t * const & operator[](const int i) const;
 };

//!Class implementing enabled trasition in BYMOC \sys
/*!BYMOC \sys does not support system transitions and enabled transitions.
 * This class is here only for abstract interface compatibility reasons.
 */
class bymoc_enabled_trans_t: public enabled_trans_t, public bymoc_system_trans_t 
{
 private:
 bool error;
 public:
 //// VIRTUAL INTERFACE OF ENABLED_TRANS_T ////
 virtual enabled_trans_t & operator=(const enabled_trans_t & second);
};


#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE

#endif //DOXYGEN_PROCESSING

#endif

