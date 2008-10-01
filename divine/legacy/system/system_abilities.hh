/*!\file
 * The main contribution of this file is the definition of structure system_abilities_t.
 */
#ifndef DIVINE_SYSTEM_ABILITIES_HH
#define DIVINE_SYSTEM_ABILITIES_HH

#ifndef DOXYGEN_PROCESSING

#include <string>

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING


//!Structure storing abilities of \sys and its subordinate components
/*!An instance of this method is returned by system_t::get_abilities().
 * Values stored in data items differ according to methods which are
 * implemented in a given representation of a system (e. g. in a system
 * created from DVE source or a system created from Promela bytecode).
 *
 * This "system of abilities" has been created to preserve rich abstract
 * interfaces for a price of testing of their abilities - but without a
 * need of typecasting.
 */
struct system_abilities_t
{
 //Properties of an explicit system:
 //!= true iff \exp_sys can work with system transitions and enabled transitions
 bool explicit_system_can_system_transitions;
 //!= true iff \exp_sys can evaluate expressions
 bool explicit_system_can_evaluate_expressions;

 //Properties of a system:
 //!= true iff \sys can be modified
 bool system_can_be_modified;
 //!= true iff \sys can work with processes
 bool system_can_processes;
 //!= true iff \sys can work with property process
 bool system_can_property_process;
 //!= true iff \sys can decompose property process graph
 bool system_can_decompose_property;
 //!= true iff \sys can work with transitions
 bool system_can_transitions;

 //Properties of processes:
 //!= true iff process can be modified
 bool process_can_be_modified;
 //!= true iff process can be read from a string representation
 bool process_can_read;
 //!= true iff process can work (and contains) transitions
 bool process_can_transitions;
 
 //Properties of transitions:
 //!= true iff transition can be modified
 bool transition_can_be_modified;
 //!= true iff transition can be read from a string representation
 bool transition_can_read;
 
 //!A constructor
 /*!Sets all abilities to false (\sys is defaultly as dumb as possible)*/
 system_abilities_t()
  {
   //defautly system is as dumb as possible:
   explicit_system_can_system_transitions = false;
   explicit_system_can_evaluate_expressions = false;
   
   system_can_be_modified        = false;
   system_can_processes          = false;
   system_can_property_process   = false;
   system_can_decompose_property = false;
   system_can_transitions        = false;

   process_can_be_modified = false;
   process_can_read        = false;
   process_can_transitions = false;

   transition_can_be_modified = false;
   transition_can_read        = false;
  }
 
};


#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif


