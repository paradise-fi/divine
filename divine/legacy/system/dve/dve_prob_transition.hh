/*!\file
 * The main contribution of this file is the class dve_prob_transition_t.
 */
#ifndef DIVINE_DVE_PROB_TRANSITION_HH
#define DIVINE_DVE_PROB_TRANSITION_HH

#ifndef DOXYGEN_PROCESSING
#include <iostream>
#include <string>
#include <sstream>
#include "system/prob_transition.hh"
#include "system/prob_system.hh"
#include "system/dve/syntax_analysis/dve_parser.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

class dve_parser_t;
class prob_system_t;


//!Class representing a DVE transition in probabilistic system
/*!This class implements the abstract interface prob_transition_t.
 */
class dve_prob_transition_t: public prob_transition_t
{
 private:
 static dve_parser_t prob_trans_parser;
 
 protected:
 bool valid;

 public:
 //!A constructor
 dve_prob_transition_t(): transition_t(), prob_transition_t() {}
 //!A constructor
 dve_prob_transition_t(prob_system_t * const system): transition_t(system), prob_transition_t(system) {}
 
 //!Sets validity of the transition (`true' means valid)
 void set_valid(const bool is_valid) { valid = is_valid; }

 //Returns true iff transition is valid
 bool get_valid() const { return valid; }
 
 //!Implements transition_t::to_string() in probabilistic DVE system.
 virtual std::string to_string() const;
 //!Implements transition_t::write() in probabilistic DVE system.
 virtual void write(std::ostream & ostr) const;
 //!Implements transition_t::read() in probabilistic DVE system.
 virtual int read(std::istream & istr, size_int_t process_gid = NO_ID);
 //!Implements transition_t::from_string() in probabilistic DVE system.
 virtual int from_string(std::string & trans_str,
                         const size_int_t process_gid = NO_ID);
};

#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif


