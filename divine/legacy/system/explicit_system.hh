/*!\file
 * This file contains:
 * - constants SUCC_NORMAL, SUCC_ERROR and SUCC_DEADLOCK
 * - functions succs_normal(), succs_error(), succs_deadlock() ... for analyses
 *   of return values of explcit_system_t::get_succs() and similar methods
 * - definition of the class succ_container_t ... container of generated
 *   successors of states of the \sys
 * - definition of the abstract interface explicit_system_t ...
 *   the most important part of this unit
 */
#ifndef DIVINE_EXPLICIT_SYSTEM_HH
#define DIVINE_EXPLICIT_SYSTEM_HH

#ifndef DOXYGEN_PROCESSING
#include "system/system.hh"
#include "system/state.hh"
#include "system/system_trans.hh"
#include "system/expression.hh"
#include "common/types.hh"
#include "common/deb.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

class succ_container_t_;
typedef std::vector< state_t > succ_container_t;
class enabled_trans_container_t;
class system_trans_t;
class enabled_trans_t;

//CONSTANTS:
//!Constant that is a possible return value of get_succs() functions.
//! See \ref succs_results "Results of methods for creating successors"
//! for deatails
const int SUCC_NORMAL = 0;
//!Constant that is a possible return value of get_succs() functions.
//! See \ref succs_results "Results of methods for creating successors"
//! for deatails
const int SUCC_ERROR = 1;
//!Constant that is a possible return value of get_succs() functions.
//! See \ref succs_results "Results of methods for creating successors"
//! for deatails
const int SUCC_DEADLOCK = 2;

//INLINED FUNCTIONS:
//!Returns, whether get_succs() method has returned SUCC_NORMAL
/*!\param succs_result = return value of get_succs() method
 * \return true iff \a succs_results == SUCC_NORMAL
 *
 * See \ref succs_results "Results of methods for creating successors"
 * for details.
 */
inline bool succs_normal(const int succs_result)
{ return (succs_result == SUCC_NORMAL); }

//!Returns, whether return value of get_succs() comprises SUCC_ERROR
/*!\param succs_result = return value of get_succs() method
 * \return true iff \a succs_results bitwise contains SUCC_ERROR
 *                  (\a succs_results is a bitwise sum of
 *                   SUCC_NORMAL, SUCC_ERROR and SUCC_DEADLOCK)
 *                   
 * See \ref succs_results "Results of methods for creating successors"
 * for details.
 */
inline bool succs_error(const int succs_result)
{ return ((succs_result | SUCC_ERROR) == succs_result); }

//!Returns, whether return value of get_succs() comprises SUCC_DEADLOCK
/*!\param succs_result = return value of get_succs() method
 * \return true iff \a succs_results bitewise contains SUCC_DEADLOCK
 *                  (\a succs_results is a bitwise sum of
 *                   SUCC_NORMAL, SUCC_ERROR and SUCC_DEADLOCK)
 * 
 * See \ref succs_results "Results of methods for creating successors"
 * for details.
 */
inline bool succs_deadlock(const int succs_result)
{ return ((succs_result | SUCC_DEADLOCK) == succs_result); }

//!Abstract interface of a class represinting a state generator based
//! on the model of \sys stored in system_t
/*!The role of \exp_sys implemented by explicit_system_t is to provide
 * an interface to the generator of states of a model of the system
 * stored in the parent class system_t.
 *
 * The most important methods are get_initial_state() and
 * get_succs(). Values returned from get_succs() can be analyzed by
 * succs_normal(), succs_error() and succs_deadlock().
 */
class explicit_system_t: public virtual system_t
{
 //PUBLIC PART:
 public:
 
 //VIRTUAL INTERFACE METHODS:
 
 //!A constructor
 /*!\param evect = \evector used for reporting of error messages*/
 explicit_system_t(error_vector_t & evect):system_t(evect) { }
 
 //!A destructor
 virtual ~explicit_system_t() {};//!<A destructor.
 
 /*! @name Obligatory part of abstact interface
    These methods have to implemented in each implementation of
    explicit_system_t  */

 //!Returns, whether the state of the system is erroneous.
 /*!An erroneous state is a special state that is unique in the whole
  * system. It is reached by bad model specification (model that
  * permits variable/index overflow synchronization collision etc.)
  * 
  * \param state = state of the system
  * \return true iff any of processes is in the state `error'.
  */
 virtual bool is_erroneous(state_t state) = 0;
  
 //!Returns, whether the state is accepting in the specifed accepting group of
 //!Buchi or generalized Buchi automata, or whether the state belongs to the
 //!first or second component of the specified accepting pair of Rabin or Streett
 //!automata.
 /*!
  * \param state = state of the system \return whether \a state (i.e. its
  * property automaton projection) belongs to specified accepting group of Buchi
  * or generalized Buchi automaton, or whether it belongs to the first
  * (`pair_member=1') or second (`pair_member=2') component of the specified
  * accepting pair of Rabin and Streett automata.  \note If the system is
  * specified without property process, <code>false</code> is returned.  
  */
 virtual bool is_accepting(state_t state, size_int_t acc_group=0, size_int_t pair_member=1) = 0;
 
 //!Returns, whether any assertion is violated in the state `state'
 /*!If an implementation of explicit_system_t do not support assertions,
  * this method can be implemented so that it returns always \c false.*/
 virtual bool violates_assertion(const state_t state) const = 0;

 //!Returns a count of assertions violated in the state `state'
 /*!If an implementation of explicit_system_t do not support assertions,
  * this method can be implemented so that it returns always 0.*/
 virtual size_int_t violated_assertion_count(const state_t state) const = 0;
 
 //!Returns a string representation of index-th assertion violated in the
 //! state `state'
 /*!If an implementation of explicit_system_t do not support assertions,
  * this method can be implemented so that it returns always empty string.*/
 virtual std::string violated_assertion_string(const state_t state,
                                              const size_int_t index) const = 0;
 
 //!Returns a count of successors to preallocate in successor container
 /*!succ_container_t uses this method to estimate an amount of memory to
  * preallocate. Repeated reallocation is slow - therefore good estimation
  * is useful, but not necessary. */
 virtual size_int_t get_preallocation_count() const = 0;
 
 //!Prints the standard text representation of a state of the system
 virtual void print_state(state_t state, std::ostream & outs = std::cout) = 0;
 
 //!Returns an initial state of the system
 /*!This method takes a model of the \sys stored in system_t (parent of this
  * class) and computes the initial state of the system. */
 virtual state_t get_initial_state() = 0;

 //!Creates successors of state `state'
 /*!Creates successors of state \a state and saves them to successor container
  * (see succ_container_t).
  * 
  * \param state = state of the system
  * \param succs = successors container for storage of successors of \a state
  * \return bitwise OR of SUCC_NORMAL, SUCC_ERROR and SUCC_DEADLOCK (use
  *         functions succs_normal(), succs_error() and succs_deadlock() for
  *         testing)
  */
 virtual int get_succs(state_t state, succ_container_t & succs) = 0;

 //!Computes i-th successor of `state'
 /* Computes i-th successor of \a state
  * \param state = state of the system
  * \param i = the index of the enabled transition
  *            (0..get_async_enabled_trans_count)
  * \param succ = the variable for storing of successor
  * \return bitwise OR of SUCC_NORMAL, SUCC_ERROR and SUCC_DEADLOCK (use
  *         functions succs_normal(), succs_error() and succs_deadlock() for
  *         testing)
  */
 virtual int get_ith_succ(state_t state, const int i, state_t & succ) = 0;
 /*@}*/

 //!Tells whether the implementation of explicit_system_t interface can
 //! work with system_trans_t and enabled_trans_t
 bool can_system_transitions()
 { return get_abilities().explicit_system_can_system_transitions; }
 

 /*! @name Methods working with system transitions and enabled transitions
   These methods are implemented only if can_system_transitions() returns true
  @{*/
  
 //!Creates successors of state `state'
 /*!Creates successors of state \a state and saves them to successor container
  * (see succ_container_t). Furthermore it stores enabled transitions of the
  * systems which are used to generate successors of \a state
  * 
  * \param state = state of the system
  * \param succs = successors container for storage of successors of \a state
  * \param etc = container of enabled transitions (see enabled_trans_container_t
  *              and enabled_trans_t)
  * \return bitwise OR of SUCC_NORMAL, SUCC_ERROR and SUCC_DEADLOCK (use
  *         functions succs_normal(), succs_error() and succs_deadlock() for
  *         testing)
  *
  * This method is implemented only if can_system_transitions() returns true.
  */
 virtual int get_succs(state_t state, succ_container_t & succs,
               enabled_trans_container_t & etc) = 0;
 
 //!Creates a list of enabled transitions in a state `state'.
 /*!Creates a list of enabled transitions in a state \a state and stores it
  * to the container \a enb_trans (see enabled_trans_container_t).
  * \param state = state of the system
  * \param enb_trans = container for storing enabled transitions
  *                    (see \ref enb_trans "Enabled transitions" for details)
  * \return bitwise OR of SUCC_NORMAL, SUCC_ERROR and SUCC_DEADLOCK (use
  *         functions succs_normal(), succs_error() and succs_deadlock() for
  *         testing)
  *
  * This method is implemented only if can_system_transitions() returns true.
  */ 
 virtual int get_enabled_trans(const state_t state,
                               enabled_trans_container_t & enb_trans) = 0;
 
 //!Computes the count of transitions in a state `state'.
 /*!Computes the count of transitions in a state \a state.
  * \param state = state of the system
  * \param count = computed count of enabled transitions
  * \return bitwise OR of SUCC_NORMAL, SUCC_ERROR and SUCC_DEADLOCK (use
  *         functions succs_normal(), succs_error() and succs_deadlock() for
  *         testing)
  *
  * \note In fact the count of transitions in state `state' is equal to
  *       the number of successors of state `state'.
  *
  * This method is implemented only if can_system_transitions() returns true.
  */
 virtual int get_enabled_trans_count(const state_t state, size_int_t & count)
                                                                            = 0;
 
 //!Computes the i-th enabled transition in a state `state'.
 /*!Computes the i-th enabled transition in a state \a state.
  * \param state = state of the system
  * \param i = the index of the enabled transition
  *            (0..get_async_enabled_trans_count)
  * \param enb_trans = the variable for storing computed transition
  * \return bitwise OR of SUCC_NORMAL, SUCC_ERROR and SUCC_DEADLOCK (use
  *         functions succs_normal(), succs_error() and succs_deadlock() for
  *         testing)
  *
  * This method is implemented only if can_system_transitions() returns true.
  */
 virtual int get_enabled_ith_trans(const state_t state,
                                   const size_int_t i,
                                   enabled_trans_t & enb_trans) = 0;

 //!Generates a successor of `state' using enabled transition `enabled'
 /*!\param state = state of system
  * \param enabled = enabled transition to use for generation of successor
  * \param new_state = state to rewrite by the successor of \a state
  *
  * This method is implemented only if can_system_transitions() returns true.
  */
 virtual bool get_enabled_trans_succ
    (const state_t state, const enabled_trans_t & enabled,
     state_t & new_state) = 0;
 
 //!Generates successors of `state' using list of enabled transitions
 //! `enabled_trans'
 /*!\param state = state of system
  * \param succs = container for storage of successors of \a state
  * \param enabled_trans = list of enabled transitions to use for generation of
  *                        successors
  *
  * This method is implemented only if can_system_transitions() returns true.
  */
 virtual bool get_enabled_trans_succs
            (const state_t state, succ_container_t & succs,
             const enabled_trans_container_t & enabled_trans) = 0;

 //!Creates an instance of enabled transition
 /*!This method is needed by enabled_trans_container_t,
  * which cannot allocate enabled transitions itself, because it does not know
  * their concrete type. The abstract type enabled_trans_t is not sufficient
  * for creation because of its purely abstract methods.
  *
  * {\i Example - implementation of this method in DVE system:}
  *
  * \code
  * enabled_trans_t * dve_explicit_system_t::new_enabled_trans() const
  *  {
  *   return (new dve_enabled_trans_t);
  *  }
  * \endcode
  *
  * This method is implemented only if can_system_transitions() returns true.
  */
 virtual enabled_trans_t * new_enabled_trans() const = 0;

/*@}*/

 //!Tells whether the implementation of explicit_system_t interface can
 //! evaluate expressions
 bool can_evaluate_expressions()
 { return get_abilities().explicit_system_can_evaluate_expressions; }
 
/*! @name Methods for expression evaluation
  These methods are implemented only if can_evaluate_expressions() returns true
 @{*/

 //!Evaluates an expression
 /*!\param expr = expression to evaluate
  * \param state = state of the system, in which we want to evaluate
  *                the expression \a expr
  * \param data = computed value of the expression
  * \return false iff no error occured during the evaluation of the expression
  * 
  * This method is implemented only if can_evaluate_expressions() returns true.
  */
 virtual bool eval_expr(const expression_t * const expr,
                       const state_t state,
                       data_t & data) const = 0;
/*@}*/
 
}; //END of class explicit_system_t

//!Class determined to store successors of some state
/*!It is the descendant of array_t<state_t> and it differs only in a
 * constructor. Its constructor has a parameter of type explicit_system_t,
 * which is used to set the sufficient size of this constainer.
 * 
 * The main reason for this class is a better efficiency.*/
class succ_container_t_: public array_t<state_t>
{
 public:
 //!A constructor (only calls a constructor of array_t<state_t> with
 //! parameters 4096 (preallocation) and 16 (allocation step).
 succ_container_t_(): array_t<state_t>(4096, 16) {}
 //!A constructor (needs only `system' to guess the preallocation
 //! needed for lists of successors).
 succ_container_t_(const explicit_system_t & system):
   array_t<state_t>(system.get_preallocation_count(), 16) {}
 //!A destructor.
 ~succ_container_t_() { DEBFUNC(cerr << "Destructing succ_container_t" << endl;)}
};


#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif
