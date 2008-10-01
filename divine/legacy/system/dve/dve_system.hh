/*!\file
 * The main contribution of this file is the class dve_system_t
 */
#ifndef DIVINE_DVE_SYSTEM_HH
#define DIVINE_DVE_SYSTEM_HH

#ifndef DOXYGEN_PROCESSING
#include <fstream>
#include "system/system.hh"
#include "system/dve/syntax_analysis/dve_parser.hh"
#include "system/dve/syntax_analysis/dve_symbol_table.hh"
#include "system/dve/dve_expression.hh"
#include "common/array.hh"
#include "common/error.hh"

//The main DiVinE namespace - we do not want Doxygen to see it
namespace divine {
#endif //DOXYGEN_PROCESSING

/*!Type of system - SYSTEM_SYNC = synchornous system,
 *                  SYSTEM_ASYNC = asynchronous system
 */
enum system_synchronicity_t { SYSTEM_SYNC = 0, SYSTEM_ASYNC = 1 };

//!Internal type for storing initial values of various identifiers
/*!Type that can contain either initial value of scalar \var1 or
 * field of initial values of vector \var1 or initial process state
 * for state of procname symbols.
 * 
 * This is an internal type.
 * Programmer of algorithms using DiVinE usually should not use this
 * structure.*/
union SYS_initial_values_t
{
 all_values_t all_value;    //!<for scalar \var1 (initial value)
 all_values_t * all_values; //!<for vector \var1 (initial values)
 size_int_t size_t_value;  //!<for state symbols (state number) and
                            //!< procname symbols (initial state number)
};

//!Structure for passing parameters to 'eval_*' functions (parameters are
//! pointers to some necessary private data of class system_t)
/* This is an internal type.
 * Programmer of algorithms using DiVinE usually should not use this
 * structure.*/
struct SYS_parameters_t
{
 SYS_initial_values_t * initial_values; //!<pointer to field initial_values
 size_int_t * initial_states; //!<pointer to field initial_states
 size_int_t * initial_values_counts;   //!<pointer to field
                                        //!<initial_values_counts
 size_int_t * state_lids; //!<pointer to field state_lids
 //!A constructor
 SYS_parameters_t(SYS_initial_values_t * const initial_vals,
                  size_int_t * const init_st,
                  size_int_t * const initial_vals_counts,
                  size_int_t * const st_lids):
  initial_values(initial_vals),initial_states(init_st),initial_values_counts(initial_vals_counts), state_lids(st_lids) {}
};

typedef SYS_parameters_t * SYS_p_parameters_t;

//!Class for a DVE system representation
/*!This class implements the abstact interface system_t.
 * 
 * This implementation contains the <b>symbol table</b> (dve_symbol_table_t)
 * (as its element).
 * It can read in and write out DVE source files. It can also offers basic
 * informations about the system represented read from DVE source file.
 * It implements the full DiVinE abstract interface and the model
 * of the system has the complete structure of DiVinE system - it means,
 * that processes, transitions and expressions are supported.
 *
 * Furthermore it provides the set of methods, which are purely DVE system
 * specific.
 *
 * <i>The very DVE system specific properties:</i>
 * (1)
 * System can be synchronous or asynchronous. These two types of system differ
 * in runs which they can do. In the synchronous system all processes have to
 * do the transition each round of run of the system. In the asynchronous system
 * only one process does do the transition except for transitions which
 * are synchronized and transitions of the property process. Therefore
 * in the asynchronous system there can simultaneously work at most 3 processes
 * (the second process is synchronized with the first process and the third
 * process can be only the property process). Such a tuple of transitions
 * which can be used in a given state of the asynchronous system is called
 * \ref enb_trans "enabled transition".
 *
 * (2)  The system needs to know the declarations of channels, \vars, states and
 * processes. For that reason it uses the class dve_symbol_table_t (\sym_table)
 * to store these declarations separatelly. Therefore system_t itself
 * doesn't provide many informations about the declarations of symbols.
 * To get these informations you should get the \sym_table first. You
 * can use the function system_t::get_symbol_table().
 */
class dve_system_t: virtual public system_t
 {
  private:
  struct expr_stack_element_t
   {
    const dve_expression_t * expr;
    array_t<dve_expression_t>::const_iterator ar;
    expr_stack_element_t(const dve_expression_t * const expr_pointer,
                 array_t<dve_expression_t>::const_iterator ar_arg):
      expr(expr_pointer),
      ar(ar_arg) {}
    void inc_ar() { ar++; }
   };

  struct expr_compacted_stack_element_t
   {
     mutable void* expr;
     void *ar;
     expr_compacted_stack_element_t(void * _expr, void* _ar):
       expr(_expr),
       ar(_ar) {}
     //     void inc_ar() { ar++; }
   };
  
  //private data obtained from post-parsing analysis:
  array_t<dve_transition_t *> transitions; //list of all transitions
  size_int_t * channel_freq_ask;
  size_int_t * channel_freq_answ;
  size_int_t * global_variable_gids;
  
  mutable std::vector<expr_stack_element_t> estack;
  mutable std::vector<expr_compacted_stack_element_t> estack_compacted;
  mutable std::vector<all_values_t> vstack;
  
  //post-parsing analysis is run in read() method
  void consolidate();
  //deletes the most of fields dynamicaly allocated in consolidate()
  void delete_fields();
  
  protected:
  dve_symbol_table_t * psymbol_table;      //!<"Symbol table"

  //protected data obtained from post-parsing analysis:
  array_t<dve_process_t *> processes; //!<field of processes (0th element is not used)
  size_int_t process_field_size; //!<capacity of field `processes'
  size_int_t channel_count;
  size_int_t var_count;
  size_int_t state_count;
  size_int_t glob_var_count; //!<count of global \vars
  
  system_synchronicity_t system_synchro;//<!Synchronicity of the system
  size_int_t prop_gid;       //!<\GID of the property process
  dve_process_t * pproperty; //!<The pointer to the property process
  bool property_has_synchronization; //!<Says, whether the property contains synchronization
  
  //!Constant set to initial_values_counts for scalar variables with initializer
  const size_int_t MAX_INIT_VALUES_SIZE;
  
  //! initial values for some symbols (see union SYS_initial_values_t)
  SYS_initial_values_t * initial_values;
  //!additional information to `initial_values'
  /*!
   * `initial_values_counts' is the additional information to
   * `intial_values':
   * initial_values_counts[i] =
   *  - 0 => no initial value
   *  - MAX_INIT_VALUES_SIZE => scalar, state or procname
   *  - 1..(MAX_INIT_VALUES_SIZE-1) => vector
   */
  size_int_t * initial_values_counts;
  size_int_t * initial_states;
  size_int_t * state_lids;
  //!types of \vars (GID of \var1 is an index to this field)
  dve_var_type_t * var_types;
  bool * constants;

  //function \vars for evaluating expressions:
  //!parameters points to the piece of the memory where additional informations
  //! for 'eval_*' functions are stored.
  void * parameters;

  //!functional \var1 that points to the function, which should be used
  //! to evaluate \vars
  all_values_t (*eval_id)
    (const void * parameters, const dve_expression_t & expr, bool & eval_err);
  all_values_t (*eval_id_compact)
    (const void * parameters, const size_int_t & gid, bool & eval_err);
  //!functional \var1 that points to the function, which should be used
  //! to evaluate \var1 fields
  all_values_t (*eval_square_bracket)
    (const void * parameters, const dve_expression_t & subexpr,
     const size_int_t & array_index, bool & eval_err);
  all_values_t (*eval_square_bracket_compact)
    (const void * parameters, const size_int_t  & gid,
     const size_int_t & array_index, bool & eval_err);
  //!functional \var1 that points to the function, which should be used
  //! to evaluate state identifiers
  all_values_t (*eval_dot)
    (const void * parameters, const dve_expression_t & subexpr, bool & eval_err);
  all_values_t (*eval_dot_compact)
    (const void * parameters, const dve_symbol_table_t *symbol_table, const size_int_t & gid,  bool & eval_err);
  //!Evaluates an expression.
  /*!Carries out complete expression evaluation. Evaluation of
   * \vars and state identifiers can be changed in the descendant
   * by changing protected \vars `eval_id', `eval_square_bracket',
   * and `eval_dot'.
   * \param expr = pointer to structure representing expression
   * \param eval_err = boolean that says, whether evaluation passed correctly*/
  all_values_t eval_expr(const dve_expression_t * const expr, bool & eval_err) const;
  //!Evaluates a compacted expression.
  /*!Carries out complete expression evaluation over compacted
   * expression. Evaluation of \vars and state identifiers can be changed in the
   * descendant by changing protected \vars `eval_id_compact',
   * `eval_square_bracket_compact', and `eval_dot_compact'.  \param p_compacted
   * = pointer to compacted expresion \param eval_err = boolean that says,
   * whether evaluation passed correctly */
  all_values_t fast_eval(compacted_viewer_t *p_compacted, bool &eval_err) const;
  
  public:
  //!A constructor.
  /*!\param evect =
   * the <b>error vector</b>, that will be used by created instance of system_t
   */
  dve_system_t(error_vector_t & evect = gerr);
  //!A destructor.
  virtual ~dve_system_t();
  /*! @name Obligatory part of abstact interface
     These methods have to implemented in each implementation of system_t*/
  //!Implements system_t::read(std::istream & ins = std::cin) in DVE \sys
  virtual slong_int_t read(std::istream & ins = std::cin);
  //!Implements system_t::read(const char * const filename) in DVE \sys
  virtual slong_int_t read(const char * const filename);
  //!Implements system_t::from_string(const std::string str) in DVE \sys
  virtual slong_int_t from_string(const std::string str);
  //!Implements system_t::write(const char * const filename) in DVE \sys
  virtual bool write(const char * const filename);
  //!Implements system_t::write(std::ostream & outs = std::cout) in DVE \sys
  virtual void write(std::ostream & outs = std::cout);
  //!Implements system_t::to_string() in DVE \sys
  virtual std::string to_string();
  /*@}*/
  
  ///// DVE SYSTEM CANNOT PROVIDE PROPERTY DECOMPOSITION ////
    /*!@name Methods to check the SCCs of property process graph
    These methods are implemented only if can_decompose_property()
    returns true.
   @{*/
   
  /*! Returns id of an SCC that the given state projects to.*/
  virtual int get_property_scc_id(state_t& _state) const
  { 
    if ((_state.ptr) || !(_state.ptr))
      {
	gerr<<"dve_system_t::get_property_scc_id not implemented"<<thr();
      }
    return 0;
  }
  
  /*! Returns type of the given SCC, where 0 means nonaccepting component, 1
    means partially accepting component, and 2 means fully accepting
    component. */
  virtual int get_property_scc_type(int _scc) const
  {
    if ((_scc) || !(_scc))
      {
	gerr<<"dve_system_t::get_property_scc_type not implemented"<<thr();
      }
    return 0;
  }

  
  /*! Returns type of an SCC that the given state projects to, where 0 means
    nonaccepting component, 1 means partially accepting component, and 2 means
    fully accepting component.*/
  virtual int get_property_scc_type(state_t& _state) const
  {
    if ((_state.ptr) || !(_state.ptr))
      {
	gerr<<"dve_system_t::get_property_scc_type not implemented"<<thr();
      }
    return 0;
  }
  
  /*! Returns the number of SCCs in the decomposition.*/
  virtual int get_property_scc_count() const
  {
    gerr<<"dve_system_t::get_property_scc_count not implemented"<<thr();
    return 0;
  }
  
  /*! Returns whether the process has a weak graph. */
  virtual bool is_property_weak() const
  {
    gerr<<"dve_system_t::is_property_weak not implemented"<<thr();
    return 0;
  }
  /*@}*/
 

  ///// DVE SYSTEM CAN WORK WITH PROPERTY PROCESS: /////
  /*! @name Methods working with property process
    These methods are implemented and can_property_process() returns true
   @{*/
  //!Implements system_t::get_property_process() in DVE \sys
  virtual process_t * get_property_process() { return pproperty; }
  //!Implements system_t::get_property_process() in DVE \sys
  virtual const process_t * get_property_process() const { return pproperty; }
  //!Implements system_t::get_property_gid() in DVE \sys
  virtual size_int_t get_property_gid() const { return prop_gid; }
  //!Implements system_t::set_property_gid() in DVE \sys
  virtual void set_property_gid(const size_int_t gid);
  //!Implements system_t::get_property_type() in DVE \sys
  virtual property_type_t get_property_type()
  {
    if (get_with_property())
      return (dynamic_cast<dve_process_t *>(get_property_process()))->get_accepting_type();
    else 
      return NONE;
  }
  /*@}*/

  
  ///// DVE SYSTEM CAN WORK WITH PROCESSES: /////
  /*!@name Methods working with processes
    These methods are implemented and can_processes() returns true.
   @{*/
  //!Implements system_t::get_process_count() in DVE \sys
  virtual size_int_t get_process_count() const 
  { return processes.size(); }
  //!Implements system_t::get_process(const size_int_t gid) in DVE \sys
  virtual process_t * get_process(const size_int_t gid)
    { return processes[gid]; }
  //!Implements system_t::get_process(const size_int_t id) const in DVE \sys
  virtual const process_t * get_process(const size_int_t id) const
    { return processes[id]; }
  /*@}*/


  ///// DVE SYSTEM CAN WORK WITH TRANSITIONS: /////
  /*!@name Methods working with transitions
    These methods are implemented and can_transitions() returns true.
   @{*/
  //!Implements system_t::get_trans_count() in DVE \sys, but see also
  //! implementation specific notes below
  /*!Returned count of transitions comprises also invalid transitions
   * (transitions, where transition_t::get_valid() returns false)
   */
  virtual size_int_t get_trans_count() const { return transitions.size(); }
  //!Implements system_t::get_transition(size_int_t gid) in DVE \sys
  virtual transition_t * get_transition(size_int_t gid)
  { return transitions[gid]; }
  //!Implements system_t::get_transition(size_int_t gid) const in DVE \sys
  virtual const transition_t * get_transition(size_int_t gid) const
  { return transitions[gid]; }
  /*@}*/
  
  ///// DVE SYSTEM CAN BE MODIFIED: /////
  /*!@name Methods modifying a system
     These methods are implemented and can_be_modified() returns true.
   @{*/
  //!Implements system_t::add_process() in DVE \sys, but see also
  //! implementation specific notes below.
  /*!\warn
   * Imporant thing is, that this is the only permitted way of adding
   * the process to the \sys. Program should not use
   * dve_symbol_table_t::add_process() instead of this method.
   */
  virtual void add_process(process_t * const process);
  //!Implements system_t::remove_process()
  virtual void remove_process(const size_int_t process_id);
  /*@}*/

  ///// DVE SYSTEM SPECIFIC METHODS: /////
  
  /*!@name DVE system specific methods
     These methods are implemented only in DVE system and they
     cannot be run using an abstract interface of system_t.
   @{*/
  //!Returns true, iff whether the property process contains any synchronizaiton
  bool get_property_with_synchro() const { return property_has_synchronization;}
  
  //!Sets, whether the property process contains any synchronizaiton
  /*!\param contains_synchro = true, iff the property process contains any synchronizaiton
   */
  void set_property_with_synchro(const bool contains_synchro)
  { property_has_synchronization = contains_synchro; }
  
  //!Returns count of channel usage in processes (`ch_gid' = GID of channel).
  size_int_t get_channel_freq(size_int_t ch_gid) const
   { return (channel_freq_ask[ch_gid] + channel_freq_answ[ch_gid]); }
  //!Returns count of channel usage in processes in a synchronization
  //! with a question mark.
  size_int_t get_channel_freq_ask(size_int_t ch_gid) const
   { return (channel_freq_answ[ch_gid]); }
  //!Returns count of channel usage in processes in a synchronization
  //! with a exclamaion mark.
  size_int_t get_channel_freq_exclaim(size_int_t ch_gid) const
   { return (channel_freq_ask[ch_gid]); }
  //!Returns pointer to the \sym_table.
  /*!\Sym_table (= instance of class dve_symbol_table_t) is
   * automatically created in creation time of system_t as
   * its private data member. It contains all declarations of all
   * named symbols (processes, channels, \vars and process states).
   * 
   * See \sym_table and dve_symbol_table_t for details.*/
  dve_symbol_table_t * get_symbol_table() const { return psymbol_table; }
  //!Returns a count of channels in the system.
  size_int_t get_channel_count() const { return channel_count; }
  //!Returns a synchronicity of the \sys.
  /*!\return value of type system_synchronicity_t:
   * - SYSTEM_ASYNC iff system is defined as asynchronous
   * - SYSTEM_SYNC iff system is defined as synchronous */
  system_synchronicity_t get_system_synchro() const { return system_synchro; } 
  //!Sets a synchronicity of the \sys.
  void set_system_synchro(const system_synchronicity_t sync)
    { system_synchro = sync; } 
  //!Returns a \GID of global \vars
  /*!Returns a \GID of global \vars. \a i is an index to the list of
   * global \vars (in an interval 0..(get_global_variable_count()-1) ).
   * You can obtain this index by dve_symbol_t::get_lid(). */
  size_int_t get_global_variable_gid(const size_int_t i) const
   { return global_variable_gids[i]; }
  //!Returns a count of global \vars
  size_int_t get_global_variable_count() const
   { return glob_var_count; }
  //!Returns, whether value of `value' is in bounds of type `var_type'
  bool not_in_limits(dve_var_type_t var_type, all_values_t value);
  //!For debugging purposes.
  /*!Prints all \vars with their initial values*/
  void DBG_print_all_initialized_variables();
  /*@}*/
 };

#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#endif //DOXYGEN_PROCESSING

#endif
