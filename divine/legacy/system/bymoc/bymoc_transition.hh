/*!\file
 * The main contribution of this file is the class bymoc_transition_t.
 */
#ifndef DIVINE_BYMOC_TRANSITION_HH
#define DIVINE_BYMOC_TRANSITION_HH

#ifndef DOXYGEN_PROCESSING
#include <iostream>
#include <string>
#include <sstream>
#include "system/transition.hh"
#include "common/error.hh"
#include "system/bymoc/bymoc_expression.hh"
#include "common/array.hh"
#include "common/bit_string.hh"
#include "common/deb.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

//!Class representing a transition in BYMOC \sys
/*!BYMOC \sys does not support transitions. This class is here only
 * for abstract interface compatibility reasons.
 */
class bymoc_transition_t: public transition_t
{
 public:
 /* Constructors and destructor */
 bymoc_transition_t(): transition_t() {}
 bymoc_transition_t(system_t * const system): transition_t(system) {}
 //!Not implemented in BYMOC \sys - throws error message
 virtual ~bymoc_transition_t();
 //!Not implemented in BYMOC \sys - throws error message
 virtual std::string to_string() const;
 //!Not implemented in BYMOC \sys - throws error message
 virtual int from_string(std::string & trans_str,
                 const size_int_t process_gid = NO_ID);
 //!Not implemented in BYMOC \sys - throws error message
 virtual void write(std::ostream & ostr) const;
 //!Not implemented in BYMOC \sys - throws error message
 virtual int read(std::istream & istr, size_int_t process_gid = NO_ID);
};

#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif


