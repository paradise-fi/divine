/*!\file
 * The main contribution of this file is the class dve_explicit_system_t
 */
#ifndef DIVINE_DVE_EXPLICIT_SYSTEM_HH
#define DIVINE_DVE_EXPLICIT_SYSTEM_HH

#define POOLED 1

#ifndef DOXYGEN_PROCESSING
#include "system/explicit_system.hh"
#include "system/dve/syntax_analysis/dve_symbol_table.hh"
#include "system/dve/dve_system.hh"
#include "system/state.hh"
#include "system/dve/dve_system_trans.hh"
#include "common/types.hh"
#include "common/deb.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

class dve_process_decomposition_t;

//!Print names of states instead of state IDs
const size_int_t ES_FMT_PRINT_STATE_NAMES = 1;
//!Print names of \vars before printing of their values
const size_int_t ES_FMT_PRINT_VAR_NAMES = 2;
//!Print names of processes before printing of its state and \vars
const size_int_t ES_FMT_PRINT_PROCESS_NAMES = 4;
//!Print `new-line' character after each process*/
const size_int_t ES_FMT_DIVIDE_PROCESSES_BY_CR = 8;
//!Print all the things given by ES_FMT_PRINT_STATE_NAMES,
//! ES_FMT_PRINT_VAR_NAMES, ES_FMT_PRINT_PROCESS_NAMES and
//! ES_FMT_DIVIDE_PROCESSES_BY_CR
const size_int_t ES_FMT_PRINT_ALL_NAMES = ES_FMT_PRINT_VAR_NAMES |
                                           ES_FMT_PRINT_STATE_NAMES |
                                           ES_FMT_PRINT_PROCESS_NAMES |
                                           ES_FMT_DIVIDE_PROCESSES_BY_CR;

/*!\typedef ushort_int_t dve_state_int_t
 * dve_state_int_t is an interger type that is used to store state ID */
typedef ushort_int_t dve_state_int_t;

//!Structure determined for passing parameters to ES_*_eval() functions
/*!dve_explicit_system_t defines its own structure for passing parameters
 * to its own functions ES_*_eval(), which are replacing SYS_*_eval()
 * fuctions used by system_t.
 *
 * This is really internal thing and the programmer of algorithms using
 * DiVinE usually should not use this structure. 
 */
struct ES_parameters_t
{
 state_t state;
 size_int_t * state_positions_var;
 size_int_t * state_positions_state;
 size_int_t * state_positions_proc;
 SYS_initial_values_t * initial_values;
 size_int_t * initial_states;
 size_int_t * state_lids;
 dve_var_type_t * var_types;
 size_int_t * array_sizes;
};

//!Pointer to ES_parameters_t
typedef ES_parameters_t * ES_p_parameters_t;

class succ_container_t_;
class enabled_trans_container_t;
class dve_system_trans_t;
class dve_enabled_trans_t;

//!Class in DVE system interpretation
/*!This class implements the abstract interface explicit_system_t.
 *
 * It is a child of dve_system_t - thus it also contains the representation
 * of DVE \sys.
 *
 * DVE system interpretation in this case comprises state generation,
 * enabled transitions generation and expression evaluation.
 * 
 * This implementation of the abstract interface implements full set of
 * its methods. Furthermore
 * It takes over the system of expression evaluation from system_t. Only
 * for evaluating varibles, fields and state identifiers there are defined
 * special functions, which return their value accoring a state of system
 * (given by a piece of a memory).
 *
 * Furthermore it provides the set of methods, which are purely DVE system
 * specific.
 */
class dve_explicit_system_t : public virtual explicit_system_t,
                              public virtual dve_system_t
{
public:

 //PRIVATE PART:
 private:

 process_decomposition_t *property_decomposition;


 void process_variable(const size_int_t gid);
 void process_channel(const size_int_t gid);
 void another_read_stuff(bool do_expr_compaction = true);
 state_t create_state_by_composition
     (state_t initial_state, enabled_trans_t * const p_enabled,
      succ_container_t & succs, size_int_t * trans_indexes,
      size_int_t * bounds);
 bool not_in_glob_conflict(size_int_t * trans_indexes,
                           size_int_t * bounds);  
 
 //PUBLIC PART:
 public:
 
 ///// VIRTUAL INTERFACE METHODS: /////
 
 //!A constructor
 /*!\param evect = \evector used for reporting of error messages*/
 dve_explicit_system_t(error_vector_t & evect = gerr);
 //!A destructor
 virtual ~dve_explicit_system_t();
 
 /*! @name Obligatory part of abstact interface
    These methods have to implemented in each implementation of
    explicit_system_t  */

 //!Implements explicit_system_t::is_erroneous() in DVE \sys
 virtual bool is_erroneous(state_t state)
 {
  return (processes[0]->get_state_count() ==
            (state_pos_to<dve_state_int_t>(state,
               state_positions_proc[processes[0]->get_gid()])));
 }
   
 //!Implements explicit_system_t::is_accepting() in DVE \sys. Returns true if
 //!the property automaton state belongs to the by-parameters-specified set of
 //!accepting condition. The set of accepting condition is specified using
 //!accepting group id and possibly pair member. Accepting !groups correspond to
 //!individual sets of states, or pairs of sets of states given by the
 //!accepting condition of the property automaton. Grups are identified by the
 //!numbers including zero (0,..,n-1) while pair members using numbers 1 and 2.
 /*! For example let Streett's accepting condition be (L1,U1), (L2,U2), (L3,U3).
  *  To check whether state q is present in U2, is_accepting(q,1,2) should be
  *  used. */
 virtual bool is_accepting(state_t state, size_int_t acc_group=0, size_int_t pair_member=1)
  {     
   return (get_with_property() && 
     pproperty->get_acceptance(state_pos_to<dve_state_int_t>(state, property_position),
			       acc_group, pair_member));   
 }
 
 //!Implements explicit_system_t::violates_assertion() in DVE \sys
 virtual bool violates_assertion(const state_t state) const;

 //!Implements explicit_system_t::violated_assertion_count() in DVE \sys
 virtual size_int_t violated_assertion_count(const state_t state) const;
 
 //!Implements explicit_system_t::violated_assertion_string() in DVE \sys
 virtual std::string violated_assertion_string(const state_t state,
                                               const size_int_t index) const;

 //!Implements explicit_system_t::get_preallocation_count() in DVE \sys
 size_int_t get_preallocation_count() const { return max_succ_count; }
 
 //!Implements explicit_system_t::print_state() in DVE \sys
 virtual void print_state(state_t state, std::ostream & outs = std::cout)
 {
  DBG_print_state(state,outs);
 }
 
 //!Implements explicit_system_t::get_initial_state() in DVE \sys
 virtual state_t get_initial_state();

 //!Implements explicit_system_t::get_succs(state_t state,
 //! succ_container_t & succs) in DVE \sys
 virtual int get_succs
   (state_t state, succ_container_t & succs)
  {
   if (system_synchro==SYSTEM_ASYNC)
     return get_async_succs(state, succs);
   else
     return get_sync_succs(state, succs);
  } 
 
 //!Implements explicit_system_t::get_ith_succ() in DVE \sys, but see also
 //! implementation specific notes below
 /*!\warning The running time of this function is relatively high.  The running
  * time is the running time of get_succs(). It is usually more efficient to use
  * get_succs() and to store all generated states, if it is possible.
  */
 virtual int get_ith_succ(state_t state, const int i, state_t & succ);
 /*@}*/

  /*!@name Methods to check the SCCs of property process graph
   @{*/

   //! Returns property decomposition
   process_decomposition_t *get_property_decomposition()
   {
       return property_decomposition;
   }
   /*@}*/

    
 ///// DVE EXPLICIT SYSTEM CAN WORK WITH SYSTEM TRANSITITONS /////
 /*! @name Methods working with system transitions and enabled transitions
    These methods are implemented and can_system_transitions() returns true
  @{*/
 
 //!Implements explicit_system_t::get_succs(state_t state,
 //! succ_container_t & succs, enabled_trans_container_t & etc) in DVE \sys
 virtual int get_succs(state_t state, succ_container_t & succs,
               enabled_trans_container_t & etc)
  {
   if (system_synchro==SYSTEM_ASYNC)
     return get_async_succs(state, succs, etc);
   else
     return get_sync_succs(state, succs, etc);
  }
 
 //!Implements explicit_system_t::get_enabled_trans() in DVE \sys
 virtual int get_enabled_trans(const state_t state,
                       enabled_trans_container_t & enb_trans)
  {
   if (system_synchro==SYSTEM_ASYNC)
     return get_async_enabled_trans(state,enb_trans);
   else
     return get_sync_enabled_trans(state,enb_trans);
  }
 
 //!Implements explicit_system_t::get_enabled_trans_count() in DVE \sys
 /*!The running time of this method is the same as the running time
  * of get_enabled_trans(). */
 virtual int get_enabled_trans_count(const state_t state, size_int_t & count);
 
 //!Implements explicit_system_t::get_enabled_ith_trans() in DVE \sys,
 //! but see also implementation specific notes below
 /*!\warning Computing of this method is the same as the running time
  * of get_enabled_trans() (it is more efficient to use get_enabled_trans()
  * and store generated enabled transitions, if it is possible).*/
 virtual int get_enabled_ith_trans(const state_t state,
                                 const size_int_t i,
                                 enabled_trans_t & enb_trans);

 //!Implements explicit_system_t::get_enabled_trans_succ() in DVE \sys
 virtual bool get_enabled_trans_succ
   (const state_t state, const enabled_trans_t & enabled,
    state_t & new_state)
 {
   if (system_synchro==SYSTEM_ASYNC)
     return get_async_enabled_trans_succ(state, enabled, new_state);
   else
     return get_sync_enabled_trans_succ(state, enabled, new_state);
 }
 
 //!Implements explicit_system_t::get_enabled_trans_succs() in DVE \sys
 virtual bool get_enabled_trans_succs
   (const state_t state, succ_container_t & succs,
    const enabled_trans_container_t & enabled_trans)
 {
   if (system_synchro==SYSTEM_ASYNC)
     return get_async_enabled_trans_succs(state, succs, enabled_trans);
   else
     return get_sync_enabled_trans_succs(state, succs, enabled_trans);
 }
 
 //!Implements explicit_system_t::new_enabled_trans() in DVE \sys
 virtual enabled_trans_t * new_enabled_trans() const;
 
 /*@}*/

 ///// DVE EXPLICIT SYSTEM CAN EVALUATE EXPRESSIONS /////
 /*! @name Methods for expression evaluation
   These methods are implemented and can_evaluate_expressions() returns true
  @{*/
 //!Implements explicit_system_t::eval_expr() in DVE \sys
 virtual bool eval_expr(const expression_t * const expr,
                        const state_t state,
                        data_t & data) const
 {
  bool eval_err;
  data.assign<all_values_t>(eval_expr(dynamic_cast<const dve_expression_t*>(expr),
                            state, eval_err));
  return eval_err;
 }
 /*@}*/
 
 
 ////DVE SPECIFIC METHODS:
 /*!@name DVE system specific methods
    These methods are implemented only in DVE system and they
    cannot be run using an abstract interface of system_t.
  @{*/
 
 //debugging functions espesially for printing contents of states
 //!Prints a state to output stream `outs' formated according to `format'
 /*!This is a function meant espesially for debugging purposes.
  * \param state = state of the system to print
  * \param outs = output stream where you want to print the state
  * \param format = format of output - you can set how the output should
  *      be structured and which names of identifiers should appear in
  *      the printout of the state (bitwise sum of the constants
  *      \ref ES_FMT_PRINT_STATE_NAMES,
  *      \ref ES_FMT_PRINT_VAR_NAMES,
  *      \ref ES_FMT_PRINT_PROCESS_NAMES,
  *      \ref ES_FMT_DIVIDE_PROCESSES_BY_CR).
  */
 void DBG_print_state(state_t state, std::ostream & outs = std::cerr,
                      const ulong_int_t format = ES_FMT_PRINT_ALL_NAMES);
 
 //!It is the same as DBG_print_state(), but finally calls `outs << std::endl'
 //! (thus prints `new-line' character and flushes the buffer).
 void DBG_print_state_CR(state_t state, std::ostream & outs = std::cerr,
                         const ulong_int_t format = ES_FMT_PRINT_ALL_NAMES)
 { DBG_print_state(state,outs,format); outs << std::endl; }

 
 //!Returns, whether process `process_id' is in a accepting process state
 /*! \param state = state of the system
  *  \param process_id = \GID of process, which we are asking for an acceptance
  *  \return whether a process with \GID \a process_ID is in an accepting
  *  state or not
  */
 bool is_accepting(state_t state, size_int_t process_id)
 {
  return processes[process_id]->get_acceptance(
                      state_pos_to<dve_state_int_t>(state,
                      state_positions_proc[processes[process_id]->get_gid()]));
 }
 
 //!Reads in a DVE source given by `filename'
 /*!Is uses system_t::read(const char * const filename), therefore see
  * that function for more information (about return value etc.) Furthermore,
  * it makes some analysis and extraction of information from the \sys
  * and \sym_table
  */
 virtual slong_int_t read(const char * const filename)
 { return read(filename,true); }

 slong_int_t read( std::istream & is )
 {
     int result = dve_system_t::read( is );
     if (result==0) another_read_stuff();
     return result;
 }

  //!Read in a DVE source given by `filename` and avoids expression compaction if `do_comp` is false
  /*!Is uses system_t::read(const char * const filename), therefore see
  * that function for more information (about return value etc.) Furthermore
  * it makes some analysis and extraction of information from the \sys
  * and \sym_table. Morover if `do_comp' is false avoids expression compaction
  */  
  virtual slong_int_t read(const char * const filename, bool do_comp);

 //!Returns a number of bytes necessary to store a state of the system
 size_int_t get_space_sum() const;
 
 //!Evaluates the expression in the context of the state of \exp_sys
 /*!\param expr = pointer to the expression represented by dve_expression_t
  * \param state = state of the system
  * \param eval_err = if there was an error during an evaluation of an
  *                   expression, \a eval_err is set to true.
  *                   Otherwise it is <b>unchanged</b>.
  * \return The value of the expression \a expr in the context of \a state
  */
 all_values_t eval_expr(const dve_expression_t * const expr,
                                         const state_t state,
                                         bool & eval_err) const
  { ES_p_parameters_t(parameters)->state = state;
  if (expr->is_compacted())
    {      
      //DEBFUNC(cerr<<"Eval: "<<expr->to_string()<<endl;);
      return dve_system_t::fast_eval(expr->get_p_compact(),eval_err);
    }
  else
    return dve_system_t::eval_expr(expr,eval_err); 
  }
 
 //!Returns the value of the \var1 in a given state of the system.
 all_values_t get_var_value
   (const state_t state, size_int_t var_gid, const size_int_t index=0)
 {
   return (
           constants[var_gid]?
             (
	       (initial_values_counts[var_gid]!=MAX_INIT_VALUES_SIZE)?
	       initial_values[var_gid].all_value:
	       initial_values[var_gid].all_values[index]
	     ):
	     get_state_creator_value_of_var_type(state,var_gid,
	                                         var_types[var_gid],index)
	  );
 }
 
 //!Changes the value of the \var1 in a given state of the system.
 /*!\param state ... system state to which the variable value should be set
    \param var_gid ... \GID of variable to set
    \param v ... value to set (be aware of bounds of byte and int types)
    \param index ... optional parameter for the case of arrays - it is the
                     index to an array
    \return true iff setting value was not successsful - i. e. value breaks
            the bounds of the type of variable, or variable is the constant
  */
 bool set_var_value(state_t state, const size_int_t var_gid,
    const all_values_t v, const size_int_t index=0)
 {
   return constants[var_gid] ||
          set_state_creator_value(state,var_gid,v,index);
 }
 
 //!Returns a \state_LID of state of a given process in a
 //! given state of the system.
 size_int_t get_state_of_process(const state_t state, size_int_t process_id) const;
 
 //!Returns a normal or a sending transition contained in enabled transition
 /*!Transition of DVE system can consist of at most 3 transitions
  * of processes. It always contains either non-synchronized transition
  * or a sending transition (and its receiving counterpart)
  *
  * This method guaranties to always return the transition, if \a sys_trans
  * contains any (which should be truth, if enabled transition has been
  * created by this class).
  *
  * \warning This method presumes, that \a sys_trans has been created by
  * this class.
  */
 dve_transition_t * get_sending_or_normal_trans(const system_trans_t & sys_trans)
                                                                         const;
 
 //!Returns a receiving transition contained in enabled transition
 /*!If get_sending_or_normal_trans(sys_trans) returns sending transition,
  * this method returns its receiving counterpart.
  *
  * Otherwise it returns 0.
  *
  * \warning This method presumes, that \a sys_trans has been created by
  * this class.
  */
 dve_transition_t * get_receiving_trans(const system_trans_t & sys_trans) const;
 
 //!Returns a transition of property process contained in enabled transition
 /*!If the system contains a property process, this method returns
  * the pointer to the property process's transition contained in
  * an enabled transition.
  *
  * Otherwise it returns 0.
  * 
  * \warning This method presumes, that \a sys_trans has been created by
  * this class.
  */
 dve_transition_t * get_property_trans(const system_trans_t & sys_trans) const;
 /*@}*/
 
 //!Returns a position of variable in a state of the system with \proc_GID `gid'
 size_int_t get_var_pos(const size_int_t gid) const 
 { return state_positions_var[gid]; }

 //!Returns a position of channel in a state of the system with \proc_GID `gid'
 size_int_t get_channel_pos(const size_int_t gid) const 
 { return state_positions_channel[gid]; }
 
 //!Returns a position of process in a state of the system with \proc_GID `gid'
 size_int_t get_process_pos(const size_int_t gid) const 
 { return process_positions[gid].begin; }

 //!Returns a size of process in a state of the system with \proc_GID `gid'
 size_int_t get_process_size(const size_int_t gid) const 
 { return process_positions[gid].size; }
 

 //PROTECTED PART:
 protected:
 //TYPES:
 struct proc_and_trans_t
 { size_int_t proc; size_int_t trans;
   proc_and_trans_t() {}
   proc_and_trans_t(size_int_t p, size_int_t t): proc(p), trans(t) {}
 };
 
 struct channels_t
 {
  size_int_t count;
  bool error; //says, that computing on asking transitions was errorneous
  proc_and_trans_t * list;
  
  channels_t(const size_int_t alloc) { allocate(alloc); }
  channels_t() { list = 0; }
  ~channels_t() { DEBAUX(std::cerr << "BEGIN of destructor of channels_t" << std::endl;) if (list) delete [] list; DEBAUX(std::cerr << "END of destructor of channels_t" << std::endl;)}
  void set_error(const bool new_error) { error = new_error; }
  bool get_error() { return error; }
  void allocate(const size_int_t alloc) { list = new proc_and_trans_t[alloc]; }
  void push_back(const proc_and_trans_t & pat) { list[count] = pat; ++count; }
 };

 struct process_position_t
 {
  size_int_t begin;
  size_int_t size;
 };

 //!Internal structure used to store inforamtions about non-constant \vars,
 //! process states and channel buffers.
 /*!Programmer of algorithms using DiVinE usually should not use this
  * structure.*/
 struct state_creator_t
 {
  enum state_creator_type_t { VARIABLE, PROCESS_STATE, CHANNEL_BUFFER };
  state_creator_type_t type; //stores a kind of state creator (variable, ...)
  dve_var_type_t var_type; //int,byte,...
  size_int_t array_size; //size of an array, when the state creator is an array
                          //otherwise array_size = 0
  size_int_t gid;
  state_creator_t
    (const state_creator_type_t sc_type,
     dve_var_type_t vtype, const size_int_t arr_size,
     const size_int_t gid_arg):
     type(sc_type), var_type(vtype), array_size(arr_size), gid(gid_arg) {}
 };
 
 struct prop_trans_t
 {
  dve_transition_t * trans;
  bool error;
  void assign(dve_transition_t * const transition, const bool erroneous)
  { trans = transition; error = erroneous; }
 };
 
 //DATA:
 array_t<byte_t *> glob_filters; //list of glob_filters for all transitions
 size_int_t first_in_succs;
 //proc_info_t * proc_infos;
 size_int_t trans_count;
 //size_int_t enabled_trans_count;
 size_int_t max_succ_count;
 enabled_trans_container_t * p_enabled_trans;
 enabled_trans_container_t * aux_enabled_trans;
 enabled_trans_container_t * aux_enabled_trans2;
 succ_container_t * aux_succ_container;
 
 channels_t * channels;
 array_t<prop_trans_t> property_trans;
 size_int_t * state_positions_var;
 size_int_t * state_positions_state;
 size_int_t * state_positions_proc;
 size_int_t * state_positions_channel;//positions of channels in states
 size_int_t * channel_buffer_size;     //sizes of buffers of channels
 size_int_t * channel_element_size;   //sizes of elements for each buffer
 dve_var_type_t ** channel_item_type; //types of items of given channels
 size_int_t ** channel_item_pos;      //offset of item of element in a channel
 size_int_t * state_sizes_var;
 size_int_t * array_sizes;
 process_position_t * process_positions;
 process_position_t global_position;
 size_int_t process_count;
 size_int_t space_sum;
 //GIDs of symbols creating the states (non-constant variables and procnames)
 //(procnames reprezent space for state of process)
 std::vector<state_creator_t> state_creators; //used in get_initial_state()
 size_int_t state_creators_count;
 size_int_t property_position;
 size_int_t prop_begin;
 size_int_t prop_size;
 size_int_t glob_count;
 
 
 ////METHODS:
 //!Changes the state of process (repr. by `process') to `error'
 /* Changes the piece of memory in \a state so, that error 
  * \param state = state of the system
  * \param process = pointer on constant structure representring the process
  */
 void go_to_error(state_t state);
 
 //!Returs true iff transitions `t1' and `t2' assign to the same global
 //! \vars.
 bool not_in_glob_conflict(const dve_transition_t * const t1,
                           const dve_transition_t * const t2)
 { return (t1->get_glob_mask() & t2->get_glob_mask()); }
 
 //!Creates a successor of `state' by applying effects of the transition `trans'
 /* Creates a successor of system state \a state by applying effects of
  * the transition \a trans idependently on satisfying conditions of guard,
  * synchonity etc.
  * There is not created any new state of the \sys, but the state `state' is
  * modified. */
 bool apply_transition_effects
   (const state_t state, const dve_transition_t * const trans);

 //!Creates a successor of `state' by applying effect `effect'
 /* Creates a successor of the state of the \sys \a state by applying
  * effect \a effect on it.
  * There is not created any new system state, but state \a state is
  * modified.
  *
  * You have to ensure that `effect' is an expression that is really
  * an effect - that means there is an assignment operator in a root of
  * syntax tree of such an expression. */
 bool apply_effect
   (const state_t state, const dve_expression_t * const effect);


 //!Implementation of get_succs() in asynchronous DVE systems
 int get_async_succs(const state_t state, succ_container_t & succs)
 { p_enabled_trans = aux_enabled_trans;
   return get_async_succs_internal(state,succs); }
 
 //!Implementation of get_succs() in synchronous DVE systems
 int get_sync_succs(state_t state, succ_container_t & succs)
 { return get_sync_succs_internal(state, succs, aux_enabled_trans2); }
 
 //!Implementation of get_succs() in asynchronous DVE systems
 int get_async_succs(const state_t state, succ_container_t & succs,
                     enabled_trans_container_t & enb_trans)
 { p_enabled_trans = &enb_trans; return get_async_succs_internal(state,succs); }
 
 //!Implementation of get_succs() in synchronous DVE systems
 int get_sync_succs(state_t state, succ_container_t & succs,
                    enabled_trans_container_t & etc)
 { return get_sync_succs_internal(state, succs, &etc); }
 
 
 //!Implementation of get_enabled_trans() in asynchronous DVE systems
 int get_async_enabled_trans(const state_t state,
                            enabled_trans_container_t & enb_trans);
 
 //!!Implementation of get_enabled_trans() in synchronous DVE systems
 int get_sync_enabled_trans(const state_t state,
                            enabled_trans_container_t & enb_trans);
 
 //!Implementation of get_enabled_trans_succ() in asynchronous DVE systems
 bool get_async_enabled_trans_succ
   (const state_t state, const enabled_trans_t & enabled,
    state_t & new_state);
 
 //!Implementation of get_enabled_trans_succ() in synchronous DVE systems
 bool get_sync_enabled_trans_succ
   (const state_t state, const enabled_trans_t & enabled,
    state_t & new_state);
 
 //!Creates a successor of `state' using the enabled transition `enabled'
 //! and a successor `property_state' gained by the transition of property
 /*!This function optimizes a generation of successors of one state,
  * because it does not compute transitions of property process any more.
  * It uses precomputed state of property process.*/
 bool get_async_enabled_trans_succ
   (const state_t state, const enabled_trans_t & enabled,
    state_t & new_state, const state_t property_state);
 
 //!Implementation of get_enabled_trans_succs() in asynchronous DVE systems
 bool get_async_enabled_trans_succs
   (const state_t state, succ_container_t & succs,
    const enabled_trans_container_t & enabled_trans);
 
 
 //!Implementation of get_enabled_trans_succs() in synchronous DVE systems
 bool get_sync_enabled_trans_succs
   (const state_t state, succ_container_t & succs,
    const enabled_trans_container_t & enabled_trans);
 
 //!Creates a successor of `state' using the enabled transition `enabled'
 //! and doesn't use the property component of the enabled transition.
 bool get_async_enabled_trans_succ_without_property
   (const state_t state, const enabled_trans_t & enabled, state_t & new_state);
 

 //!Creates "erroneous state"
 /*!Creates "erroneous state", which is the unique in whole system.
  * This state should not be interpreted (you would get unexpected
  * values of \vars, which you want to see).
  *
  * You can detect error states by function is_erroneous().
  */
 state_t create_error_state();
 
 //!Computes successors of `state' using transitions of process with \GID
 //! `process_number' without any regard to synchronization
 bool compute_successors_without_sync
   (const size_int_t process_number,
    succ_container_t & succs, const state_t state);
 
 //!Appends an new enabled transition given by sending and receiving transition
 //! and erroneousness.
 /*!It also combines sending and receiving transitions with property transitions
  * stored in private variable property_trans, if there are any. Thus it
  * stores 1 enabled transition for a system without property process and
  * n enabled transition for a system with property process with n executable
  * transitions stored in property_trans.
  */
 void append_new_enabled(dve_transition_t * const t_answ,
                         dve_transition_t * const t_ask,
                         const bool trans_err);

 //!Appends a new enabled transition given by sending and receiving property
 // transition and erroneousness.
 /*!\param t_answ = sending transition
  * \param t_prop = receiving transition of the property process
  * \param trans_err = says, whether an evaluation of some of guards of
                       transitions was erroneous
  */
 void append_new_enabled_prop_sync(dve_transition_t * const t_answ,
                                   dve_transition_t * const t_prop,
                                   const bool trans_err);
 
 //!Searching for receiving transitions with satisfied guards (stores them to
 //! `channels' - transition sorted by the used channel)
 bool compute_enabled_stage1
   (const size_int_t process_number,
    channels_t * channels, const state_t state, const bool only_commited);
    
 //!Searching for sending transitions and trasitions without synchronization
 //! with satisfied guards.
 /*!It combines them with transitions from `channels'
  * created by compute_enabled_stage1(). It creates a list of enabled
  * transisitions in p_enabled_trans.
  */
 bool compute_enabled_stage2
   (const size_int_t process_number,
    channels_t * channels, const state_t state, const bool only_commited);
 
 //!Computes trasitions of property process with satisfied guards and
 //! stores them to the list property_trans
 bool compute_enabled_of_property(const state_t state);
 
 //!Returns a value of a variable with \GID `var_gid' in a state `state'
 /*!\param state = state of system
  * \param var_gid = \GID of variable
  * \param var_type = type of variable
  * \param index = index for a case of vector of variables
  * \return a value of variable with \GID \a var_gid in a state \a state.
  *         It uses \a var_type to determine the type of conversion
  *         and \a index (voluntary) for determining an pointer to the
  *         variable in a vector
  */
 all_values_t get_state_creator_value_of_var_type
   (const state_t state, size_int_t var_gid, const dve_var_type_t var_type,
    const size_int_t index=0);
 
 //!Sets a value of a variable with \GID `var_gid' in a state `state'
 /*!\param state = state of system
  * \param var_gid = \GID of variable
  * \param var_type = type of variable
  * \param index = index for a case of vector of variables
  * \param v = value to assign
  * \return true iff no error occured during a call (i. e. value is in
  * bounds of its type)
  */
 bool set_state_creator_value_of_var_type(state_t state,
    const size_int_t var_gid, const dve_var_type_t var_type, const all_values_t v,
    const size_int_t index);

 //!set_state_creator_value_of_var_type() with `var_type` set to
 //! `var_types[var_gid]`
 bool set_state_creator_value(state_t state, const size_int_t var_gid,
    const all_values_t v, const size_int_t index=0)
 { return set_state_creator_value_of_var_type(state,var_gid,var_types[var_gid],
                                              v,index); }
 //!Sets a value `val' to the variable given by `to_assign'
 /*!\param state = original state
  * \param new_state = new state, where the value of variable should be assigned
  * \param to_assign = expression representing a variable of indexed element
  *                    of array (index is evaluated in a constext of \a state)
  * \param val = value to assign
  * \param error = erroneousness of possible evaluation of index of array
  *                or potential breaking of bound of an array
  */
 void set_state_creator_value_extended(
   const state_t & state, const state_t & new_state,
   const dve_expression_t & to_assign, const all_values_t & val, bool & error);
 
 //!Internal method for implementation of both variants of get_async_succs() 
 int get_async_succs_internal(const state_t state, succ_container_t & succs);
 
 //!Internal method for implementation of both variants of get_sync_succs() 
 int get_sync_succs_internal(state_t state, succ_container_t & succs,
                             enabled_trans_container_t * const etc);
 
 //!Returns a count of elements stored in a channel with \GID `gid' in state
 //! `state'
 size_int_t channel_content_count(const state_t & state, const size_int_t gid);
 
 //!Returns true iff channel with \GID `gid' is empty in state `state'
 bool channel_is_empty(const state_t & state, const size_int_t gid);

 //!Returns true iff channel with \GID `gid' is full in state `state'
 bool channel_is_full(const state_t & state, const size_int_t gid);
 
 void push_back_channel(state_t & state, const size_int_t gid);
 void pop_front_channel(state_t & state, const size_int_t gid);
 void write_to_channel(state_t & state, const size_int_t gid,
                       const size_int_t item_index, const all_values_t value);
 all_values_t read_from_channel(const state_t & state, const size_int_t gid,
                                const size_int_t item_index,
                                const size_int_t elem_index = 0);
//INLINED FUNCTIONS:
 
 //!Returns true iff transition `t' is a transitiom from a process state
 //! `state1' with satisfied guard in a system state `state'
 /*!Returns also errors during an evaluation to the variable \a eval_err*/
 bool inline passed_through
    (const state_t state, const dve_transition_t * const t,
     const dve_state_int_t state1, bool & eval_err)
 {
  DEB(cerr << dve_state_int_t(t->get_state1_lid()) << "?=?" << state1 << " and ")
  DEB(     << "guard pointer = " << int(t->get_guard()) << endl;)
  if ((dve_state_int_t(t->get_state1_lid()) == state1) &&
      (t->get_guard() == 0 || eval_expr(t->get_guard(),state,eval_err)))
    { DEB(cerr << "Passed through" << endl;) return true; }
  else
    if (eval_err) return true; //erroneous transitions automatically pass
    else return false;
 }
 
 //!Clears list of transitions created by compute_enabled_stage1()
 void prepare_channels_info()
 {
  for (size_int_t i = 0; i!=get_channel_count(); ++i)
    { channels[i].count = 0; channels[i].error = false; }
 }
 
 //!Returns, whether at least one process is in a commited state
 bool is_commited(state_t state)
 {
  bool result = false;
  for (size_int_t i = 0; i!=processes.size() && !result; ++i)
    if (processes[i]->get_commited(get_state_of_process(state,i)))
      result = true;
  return result;
 }
 
 all_values_t retype(dve_var_type_t type, all_values_t value)
 {
  switch (type)
   {
    case VAR_BYTE: return byte_t(value); break;
    case VAR_INT: return sshort_int_t(value); break;
    default: gerr << "Unexpected error: dve_explicit_system_t::retype: "
                     "unknown type" << thr(); return 0; break;
   };
 }
}; //END of class dve_explicit_system_t



#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif
