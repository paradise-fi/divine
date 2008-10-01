/*!\file
 * This file contains:
 * - definition of the abstract interface prob_explicit_system_t
 *   the most important part of this unit
 */
#ifndef DIVINE_PROB_EXPLICIT_SYSTEM_HH
#define DIVINE_PROB_EXPLICIT_SYSTEM_HH

#ifndef DOXYGEN_PROCESSING
#include "system/explicit_system.hh"
#include "system/prob_system.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING


class prob_succ_container_t;

//!Abstract interface of a class representing a state generator based
//! on the model of \sys stored in prob_system_t
/*!This class works like explicit_system_t, but in addition to explicit_system_t
 * it also provides methods:
 *  - get_succs(state_t state, prob_succ_container_t & succs, enabled_trans_container_t & etc)
 *  - get_succs(state_t state, prob_succ_container_t & succs), which 
 *
 * These methods return successors with their weight and \GID of used
 * probabilistic transition.  All these informations are also possible to find
 * out using information about enabled transitions (parameter \a etc of
 * get_succs()) and methods prob_system_t::get_prob_trans_of_trans(),
 * get_index_of_trans_in_prob_trans() and interface of prob_transition_t, but
 * parameter \a etc is voluntary and thus  get_succs(state_t state,
 * prob_succ_container_t & succs) provides a minimal interface to do a
 * probabilistic state space generation.
 */
class prob_explicit_system_t: public virtual explicit_system_t,
                              public virtual prob_system_t
{
 //PUBLIC PART:
 public:
 
 //VIRTUAL INTERFACE METHODS:
 
 //!A constructor
 /*!\param evect = \evector used for reporting of error messages*/
 prob_explicit_system_t(error_vector_t & evect):explicit_system_t(evect),
                                                prob_system_t(evect) { }
  
 //!Creates probabilistic successors of state `state'
 /*!Creates probabilistic successors of state \a state and saves them to
  * successor container \a succs (see prob_succ_container_t).
  * 
  * \param state = state of the system
  * \param succs = successors container for storage of successors of \a state
  * \return bitwise OR of SUCC_NORMAL, SUCC_ERROR and SUCC_DEADLOCK (use
  *         functions succs_normal(), succs_error() and succs_deadlock() for
  *         testing)
  */
 virtual int get_succs(state_t state, prob_succ_container_t & succs) = 0;
 
 //!Creates probabilistic successors of state `state'
 /*!Creates probabilistic successors of state \a state. In addition to
  * get_succs(state_t state, prob_succ_container_t & succs) this method
  * also creates a piece of information about enabled transitions used
  * for successor generation.
  *
  * Together with methods prob_system_t::get_prob_trans_of_trans(),
  * prob_system_t::get_index_of_trans_in_prob_trans() and methods of
  * prob_transition_t it is possible to extract all additional information (and
  * even more) that is stored in prob_succ_container_t.
  */
 virtual int get_succs(state_t state, prob_succ_container_t & succs, enabled_trans_container_t & etc) = 0;

}; //END of class explicit_system_t


//!Duple of transitions GIDs denoting probabilistic transition of the system
//! multipied with transitions of the property process
struct prob_and_property_trans_t {
  //!A default constructor (set "uninitialized" values to both items)
  prob_and_property_trans_t():
    prob_trans_gid(NO_ID), property_trans_gid(NO_ID) {}\
  //!A constructor
  prob_and_property_trans_t(const size_int_t init_prob_trans_gid,
                            const size_int_t init_property_trans_gid):
    prob_trans_gid(init_prob_trans_gid),
    property_trans_gid(init_property_trans_gid) {}
  size_int_t prob_trans_gid; //!<\GID of probabilistic transition
  size_int_t property_trans_gid; //!<\GID of transition of the property process
  //!An operator of equality
  bool operator==(const prob_and_property_trans_t & second)
  {
   return (prob_trans_gid==second.prob_trans_gid &&
           property_trans_gid==second.property_trans_gid);
  }
  //!An operator of inequality
  bool operator!=(const prob_and_property_trans_t & second)
  {
   return (prob_trans_gid!=second.prob_trans_gid ||
           property_trans_gid!=second.property_trans_gid);
  }
};

//!Single element to store in prob_succ_container_t
/*!The structure consists of a state (successor in a state generation),
 * a weight of a transition and a used probabilistic transition.
 */
struct prob_succ_element_t
{
  prob_succ_element_t():weight(0), sum(0) { }
  prob_succ_element_t(state_t init_state, const ulong_int_t init_weight,
                  const ulong_int_t init_sum,
                  const prob_and_property_trans_t init_prob_and_property_trans):
    state(init_state), weight(init_weight), sum(init_sum),
    prob_and_property_trans(init_prob_and_property_trans) { }
  state_t state; //!<A state (successor)
  ulong_int_t weight; //!<Weight of a transition used for generation of a successor
  ulong_int_t sum; //!<Sum of weights of transitions contained in  probabilistic transition with \GID `prob_and_property_trans.prob_trans_gid'
  prob_and_property_trans_t prob_and_property_trans; //!<\GID of probabilistic transition and \GID of property transition used for generation of a successor
};


//!Class determined to store probabilistic successors of some state
/*!It is the descendant of array_t<prob_succ_element_t> and it differs only in a
 * constructor. Its constructor has a parameter of type explicit_system_t,
 * which is used to set the sufficient size of this constainer.
 * 
 * The main reason for this class is a better efficiency.*/
class prob_succ_container_t: public array_t<prob_succ_element_t>
{
 public:
 //!A constructor (only calls a constructor of array_t<state_t> with
 //! parameters 4096 (preallocation) and 16 (allocation step).
 prob_succ_container_t(): array_t<prob_succ_element_t>(4096, 16) {}
 //!A constructor (needs only `system' to guess the preallocation
 //! needed for lists of successors).
 prob_succ_container_t(const explicit_system_t & system):
   array_t<prob_succ_element_t>(system.get_preallocation_count(), 16) {}
 //!A destructor.
 ~prob_succ_container_t() {}
};


#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif

