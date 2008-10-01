/*!\file
 * The main contribution of this file is class dve_parser_t, that serves as a symbol
 * table and the interface for parsing and analysing DVE sources.
 *
 * \author Pavel Simecek
 */
#ifndef DIVINE_DVE_PARSER
#define DIVINE_DVE_PARSER

#ifndef DOXYGEN_PROCESSING
#include <vector>
#include <list>
#include <stack>
#include <cstdio>
#include <string>
#include "common/error.hh"
#include "common/bit_string.hh"
#include "common/array.hh"
#include "system/dve/dve_expression.hh"
#include "system/dve/dve_transition.hh"
#include "system/dve/dve_prob_transition.hh"
#include "system/dve/dve_process.hh"
#include "system/dve/syntax_analysis/dve_symbol.hh"
#include "system/dve/syntax_analysis/dve_symbol_table.hh"
#include "system/dve/syntax_analysis/dve_commonparse.hh"
#include "system/dve/dve_prob_process.hh"
#include "common/array.hh"
#include "system/dve/dve_system.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

//!maximal bound for array in DVE source
const size_int_t DVE_MAX_ARRAY_SIZE = 2147483647;

class dve_system_t;
class dve_prob_process_t;
class dve_prob_transition_t;

//!Class that provides an interface to the parsing of DVE sources.
/*!It is used by reading methods of dve_system_t, dve_process_t, dve_transition_t
 * and dve_expression_t
 */
class dve_parser_t {
 public:
  //!Modes of parsing
  /*!Used in methods get_mode(), set_mode(). Mode of parsing is automatically
   * set by used constructor od dve_parser_t
   */
  enum parser_mode_t { SYSTEM, //!< Parser will parse entire DVE source file
                       PROCESS, //!< Parser will parse only DVE process
                       PROB_TRANSITION, //!< Parser will parse only DVE prob. transition
                       TRANSITION, //!< Parser will parse a DVE transition
                       EXPRESSION //!< Parser will parse a DVE expression
                     };
  //!A constructor for creating of a parser of entire DVE source file
  dve_parser_t(error_vector_t & evect, dve_system_t * my_system);
  //!A constructor for creating of a parser of DVE process
  dve_parser_t(error_vector_t & evect, dve_process_t * my_process);
  //!A constructor for creating of a parser of probabilistic DVE process
  dve_parser_t(error_vector_t & evect, dve_prob_process_t * my_process);
  //!A constructor for creating of a parser of DVE transition
  dve_parser_t(error_vector_t & evect, dve_transition_t * my_transition);
  //!A constructor for creating of a parser of DVE transition
  dve_parser_t(error_vector_t & evect, dve_prob_transition_t * my_transition);
  //!A constructor for creating of a parser of DVE expression
  dve_parser_t(error_vector_t & evect, dve_expression_t * my_expression);
  //!A destructor
  ~dve_parser_t();
  
  //!Sets, whether parsed system can be probabilistic
  void set_probabilistic(const bool can_be_prob)
  { probabilistic = can_be_prob; }

  //!Returns, whether parsed system can be probabilistic
  bool get_probabilistic() { return probabilistic; }
  
  /*! @name Parsing-time methods 
   * These methods are called by a Yacc/Bison generated parser
   * and they are determined to store the informations from the syntax
   * analysis.
   *
   * These functions should not be interesting for a usual developer. */
  //@{
  //!Called by Bison parser in case of reporting of the beginning of
  //! the current position to this class
  void set_fpos(int line, int col);
  //!Called by Bison parser in case of reporting of the end of
  //! the current position to this class
  void set_lpos(int line, int col);
  //!Returns the first line of the current position of Bison parser
  int get_fline() { return firstline; }
  //!Returns the last line of the current position of Bison parser
  int get_lline() { return lastline; }
  //!Returns the first column of the current position of Bison parser
  int get_fcol()  { return firstcol; }
  //!Returns the column line of the current position of Bison parser
  int get_lcol()  { return lastcol; }
  
  
  //!Method called by Bison parser after finished parsing
  void done();
  
  
  //!Called by Bison parser, when it observes the begin of process
  void proc_decl_begin(const char * name);
  //!Called by Bison parser, when it observes the end of process
  void proc_decl_done();
  
  
  //!Called by Bison parser, when it observes, that newly declared variable is
  //! constant
  void type_is_const(const bool const_prefix) { current_const = const_prefix; }
  
  
  //!Called by Bison parser, when parsing of expression is expected
  void take_expression();
  //!Called by Bison parser in case of error during parsing of expression
  void take_expression_cancel();
  
  
  //!Called by Bison parser, when variable declaration begins
  /*!Bison parser tells to this class the type of curretly declared
   * variable in parameter \a type_nbr
   */
  void var_decl_begin(const int type_nbr);
  //!Called by Bison parser, during parsing of initial value of variable.
  /*!Parser reports, whether initial value is an array of scalar value*/
  void var_init_is_field(const bool is_field);
  //!Called by Bison parser, while it finishes a parsing of expression
  //! initialization
  /*!Stores last parsed expression to the vector of initial values*/
  void var_init_field_part();
  //!Called by Bison parser after finished parsing of declaration of variable
  /*!Currently empty method - variable is really created in var_decl_create()*/
  void var_decl_done();
  //!Called by Bison parser after parsing of identifier and initial
  //! values
  /*!Creates a symbol in \Sym_table*/
  void var_decl_create(const char * name, const int arrayDimesions,
                       const bool initialized);
  //!Called by Bison parser, when error occures during parsing of declaration
  //! of variable
  void var_decl_cancel();
  //!Called by Bison parser, when number representing a bound of array
  //! is parsed.
  void var_decl_array_size(const int bound);
  
  
  //!Called by Bison parser, when token "true" is read during parsing of
  //! an expression
  void expr_false();
  //!Called by Bison parser, when token "false" is read during parsing of
  //! an expression
  void expr_true();
  //!Called by Bison parser, when token representing a natural number is read
  //! during parsing of an expression
  void expr_nat(const int num);
  //!Called by Bison parser, when token representing an identifier of variable
  //! is read during parsing of an expression
  /*!Variable must not be an array - arrays are covered by method
   * expr_array_mem()*/
  void expr_id(const char * name, int expr_op = T_ID);
  //!Called by Bison parser, when token representing an identifier of array
  //! is read during parsing of an expression
  void expr_array_mem(const char * name, int expr_op = T_SQUARE_BRACKETS);
  //!Called by Bison parser, when parethesis are read during parsing of an
  //! expression
  /*!At the time of calling of this method everything inside them is already
   * parsed.*/
  void expr_parenthesis();
  //!Called by Bison parser, when assignment is read during parsing of an
  //! expression
  /*!Assignment is usually not allowed in the syntax of expressions, but
   * this method is only called in effect of transition.
   */
  void expr_assign(const int assignType);
  //!Called by Bison parser, when unary operator is read during parsing of an
  //! expression
  void expr_unary(const int unaryType);
  //!Called by Bison parser, when binary operator is read during parsing of
  //! an expression
  void expr_bin(const int binaryType);
  //!Called by Bison parser, when a test on a state of a process has been read
  //! during parsing of an expression
  void expr_state_of_process(const char * proc_name, const char * name);
  //!Called by Bison parser, when a value of a foreign variable has been
  //! read during parsing
  void expr_var_of_process(const char * proc_name, const char * name,
                          const bool array = false);
  
  //!Called by Bison parser, when declaration of state is parsed
  void state_decl(const char * name);
  //!Called by Bison parser, when declaration of initial state is parsed
  void state_init(const char * name);

  //!Called by Bison parser, when property type is clear (ie complete accepting
  //!condition parsed)
  void accept_type(int type);

  //!Called by Bison parser, when the declaration of accepting state in Buchi
  //!accepting condition is done
  void state_accept(const char * name);

  //!Called by Bison parser, when the declaration of an accepting set in
  //!GenBuchi or Muller accepting condition is complete
  void accept_genbuchi_muller_set_complete();

  //!Called by Bison parser, when the declaration of accepting state in
  //!GenBuchi or Muller accepting set is done
  void state_genbuchi_muller_accept(const char * name);
  
  //!Called by Bison parser, when the declaration of an accepting pair in
  //!Rabin or Street accepting condition is complete
  void accept_rabin_streett_pair_complete();

  //!Called by Bison parser, when the declaration of first part of the accepting
  //!pair in Rabin or Street accepting condition is complete
  void accept_rabin_streett_first_complete();

  //!Called by Bison parser, when the declaration of accepting state in
  //!Rabin or Streett accepting set is done
  void state_rabin_streett_accept(const char * name);

  //!Called by Bison parser, when declaration of commited state is parsed
  void state_commit(const char * name);
  //!Called by Bison parser, when declarations of all states of current
  //! process are parsed
  void state_list_done();
 
  //!Called by Bison parser after parsing of assertion definition
  void assertion_create(const char * state_name);
 
  //!Called by Bison parser after parsing of transition
  void trans_create(const char * name1, const char * name2,
    const bool has_guard, const int sync, const int effect_count);
  //!Called by Bison parser after keyword "effect" is parsed
  void trans_effect_list_begin();
  //!Called by Bison parser after all effects are parsed
  void trans_effect_list_end();
  //!Called by Bison parser, when error occurs during parsing of effects
  void trans_effect_list_cancel();
  //!Called by Bison parser, when single effect is parsed
  /*!Last expression (containing effect's assignment) is then taken as an
   * effect of the current transition
   */
  void trans_effect_part();
  //!Called by Bison parser, when synchronization in a transition is parsed
  void trans_sync(const char * name, const int sync, const bool sync_val);
  //!Called by Bison parser, when guard in a transition is parsed
  void trans_guard_expr();
  
  
  //!Called by Bison parser, when declaration of channel is parsed
  void channel_decl(const char * name);
  //!Called by Bison parser, when declaration of typed channed is parsed
  void typed_channel_decl(const char * name, const int buffer_size);
  
  //!Called by Bison parser, when the item of the list of types is parsed
  void type_list_add(const int type_nbr);
  //!Called by Bison parser, when the item of the list of types is already
  //! copied somewhere and thus it can be cleared
  void type_list_clear();
  
  //!Called by Bison parser, when the item of the list of expressions is parsed
  void expression_list_store();

  
  //!Called by Bison parser, when synchronicity of a system is parsed
  void system_synchronicity(const int sync, const bool prop = false);
  //!Called by Bison parser, when a name of the property process is parsed
  void system_property(const char * name);

  //!Called by Bison parser, when probabilistic transition is detected
  void prob_trans_create(const char * name);
  
  //!Called by Bison parser, when a part of probabilistic transition is read
  void prob_transition_part(const char * name, const int weight);
  //@}
  
 private:
  //!Initialisation of a parser (called e.g. in creators)
  void initialise(dve_system_t * const pointer_to_system);
  //!Method that adds a channel with a name `name'
  /*!\param name = name of channel
   * \param typed = true iff this channel is a typed channel
   * \param buffer_size = length of buffer in a typed channel
   * \return true iff symbol does not already exist in the symbol table
   */
  bool add_channel(const char * name, const bool typed = false,
                   const int buffer_size = 0);
  //!Method that adds a process with a name `name'
  size_int_t add_process(const char * name,
                            const bool only_temporary=false);
  //!Method that adds a state with a name `name' to the process with \GID
  //! `proc_id`
  size_int_t add_state
    (const char * const name, const size_int_t proc_id,
     const bool only_temporary = false);
  //!Method that adds a variable with a name `name' to the process with \GID
  //! `proc_id'
  bool add_variable
    (const char * name, const size_int_t proc_id, const bool initialized,
     const bool isvector=false);
  //!Adds a given transition to the current process
  void add_transition(dve_transition_t * const transition);

  //!Adds a given probabilistic transition to the current process
  void add_prob_transition(dve_prob_transition_t * const prob_trans);
 public:
  //!Returns the last parsed expression
  /*!Causes an exception, when there is not exacly one expression stored
   * in a stack of parsed expressions
   */
  dve_expression_t * get_expression() const;
  //!Sets the context for parsing of expression or transition
  void set_current_process(size_int_t process_gid)
    { current_process = process_gid; }
  //!Returns the context for parsing of expression or transition
  size_int_t get_current_process() const { return current_process; }
  //!Returns a mode of parsing
  /*!Parser is able to parse a system (in DVE format), process, transition
   * or expression according to the pointer given in a constructor */
  parser_mode_t get_mode() { return mode; }
  //!Sets a mode of parsing
  void set_mode(const parser_mode_t new_mode) { mode = new_mode; }
  //!Returns a system, which is filled in by the parser
  dve_system_t * get_dve_system_to_fill() { return dve_system_to_fill; }
  //!Sets a system, which should be filled in a parser
  void set_dve_system_to_fill(dve_system_t * const sys);
  //!Returns a process, which is filled in by the parser
  dve_process_t * get_dve_process_to_fill() { return dve_process_to_fill; }
  //!Sets a process, which should be filled in a parser
  void set_dve_process_to_fill(dve_process_t * const proc);
  //!Returns a transition, which is filled in by the parser
  dve_transition_t * get_dve_transition_to_fill() { return dve_transition_to_fill; }
  //!Sets a transition, which should be filled in a parser
  void set_dve_transition_to_fill(dve_transition_t * const trans);
  //!Returns a probabilistic transition, which is filled in by the parser
  dve_prob_transition_t * get_dve_prob_transition_to_fill() { return dve_prob_transition_to_fill; }
  //!Sets a probabilistic transition, which should be filled in a parser
  void set_dve_prob_transition_to_fill(dve_prob_transition_t * const trans);
  //!Returns a expression, which is filled in by the parser
  dve_expression_t * get_dve_expression_to_fill() { return dve_expression_to_fill; }
  //!Sets a expression, which should be filled in a parser
  void set_dve_expression_to_fill(dve_expression_t * const expr);
  //!Takes a property process currently set in a filled system and
  //! checks, whether all restrictions are fulfilled. If not, it throws errors.
  void check_restrictions_put_on_property();

  
 private:
 ////PRIVATE TYPES:
  typedef size_int_t bound_t;
  //Requirement on a process to be a property process - only in a case,
  //when a variable of another process is refered in that process, which
  //is permited only inside of property process.
  struct prop_req_t { bool is_set;  //whether there is such a requirement
                      size_int_t process;  //which process requires it
                      unsigned int l1; unsigned int l2; //lines of requirement
                      unsigned int c1; unsigned int c2; //columns of requirement
		    };
 ////PRIVATE METHODS:
  //!Computes count of global variables and stores it into the glob_count
  //! variable (useful in a case of parsing in mode PROCESS of TRANSITION)
  void compute_glob_count_according_to_symbol_table();
  
  /*!In a case of refering variable of other process using syntax construct
   * process_name.variable_name there comes the requirement on the process
   * where this construct is placed to be a property process, because
   * such a construct is permitted only inside the property process
   */
  void register_requirement_for_being_property_process(
                                                    const size_int_t proc_id);
  
  //Private methods of adding content to the expression.
  //They are working above the subexpr stack
  void add_sub_op(int expr_op, short int ar = 0);
  void add_sub_id(size_int_t sid, int expr_op = T_ID, short int ar = 0);
  void add_sub_num(int nbr);
  
  dve_process_t * get_current_process_pointer() const;

 ////DATA:
  //flag, whether the parsed system can be probabilistic
  bool probabilistic;
  
  //context of process, where the parsing is running (NO_ID in case of
  //global environment)
  size_int_t current_process;
  
  //stack of already parsed, but unsorted subexpression:
  std::stack<dve_expression_t *> subexpr_stack; 
  
  bound_t current_bound;
  int current_vartype;
  bool current_const;
  bool last_init_was_field;
  dve_expression_t * current_guard;
  std::list<dve_expression_t *> current_sync_expr_list;
  std::vector<dve_expression_t *> current_effects;
  size_int_t current_channel_gid;
  std::vector<dve_expression_t *> current_field;
  size_int_t current_first_state_gid;
  std::list<size_int_t> glob_used;
  std::list<dve_expression_t *> current_expression_list;
  std::list<dve_var_type_t> current_type_list;
  prop_req_t prop_req; //requirement on process to be property process
  size_int_t glob_count;
  bool save_glob_used;
  error_vector_t & terr;
  
  parser_mode_t mode; //mode of parsing (see parser_mode_t)
  
  //objects to fill in:
  dve_system_t * dve_system_to_fill;
  dve_process_t * dve_process_to_fill;
  dve_transition_t * dve_transition_to_fill;
  dve_expression_t * dve_expression_to_fill;
  dve_prob_transition_t * dve_prob_transition_to_fill;
  
  //symbol table to fill in and to take the informations from:
  dve_symbol_table_t * psymbol_table;
  dve_system_t * psystem;
  
  //varibles for storing the position in the source
  int firstline;
  int lastline;
  int firstcol;
  int lastcol;
  

  struct prob_trans_part_t{ std::string name; int weight; };
  //variable to temporarily store parts prob. transition
  std::list<prob_trans_part_t> prob_trans_parts;
  std::list<transition_t *> prob_trans_trans;
  typedef std::list<prob_trans_part_t>::const_iterator prob_trans_part_iter_t;
  typedef std::list<transition_t *>::const_iterator prob_trans_trans_iter_t;
  
  //auxiliary variables to handle accepting condition specification
  size_int_t accepting_groups;
  bool new_accepting_set;
  std::list< std::list<size_int_t> > accepting_sets;

}; //end of class dve_parser_t


#ifndef DOXYGEN_PROCESSING
} //end of namespace

#endif //DOXYGEN_PROCESSING

#endif
