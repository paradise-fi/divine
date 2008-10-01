/*!\file
 * The main contribution of this file is the class dve_transition_t.
 */
#ifndef DIVINE_DVE_TRANSITION_HH
#define DIVINE_DVE_TRANSITION_HH

#ifndef DOXYGEN_PROCESSING
#include <iostream>
#include <string>
#include <sstream>
#include "system/transition.hh"
#include "common/error.hh"
#include "system/dve/dve_expression.hh"
#include "system/dve/dve_source_position.hh"
#include "common/array.hh"
#include "common/bit_string.hh"
#include "common/deb.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

class dve_parser_t;
class dve_symbol_table_t;

//it's good to have assigned integer values to the enum type values
//anyway it's used for giving semantics to the fields of transitions
//!Synchronization mode of a transition
enum sync_mode_t { SYNC_NO_SYNC = 0, //!<transition is not synchronized
                   SYNC_EXCLAIM = 1, //!<transition is synchronized and sends a value to another process
                   SYNC_ASK = 2,     //!<transition is synchronized and receives a value from another process
                   SYNC_EXCLAIM_BUFFER = 3, //!<transition is asynchronous and a message is stored into a buffer
                   SYNC_ASK_BUFFER = 4     //!<transition is asynchronous and a message is taken from a buffer
                 };

//!Default allocation size of array_t used in an implementation of
//! dve_transition_t - quite internal thing
const size_int_t TR_effects_default_alloc = 10;
//!Allocation step of array_t used in an implementation of dve_transition_t
//! - quite internal thing
const size_int_t TR_effects_alloc_step = 20;

//!Class representing a transition
/*!This class implements the abstract interface transition_t.
 *
 * Notice, there is the set of methods, which are not virtual and they
 * are already present in the class transition_t (e. g.
 * transition_t::get_gid(), transition_t::get_lid(), ...) and there
 * are also virtual methods with default implementation. They are
 * not changed in dve_transition_t (e. g. transition_t::set_gid(),
 * transition_t::set_lid(), ...).
 * 
 * It supports the full set of methods of an abstract interface and
 * adds many DVE specific methods to this class, corresponding to
 * the DVE point of view to transitions of processes.
 *
 * Very DVE system specific feature:
 * This class also contains a global variables mask that tells which global
 * variables are used in this transition. This mask is maintained by \sys
 * during its creation or consolidation. Functions for access to this
 * mask are get_glob_mask(), set_glob_mask() and alloc_glob_mask().
 * 
 * There is a list of
 * all transitions in dve_system_t (use functions system_t::get_trans_count()
 * and system_t::get_transition()) and also each process has a list of his
 * own transitions (functions process_t::get_trans_count() and
 * process_t::get_transition()
 */
class dve_transition_t: public dve_source_position_t, public virtual transition_t
{
 private:
 static dve_parser_t trans_parser;

 protected:
 typedef array_t<dve_expression_t *> effects_container_t; 
 bool valid; //says, whether a transtition is valid or not - invalid transitions
             // are ignored
 size_int_t state1_gid; //GID od the first state in transition
 size_int_t state1_lid; //ID of the first state in transition
 size_int_t state2_gid; //GID of the second state in transition
 size_int_t state2_lid; //ID of the second state in transition
 size_int_t channel_gid; //GID of channel to synchronize through
 dve_expression_t * guard; //expression representing guard of transition
                       //- it can be 0
 array_t<dve_expression_t *> sync_exprs; //expressions behind '?' od '!' in synchonisation
                           //- it can be empty
 sync_mode_t sync_mode; //mode of synchronisation of a trantition
 effects_container_t effects; //list of expressions representing effects

 /* mask of assigned global variables */
 bit_string_t glob_mask;
 
 size_int_t process_gid; //identifier of parent process
 
 /* various identifiers of transition*/
 size_int_t part_id;
 
 public:
 /* Constructors and destructor */
 //!A constructor (no system set - call set_parent_system() after creation)
 dve_transition_t():
   transition_t(),
   valid(false),
   sync_exprs(1,10),
   effects(TR_effects_default_alloc,TR_effects_alloc_step)
   { guard = 0; }
 //!A constructor (transition has to be created in a context of a \sys
 //! - especially its \sym_table given in a parameter `system')
 dve_transition_t(system_t * const system):
   transition_t(system),
   valid(false),
   sync_exprs(1,10),
   effects(TR_effects_default_alloc,TR_effects_alloc_step)
   { guard = 0; }
 
 //!A destructor.
 virtual ~dve_transition_t();
 
 /*! @name Obligatory part of abstact interface */
 //!Implements transition_t::to_string() in DVE \sys
 virtual std::string to_string() const;
 //!Implements transition_t::write() in DVE \sys
 virtual void write(std::ostream & ostr) const;
 
 /*! @name Methods for reading a transition from a string representation
   These methods are implemented only if can_read() returns true.
  @{*/
 //!Implements transition_t::from_string() in DVE \sys
 virtual int from_string(std::string & trans_str,
                 const size_int_t process_gid = NO_ID);
 //!Implements transition_t::read() in DVE \sys
 virtual int read(std::istream & istr, size_int_t process_gid = NO_ID);
 /*@}*/
 
 
 
 /*! @name DVE system specific methods
     These methods are implemented only in DVE system and they
     cannot be run using an abstract interface of transition_t.
  */
 
 /* methods for getting/setting informations about transition */
 //!Returns a symbol table corresponding to this transition
 /*!In this symbol table there are stored declarations of variables states,
  * processes and channels, which are used in this transition.*/
 dve_symbol_table_t * get_symbol_table() const;
 //!Returns, whether this transition is valid or not.
 /*!Returns, whether this transition is valid or not. When transition is not
  * valid, it is not used, when we generate the states of system*/
 bool get_valid() const { return valid; }
 //!Sets the validity of the transition (`true' means valid)
 void set_valid(const bool valid_value) { valid = valid_value; }
 //!Returns a \GID of first (starting) state of a transition
 size_int_t get_state1_gid() const { return state1_gid; }
 //!Sets a \GID of first (starting) state of a transition
 void set_state1_gid(const size_int_t state_gid);
 //!Returns a \GID of a second (finishing) state of a transition
 size_int_t get_state2_gid() const { return state2_gid; }
 //!Sets a \GID of a second (finishing) state of a transition
 void set_state2_gid(const size_int_t state_gid);
 //!Returns a \LID of first (starting) state of a transition
 size_int_t get_state1_lid() const { return state1_lid; }
 //!Returns a \LID of a second (finishing) state of a transition
 size_int_t get_state2_lid() const { return state2_lid; }
 //!Returns a \GID of channel used in a synchronisation of this transition
 /*!Returns a \GID of channel used in a synchronisation of this transition.
  * Is has no reasonable meaning, when get_sync_mode()==SYNC_NO_SYNC */
 size_int_t get_channel_gid() const { return channel_gid; }
 //!Sets a \GID of channel used in a synchronisation of this transition
 void set_channel_gid(const size_int_t gid_of_channel)
  { channel_gid = gid_of_channel; }
 //!Returns a guard of this transition
 /*!In case when there is no guard get_guard() returns 0.*/
 dve_expression_t * get_guard() { return guard; }
 //!Returns a pointer to the constant expression representing a guard of this
 //! transition
 /*!In case when there is no guard get_guard() returns 0.*/
 const dve_expression_t * get_guard() const { return guard; }
 //!Sets a guard of this transition
 void set_guard(dve_expression_t * const guard_expr) { guard = guard_expr; }
 //!Returns a count of expressions written after '!' or '?'
 size_int_t get_sync_expr_list_size() const { return sync_exprs.size(); }
 //!Sets a count of expression written after '!' or '?'
 void set_sync_expr_list_size(const size_int_t size) {sync_exprs.resize(size);}
 //!Returns the `i'-th expression in a synchronization written after '!' or '?'
 /*!Returns the `i'-th expression in a synchronization of this transition.
  * In case when there is no synchronization * get_sync_expr() returns 0.*/
 dve_expression_t * get_sync_expr_list_item(const size_int_t i)
 { return (sync_exprs.size() ? sync_exprs[i] : 0); }
 //!Returns the `i'-th constant expression in a synchronization written after
 //! '!' or '?'
 /*!Returns the `i'-th constant expression in a synchronization of this
  * transition. In case when there is no synchronization * get_sync_expr()
  * returns 0.*/
 const dve_expression_t * get_sync_expr_list_item(const size_int_t i) const
 { return (sync_exprs.size() ? sync_exprs[i] : 0); }
 //!Sets the `i'-th expression in a synchronization written after '!' of '?'
 void set_sync_expr_list_item(const size_int_t i,
                              dve_expression_t * const synchro_expr)
 { sync_exprs[i] = synchro_expr; }
 //!Returns a synchronization mode of this transition
 sync_mode_t get_sync_mode() const { return sync_mode; }
 //!Sets a sync_mode mode of this transition
 void set_sync_mode(const sync_mode_t synchro_mode) { sync_mode=synchro_mode; }
 //!Returns true iff transition contains no synchronisation
 bool is_without_sync() const { return (sync_mode==SYNC_NO_SYNC); }
 //!Returns true iff transition contains synchronisation of type `exclaim' 
 bool is_sync_exclaim() const { return (sync_mode==SYNC_EXCLAIM); }
 //!Returns true iff transition contains synchronisation of type `question'
 bool is_sync_ask() const { return (sync_mode==SYNC_ASK); }
 //!Returns an <code>eff_nbr<code>-ith effect.
 /*! \param eff_nbr = number of effect (effects have numbers from <code>0
  *                   </code>to
  *                   <code>get_effect_count()-1</code>)
  */
 dve_expression_t * get_effect(size_int_t eff_nbr) const
   { return effects[eff_nbr];}
 //!Adds an effect to the list of effects contained in a transition
 void add_effect(dve_expression_t * const effect)
   { effects.push_back(effect); }
 //!Returns a count of effects in this transition
 size_int_t get_effect_count() const { return effects.size(); }
 //!Returns a bit mask of assigned variables
 /*!Returns an object of type bit_string_t, which represents the
  * bit mask of assigned variables in effects of this transition
  */
 const bit_string_t & get_glob_mask() const { return glob_mask; }
 //!Turns bit i-th bit in a global variables mask to `mark'
 void set_glob_mark(const size_int_t i, const bool mark)
  { if (mark) glob_mask.enable_bit(i); else glob_mask.disable_bit(i); }
 //!Prepare bitmask of global variables - allocate as many bits as a count
 //! of global variables
 void alloc_glob_mask(const size_int_t count_of_glob_vars)
  { glob_mask.alloc_mem(count_of_glob_vars); }
 //!Returns a \proc_GID of process owning this trantision.
 size_int_t get_process_gid() const { return process_gid; }
 //!Sets a \proc_GID of process owning this trantision.
 /*!\warning Use this method carefuly. Do not try to cause inconsistencies */
 void set_process_gid(const size_int_t gid) { process_gid = gid; }
 //!Returns an index into vector of transitions of process with
 //! the same `sync_mode'
 /*!Together with \a sync_mode forms an unique identifier of
  * transition inside the process. To gain a single identifier use
  * a function get_id() */
 size_int_t get_partial_id() const { return part_id; }
 //!Sets \part_ID of this transition
 /*!\warning \part_ID should be set only by process_t, when trantition is added
  * to the list of process's transitions*/
 void set_partial_id(const size_int_t partial_id) { part_id = partial_id; }
 
 
 /* methods for getting string representation transition */
 //!Returns the name of the first state
 const char * get_state1_name() const;
 //!Returns the name of the second state
 const char * get_state2_name() const;
 //!Sets the string represetation of guard or returns false
 /*!\param str = variable to which to return the string repr. of guard
  * \returns true iff transition has a guard
  */
 bool get_guard_string(std::string & str) const;
 //!Returns the name of channel or 0 (see details).
 /*!If the transition is synchronised it returns the name of
  * channel used for this synchronisation. Otherwise it returns 0.*/
 const char * get_sync_channel_name() const;
 //!Sets the string represetation of expression in a synchronization or
 //! returns false
 /*!\param str = variable to which to return the string repr. of expression
  *              from synchronisation
  * \returns true iff transition has an expression in a synchronisation
  */
 bool get_sync_expr_string(std::string & str) const;
 //!Returns the string representation of i-th effect in the argument \a str.
 void get_effect_expr_string(size_int_t i, std::string & str) const;
 /*@}*/
};

#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif


