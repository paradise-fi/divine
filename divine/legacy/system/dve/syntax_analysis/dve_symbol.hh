#ifndef DVE_SYMBOL_HH
#define DVE_SYMBOL_HH
/*!\file symbol.hh
 * The main contribution of this file is a definition of class dve_symbol_t.
 * There are also declared some types used in a class dve_symbol_t: e. g. 
 * \ref dve_var_type_t and \ref dve_sym_type_t */

#ifndef DOXYGEN_PROCESSING
#include <vector>
#include "system/dve/dve_expression.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
#endif //DOXYGEN_PROCESSING

class dve_expression_t;
//!Integer type of a variable
enum dve_var_type_t
 {
  VAR_BYTE = 0, /*!<... byte variable (0..255)*/
  VAR_INT = 1 /*!<... integer variable (-32768..32767)*/
 };
//!A type of a symbol
enum dve_sym_type_t
  { SYM_VARIABLE, //!<symbol is a variable
    SYM_STATE,    //!<symbol is a process state
    SYM_PROCESS,  //!<symbol is a process
    SYM_CHANNEL   //!<symbol is a channel
    };

//!Class that represents the declaration of symbol (= process, channel,
//! variable or process state).
/*!dve_symbol_t represents so called symbol. Symbol is a named entity in
 * a \sys.
 * 
 * <h3>The common properties of symbols</h3>
 * Each symbol has a unique identifier \SID. You can obtain it using get_sid().
 *
 * Each symbol has also an identifier \GID. \GID is unique too, but only
 * among the symbols of the same type. You can get it using get_gid().
 *
 * Locally declared symbols have also an identifier \LID. \LID is unique
 * among all symbols inside the samo process. Only variables and process states
 * can be declared locally. You can get \LID of symbol using get_lid().
 * Method get_lid() has a <b>special semantics in case of global symbols</b>.
 * 
 * Each locally declared symbol has a unique parent process (a process where
 * the symbol was declared). You can obtain a \GID of this process by
 * method get_process_gid(). In case of global symbols method get_process_gid()
 * returns NO_ID.
 *
 * Each symbol has its name of course. You can get it using a method get_name().
 * The length of this name is available using a method get_name_length().
 * 
 * It is requested to distinguish among separate types of symbols. You
 * can do is using a method get_symbol_type() or using a set of functions
 * is_variable(), is_process(), is_channel() and is_state().
 * 
 * There are altogether 4 types of symbols - see \ref symbols_page for details.
 */
class dve_symbol_t
 {
  private:
  struct var_t //structure of informations about variable symbols
  {
   bool vector;
   bool const_var;
   dve_var_type_t var_type;
   size_int_t bound;
   union
    {
     dve_expression_t * expr;
     std::vector<dve_expression_t *> * field;
    } init;
  }; //END of var_t structure
  size_int_t lid;   //type-of-symbol-dependent local identifier
  size_int_t sid;   //global symbol identifier
  size_int_t gid;   //type-of-symbol-dependent global ID
  dve_sym_type_t type;
  const char *name;//name of symbol
  size_int_t namelen; //length of symbol name
  bool valid;  //not really declared, but already used
  var_t info_var; //more information about symbol
  size_int_t frame;          //info for proctype
  size_int_t channel_item_count; //information about type of channel
  bool channel_typed; //tells, whether channel is typed=>next 2 items are valid:
  array_t<dve_var_type_t> channel_type_list; //the list of types transmitted
                                             //simultaneously
  slong_int_t channel_buffer_size; //size of the buffer for typed channeld
  
  public:
  //!Return value get_channel_item_count() iff channel has not been used
  /*!Constant returned by get_channel_item_count() if the number of items
   * transmittable simultaneously through a channel has not been initialized
   * (by the first usage of channel)*/
  static const size_int_t CHANNEL_UNUSED;
  //!A constructor
  /*!\param t = type of a symbol
   * \param n = name of a symbol
   * \param l = length of a name of a symbol
   */
  dve_symbol_t(const dve_sym_type_t t, const char * const n, const size_int_t l):
     lid(NO_ID), sid(NO_ID), gid(NO_ID), type(t), name(n),
     namelen(l), valid(false), channel_item_count(CHANNEL_UNUSED),
     channel_typed(false), channel_buffer_size(0) {}
  //!A destructor
  ~dve_symbol_t();
  //!Returns a \LID of symbol.
  /*!Returns a \LID of symbol. If the symbol is global, it returns:
   * - \GID in case of process, channel and state
   * - index to the list of global variables (you can use it as a parameter of
   *   system_t::get_global_variable_gid()) */
  size_int_t get_lid() const { return lid; }
  //!Sets a \LID of symbol.
  void set_lid(const size_int_t lid_arg) { lid = lid_arg; }
  //!Returns a \SID of symbol
  size_int_t get_sid() const { return sid; }
  //!Sets a \SID of symbol.
  void set_sid(const size_int_t symb_id) { sid = symb_id; }
  //!Returns a \GID of symbol
  size_int_t get_gid() const { return gid; }
  //!Sets a \GID of symbol.
  void set_gid(const size_int_t gid_arg) { gid = gid_arg; }
  //!Returns a \GID of process in which the symbol is declared
  /*!Returns a \GID of process in which the symbol is declared.
   * If the symbol is global, returns NO_ID. */
  size_int_t get_process_gid() const { return frame; }
  //!Sets a \GID of process in which the symbol is declared
  void set_process_gid(const size_int_t nbr) { frame = nbr; }
  //!Returns a name of symbol
  const char * get_name() const { return name; }
  //!Sets a name of symbol - be aware of correct length of `new_name'
  void set_name(const char * new_name) { name = new_name; }
  //!Returns a length of name of symbol
  size_int_t get_name_length() const { return namelen; }
  //!Sets a name of a symbol and its length.
  void set_name(const char * const str, const size_int_t length)
   { namelen = length; name = str; }
  //!Returns a type of symbol - see \ref dve_sym_type_t
  dve_sym_type_t get_symbol_type() const { return type; }
  //!Sets a type of symbol - see \ref dve_sym_type_t
  void set_symbol_type(const dve_sym_type_t symb_type) { type = symb_type; }
  //!Returns whether symbol is considered valid (invalid==ignored)
  bool get_valid() const { return valid; }
  //!Sets whether symbol is considered valid (invalid==ignored)
  void set_valid(const bool is_valid) { valid = is_valid; }
  //!Returns true iff symbol is a variable
  bool is_variable() const { return (type == SYM_VARIABLE); }
  //!Returns true iff symbol is a process
  bool is_process() const { return (type == SYM_PROCESS); }
  //!Returns true iff symbol is a channel
  bool is_channel() const { return (type == SYM_CHANNEL); }
  //!Returns true iff symbol is a process state
  bool is_state() const { return (type == SYM_STATE); }
  /*! @name Following functions can be called only for variable symbols */
  //@{
  //!Returns true iff a variable symbol is a vector
  bool is_vector() const { return info_var.vector; }
  //!Sets, whether a variable symbol is a vector
  void set_vector(const bool is_vector) { info_var.vector = is_vector; }
  //!Returns true iff a variable symbol is constant
  bool is_const() const { return info_var.const_var; }
  //!Sets, whether a variable symbol is constant
  void set_const(const bool const_var) { info_var.const_var = const_var; }
  //!Returns true iff a variable symbol is of byte type
  /*!Returns true iff a variable symbol is of type byte - that is
   * dve_symbol_t::get_var_type() returns VAR_BYTE. */
  bool is_byte() const { return (info_var.var_type == VAR_BYTE); }
  //!Returns true iff a variable symbol is of integer type
  /*!Returns true iff a variable symbol is of integer type - that is
   * dve_symbol_t::get_var_type() returns VAR_INT. */
  bool is_int() const { return (info_var.var_type == VAR_INT); }
  //!Returns a type of variable - see \ref dve_var_type_t
  dve_var_type_t get_var_type() const { return info_var.var_type; }
  //!Sets a type of variable - see \ref dve_var_type_t
  void set_var_type(const dve_var_type_t var_type) { info_var.var_type=var_type; }
  //@}

  /*! @name Following functions can be called only for scalar variables */
  //@{
  //!Returns an initial value of scalar variable.
  /*!Returns an initial value of scalar variable. Initial value is represented
   * by a pointer to the expression (see dve_expression_t) and it can be 0, when
   * there was no initial value.*/
  dve_expression_t * get_init_expr() { return (info_var.init.expr); }
  //!The same as get_init_expr() above, but for constant instances of this
  //! class.
  const dve_expression_t * get_init_expr() const { return (info_var.init.expr); }
  //!Sets an initial value of scalar variable.
  void set_init_expr(dve_expression_t * const p_expr)
   { info_var.init.expr = p_expr; }
  //@}
  /*! @name Following functions can be called only for vector variables */
  //@{
  //!Returns a size of the vector.
  size_int_t get_vector_size() const { return info_var.bound; }
  //!Sets a size of the vector.
  void set_vector_size(const size_int_t ar_size) { info_var.bound = ar_size; }
  //!Returns a count of values initializing the vector.
  size_int_t get_init_expr_count() const
   { return (info_var.init.field ? (*info_var.init.field).size() : 0); }
  //!Returns a pointer to the vector of initial values.
  std::vector<dve_expression_t *> * get_init_expr_field()
   { return info_var.init.field; }
  //!Creates a new field of initial values (be aware: old one should be
  //! deleted first)
  void create_init_expr_field()
   { info_var.init.field = new std::vector<dve_expression_t *>; }
  //!Tells that array has no inital values.
  /*!Sets a internal pointer to is field of initial values to zero.
   * Thus tells, that there are no inital values for the field
   */
  void no_init_expr_field() { info_var.init.field = 0; }
  //!Returns a pointer to the i-th initial value
  const dve_expression_t * get_init_expr(const size_int_t i) const
   { return (*info_var.init.field)[i]; }
  //!Sets `i'-th initial value from inital expression field
  void set_init_expr(const size_int_t i, dve_expression_t * const trans)
   { (*info_var.init.field)[i] = trans; }
  //@}
  /*! @name Following functions can be called only for channel symbols */
  //@{
  //!Returns a number of items transmittable through a channel simultaneously
  /*!For typed channels it is the same as get_channel_type_list_size()
   *
   * \return initially CHANNEL_USUSED, until the number of items is set */
  size_int_t get_channel_item_count() const
  { if (get_channel_typed()) return get_channel_type_list_size();
    else return channel_item_count; }
  //!Sets a number of items transmittable through a channel simultaneously
  /*!For typed channels it is the same as set_channel_type_list_size()*/
  void set_channel_item_count(const size_int_t size)
  { if (get_channel_typed()) set_channel_type_list_size(size);
    else channel_item_count = size; }
  //!Returns true iff the channel is a typed channel
  bool get_channel_typed() const { return channel_typed; }
  //!Sets, whether channel is typed (`typed'==true) or not (`typed'==false)
  void set_channel_typed(const bool typed) { channel_typed = typed; }
  //!Returns a count of types of values transmitted simultaneously through
  //! a typed channel
  size_int_t get_channel_type_list_size() const
  { return channel_type_list.size(); }
  //!Sets a count of types of values transmitted simultaneously through
  //! a typed channel
  void set_channel_type_list_size(const size_int_t size)
  { channel_type_list.resize(size); }
  //!Returns a type of `index'-th element of data sent simultaneously
  //! through a typed channel
  dve_var_type_t get_channel_type_list_item(const size_int_t index) const
  { return channel_type_list[index]; }
  //!Sets a type of `index'-th element of data sent simultaneously
  //! through a typed channel
  void set_channel_type_list_item(const size_int_t index,
                                  const dve_var_type_t type_item)
  { channel_type_list[index] = type_item; }
  //!Returns a size of a buffer of typed channel
  slong_int_t get_channel_buffer_size() const
  { return channel_buffer_size; }
  //!Sets a size of a buffer of typed channel
  void set_channel_buffer_size(const slong_int_t buffer_size)
  { channel_buffer_size = buffer_size; }
  //@}
 };
  
#ifndef DOXYGEN_PROCESSING
} //end of namespace
#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif

