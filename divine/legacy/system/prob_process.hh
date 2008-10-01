/*!\file
 * The main contribution of this file is the abstact interface prob_process_t.
 */
#ifndef DIVINE_PROB_PROCESS_HH
#define DIVINE_PROB_PROCESS_HH

#ifndef DOXYGEN_PROCESSING

#include "system/system.hh"
#include "system/prob_system.hh"
#include "system/prob_transition.hh"

namespace divine { //We want Doxygen not to see namespace `divine'

#endif //DOXYGEN_PROCESSING

//predeclarations of classes:
class prob_system_t;
class prob_transition_t;
class system_t;
class transition_t;

//!Abstact interface of a class representing a probabilistic process of a
//! probabilistic system
/*!A "parent" probabilistic system is set in a constructor
 * prob_process_t(prob_system_t * const system) or using the method
 * set_parent_system().
 *
 * \note Developer is responsible
 * for correct setting of corresponding "parent" \sys (but he/she
 * rarely needs to create own processes - they are usually created
 * automatically during reading of a source of the system from a file)
 */
class prob_process_t: public virtual process_t
{
 protected:
 //!Protected data item storing a parent probabilistic system
 prob_system_t * parent_prob_system;

 public:
 //!A constructor
 prob_process_t():process_t(0),parent_prob_system(0) {}
 //!A constructor
 /*!\param system = "parent" \sys of this process*/
 prob_process_t(prob_system_t * const prob_system):process_t(prob_system) 
 { parent_prob_system=prob_system; }
 //!A destructor
 virtual ~prob_process_t() {}//!<A destructor

 //!Returns the "parent" probabilistic system of this process
 prob_system_t * get_parent_prob_system() const { return parent_prob_system; }
 
 //!Sets the "parent" probabilistic system of this process
 virtual void set_parent_prob_system(prob_system_t & system)
  { parent_prob_system = &system; }

 //!Returns a pointer to the constant probabilistic transition with
 //! \LID prob_trans_lid
 virtual const prob_transition_t * get_prob_transition
                                 (const size_int_t prob_trans_lid) const = 0;
 
 //!Returns a pointer to the probabilistic transition with \LID prob_trans_lid
 virtual prob_transition_t * get_prob_transition
                                    (const size_int_t prob_trans_lid) = 0;
 
 //!Returns a number of probabilistic transitions contained in a process
 virtual size_int_t get_prob_trans_count() const = 0;
 
 //!Adds the probabilistic transition `transition' to the process and
 //! sets its \LID
 virtual void add_prob_transition(prob_transition_t * const transition) = 0;

 //!Removes the probabilistic transition with \LID `prob_trans_lid' from
 //! the process
 virtual void remove_prob_transition(const size_int_t prob_trans_lid) = 0;
 
};


#ifndef DOXYGEN_PROCESSING  
} //END of namespace divine

#endif //DOXYGEN_PROCESSING

#endif




