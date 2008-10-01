/*!\file
 * The main contribution of this file is the class dve_process_t.
 */
#ifndef DIVINE_DVE_PROCESS_HH
#define DIVINE_DVE_PROCESS_HH

#ifndef DOXYGEN_PROCESSING
#include <iostream>
#include <string>
#include <sstream>
#include "system/process.hh"
#include "common/error.hh"
#include "common/array.hh"
#include "system/dve/dve_transition.hh"
#include "common/deb.hh"


namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

DEB(using namespace std;)

class dve_symbol_table_t;
class dve_transition_t;
class dve_expression_t;
class dve_parser_t;

//!Alloctaion steps for array_t used in an implementation of dve_process_t
//! - quite internal thing
const size_int_t DVE_PROCESS_ALLOC_STEP = 20;

//!Class representing a process
/*!This class implements the abstract interface process_t
 *
 * Notice, there are methods, which are not virtual and they
 * are already present in the class process_t (e. g. process_t::get_gid()) and
 * there are also virtual methods with default virtual implementation.
 * They are not changed in dve_transition_t (e. g. process_t::set_gid()).
 *
 * It supports the full set of methods of an abstract interface and
 * adds many DVE specific methods to this class, corresponding to
 * the DVE point of view to transitions of processes.
 */
class dve_process_t: public virtual process_t
 {
  private:
  static dve_parser_t proc_parser;   
  
  //Informations necessary for computing succesor:
  array_t<dve_transition_t *> transitions;
  array_t<dve_transition_t *> trans_by_sync[5];
  size_int_t initial_state;
  property_type_t accepting_type;
  size_int_t accepting_groups;
  array_t<bool> *accepting1;
  array_t<bool> *accepting2;
  array_t<bool> commited;
  
  //Another informations:
  array_t<size_int_t> var_gids;
  array_t<size_int_t> state_gids;
  bool valid;
  
  //Assertions:
  array_t< array_t<dve_expression_t *> * > assertions;
  
  void intitialize_dve_specific_parts();
  
  protected:
  void write_declarations(std::ostream & ostr) const;
  int read_using_given_parser(std::istream & istr, dve_parser_t & parser);
  
  public:
  ///// IMPLEMENTATION OF VIRTUAL INTERFACE /////
  //!A constructor (initializates the process_t part and DVE specific parts)
  dve_process_t();
  //!A constructor (initializates the process_t part and DVE specific parts)
  dve_process_t(system_t * const system);
  //!A destructor
  virtual ~dve_process_t();
 /*! @name Obligatory part of abstact interface */
 //!Implements process_t::to_string() in DVE \sys
  virtual std::string to_string() const;
  //!Implements process_t::write() in DVE \sys
  virtual void write(std::ostream & ostr) const;
 /*@}*/
  
 /*! @name Methods working with transitions of a process
   These methods are implemented and can_transitions() returns true.
  @{*/
  //!Implements process_t::get_transition(const size_int_t lid) in
  //! DVE \sys
  virtual transition_t * get_transition(const size_int_t lid)
    { return transitions[lid]; }
  //!Implements process_t::get_transition(const size_int_t lid) const in
  //! DVE \sys
  virtual const transition_t * get_transition(const size_int_t lid) const
    { return transitions[lid]; }
  //!Implements process_t::get_trans_count() in DVE \sys
  virtual size_int_t get_trans_count() const { return transitions.size(); }
  /*@}*/

  
 /*!  @name Methods modifying a process
   These methods are implemented only if can_transitions() returns true.
  @{*/
  //!Implements process_t::add_transition() in DVE \sys, but see also
  //! implementation specific notes below
  /*!\warn This method modifies the added
   * transition because it has to set \trans_LID and \part_ID.
   */
  virtual void add_transition(transition_t * const transition);
  //!Implements process_t::remove_transition() in DVE \sys
  virtual void remove_transition(const size_int_t transition_lid)
  { transitions[transition_lid]->set_valid(false); }
 /*@}*/
 

 /*!
   @name Methods for reading a process from a string representation
   These methods are implemented and can_read() returns true.
  @{*/
  //!Implements process_t::from_string() in DVE \sys
  virtual int from_string(std::string & proc_str);
  //!Implements process_t::read() in DVE \sys
  virtual int read(std::istream & istr);
 /*@}*/

  ///// DVE SPECIFIC METHODS: /////
 /*! @name DVE system specific methods
     These methods are implemented only in DVE system and they
     cannot be run using an abstract interface of process_t.
  */

  //!Returns true iff process is valid
  bool get_valid() const { return valid; }

  //!Returns a pointer to the transition given by the synchronization and
  //! \part_ID.

  /*!Returns a pointer to the transition given by parameters:
   * \param sync_mode = synchronization mode of the transition
   *                    (SYNC_NO_SYNC, SYNC_ASK or SYNC_EXCLAIM)
   * \param trans_nbr = ID of the transition in this process */
  dve_transition_t * get_transition(const sync_mode_t sync_mode,
                                        const size_int_t trans_nbr)
    { return trans_by_sync[sync_mode][trans_nbr]; }
  //!Returns a pointer to the selected constant transition.
  //! For further informations see get_transition(const sync_mode_t sync_mode,
  //! const size_int_t trans_nbr) above.
  const dve_transition_t * get_transition(const sync_mode_t sync_mode,
                                             const size_int_t trans_nbr) const
    { return trans_by_sync[sync_mode][trans_nbr]; }
  //!Returns a count of transitions of given synchronization mode
  /*!Returns a count of transitions of synchronization mode given by
   * parameter \a sync_mode
   *
   * \part_ID then can be from 0 to
   * (get_trans_count(sync_mode) - 1)
   */
  size_int_t get_trans_count(const sync_mode_t sync_mode) const
    { return trans_by_sync[sync_mode].size(); }
  
  //!Returns a count of local variables in this process
  size_int_t get_variable_count() const { return var_gids.size(); }
  //!Returns a \GID of variable with \LID `lid'
  size_int_t get_variable_gid(size_int_t lid) const { return var_gids[lid]; } 
  //!Returns a count of process states in this process
  size_int_t get_state_count() const { return state_gids.size(); }
  //!Returns a \GID of process state with \LID `lid'
  size_int_t get_state_gid(size_int_t lid) const { return state_gids[lid]; } 
  //!Returns a \LID of initial state of this process
  size_int_t get_initial_state() const { return initial_state; }
  
  //!Sets validity of the process (`true' means valid)
  void set_valid(const bool is_valid) { valid = is_valid; }
  //!Sets, which process state is initial.
  /*!Sets, which process state is initial.
   * \param state_lid = \LID of initial state.
   */
  void set_initial_state(const size_int_t state_lid)
    { initial_state = state_lid; }
  
  //!Returns, whether state with \LID = `lid' in accepting group `group' and pair
  //!member `pair_member' is s accepting or not.
  bool get_acceptance(size_int_t lid, size_int_t group=0, size_int_t pair_member=1) const
   { 
     if (pair_member==1)
       {
	 return (lid<=(accepting1[group].size()) && (accepting1[group][lid]));
       }
     else
       {
	 return (lid<=(accepting2[group].size()) && (accepting2[group][lid]));
       }
   }

   //!Sets, whether state with \LID = `lid' in accepting group `group' and pair
   //!member `pair_member' is s accepting or not.
  void set_acceptance(size_int_t lid, bool is_accepting, size_int_t group=0, size_int_t pair_member=1)
   { 
     DEB(cerr <<"Set acceptance:"<<lid<<"("<<is_accepting<<") group:"<<group<<" pair:"<<pair_member<<endl;)     
     if (pair_member==1)
       {
	 (accepting1[group])[lid] = is_accepting;
       }
     else
       {
	 (accepting2[group])[lid] = is_accepting;
       }       
   }
     
   //!Sets accepting type and number of accepting groups
  void set_acceptance_type_and_groups(property_type_t prop_type,size_int_t groups_count) 
   {
     DEB(cerr<<"Acc type:"<< prop_type<<" with "<<groups_count<<" groups."<<endl;);
     accepting_type = prop_type;
     accepting_groups = groups_count;
     accepting1= new array_t<bool>[groups_count];
     accepting2= new array_t<bool>[groups_count];
     /*complementing acceptance lists by default values (i. e. false)*/
     for (size_int_t i=0; i!=accepting_groups; ++i)
      {
       accepting1[i].resize(state_gids.size());
       accepting2[i].resize(state_gids.size());
       for (size_int_t j=0; j!=state_gids.size(); ++j)
        {
         accepting1[i][j] = false;
         accepting2[i][j] = false;
        }
      }
   }

  //!Returns number of accepting groups. Accepting groups correspond to the
  //!number of sets in accepting condition of generalized Buchi automaton or
  //!number of pairs in accepting conditions of Rabin and Streett automata.
  size_int_t get_accepting_group_count() const { return accepting_groups; }

  //!Returns property accepting type.
  property_type_t get_accepting_type() const { return accepting_type; }

  //!Returns, whether state with \LID = `lid' is commited or not
  bool get_commited(size_int_t lid) const
   { return ((lid<=commited.size()) && commited[lid]); }
  //!Sets, whether state with \LID = `lid' is commited or not
  void set_commited(size_int_t lid, bool is_commited)
   { commited[lid] = is_commited; }
  
  //!This function should not be used if the process is the part of the \sys
  //! Otherwise is may cause inconsistencies in a \sys.
  void add_variable(const size_int_t var_gid);
  //!This function should not be used if the process is the part of the \sys
  //! Otherwise is may cause inconsistencies in a \sys.
  void add_state(const size_int_t state_gid);
  
  //!Returns a count of assertions associated with state with \LID `state_lid'
  size_int_t get_assertion_count(const size_int_t state_lid) const
  { return assertions[state_lid]->size(); }
  
  //!Returns a pointer to expression representing assertion
  /*!\param state_lid = \LID of state, which the assertion is associated with
   * \param index     = number from 0 to get_assertion_count(state_lid)-1
   */
  dve_expression_t * get_assertion(const size_int_t state_lid,
                                   const size_int_t index)
  { return (*assertions[state_lid])[index]; }
  
  //!Returns a constant pointer to expression representing assertion
  /*!\param state_lid = \LID of state, which the assertion is associated with
   * \param index     = number from 0 to get_assertion_count(state_lid)-1
   */
  const dve_expression_t * get_assertion(const size_int_t state_lid,
                                         const size_int_t index) const
  { return (*assertions[state_lid])[index]; }
  
  //!Associates state with \LID `state_lid' with assertion `assert_expr'
  /*!Several assertions can be associated with the same state*/
  void add_assertion(size_int_t state_lid,dve_expression_t * const assert_expr);

  
  //!Returns a \sym_table of the \sys, where this process is contained
  dve_symbol_table_t * get_symbol_table() const;
 /*@}*/
 };


#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif


