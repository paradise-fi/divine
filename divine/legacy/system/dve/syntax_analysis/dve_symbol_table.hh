#ifndef DVE_SYMBOL_TABLE_HH
#define DVE_SYMBOL_TABLE_HH
/*!\file dve_symbol_table.hh
 * The main contribution of this file is a definition of class dve_symbol_table_t.
 */

#ifndef DOXYGEN_PROCESSING
#include <vector>
#include <list>
#include <stack>
#include <cstdio>
#include "system/dve/syntax_analysis/dve_symbol.hh"
#include "common/error.hh"
#include "common/array.hh"
#include "system/dve/syntax_analysis/dve_commonparse.hh"
#include "system/dve/syntax_analysis/dve_token_vector.hh"
#include "common/array.hh"
#include "common/deb.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
#endif //DOXYGEN_PROCESSING

//ERROR_STATE//const char * const TAB_ERROR_TOKEN = "_error_";
class dve_symbol_t;

//!Class which stores the declaration of symbols (see dve_symbol_t).
class dve_symbol_table_t
 {
  protected:
  ////DATA:
  array_t<dve_symbol_t *> symbols;
  array_t<dve_symbol_t *> channel_symbs;
  array_t<dve_symbol_t *> variable_symbs;
  array_t<dve_symbol_t *> process_symbs;
  array_t<dve_symbol_t *> state_symbs;
  array_t<array_t<dve_symbol_t *> *> symbols_of_processes;
  array_t<std::size_t> processes_of_symbols;//projection: symbols -> frames
  array_t<dve_symbol_t *> global_symbols;
  dve_token_vector_t token_vector;
  error_vector_t & terr;
 
  public:
  ////METHODS:
  //!A constructor.
  dve_symbol_table_t(error_vector_t & evect): terr(evect) {}
  //!Returns, whether symbol exists.
  /*!Searches for a symbol in a selected process
   * \param name = name of symbol
   * \param proc_gid = \GID of process
   * \return true iff symbol was found */
  bool found_symbol(const char * name, const std::size_t proc_gid) const;
  
  //!Returns, whether there exists a global symbol of name `name'.
  /*!Searches for a symbol in the list of global symbols.
   * \param name = name of symbol
   * \returns true iff symbol of name \a name exists and it is a global symbol*/
  bool found_global_symbol(const char * name) const;
  
  //!Searches for a symbol in a given process.
  /*!\param name = name of symbol
   * \param proc_gid = \GID of process in which we look for a symbol
   * \returns \SID of symbol if the symbol exists in a process with \GID
   *          \a proc_gid, otherwise it returns NO_ID.
   */
  std::size_t find_symbol(const char * name, const std::size_t proc_gid) const;
  
  //!Searches for a symbol in the list of global symbols.
  /*!\param name = name of symbol
   * \returns \SID of symbol if the symbol exists in the list of global symbols,
   *          otherwise it returns NO_ID.
   */
  
  std::size_t find_global_symbol(const char * name) const;
  
  //!Searches for a symbol in a given process and in the list of global
  //! symbols.
  /*!This method searches for a symbol visible from a process with \GID
   * \a proc_gid. First it searches in the list of symbols of a given process
   * and only then it searches in the list of global symbols.
   * \param name = name of symbol
   * \param proc_gid = \GID of process in which we look for a symbol
   * \returns \SID of symbol if the symbol exists in a process or
   *          in the list of global symbols,
   *          otherwise it returns NO_ID.
   */
  std::size_t find_visible_symbol(const char * name,
                                  const std::size_t proc_gid) const;
 
  //!Returns the pointer on a symbol that has a given \SID
  dve_symbol_t * get_symbol(const std::size_t sid)
   { return symbols[sid]; }
  
  //!The same as get_symbol() above, but this is used in a contant instances
  //! of dve_symbol_table_t
  const dve_symbol_t * get_symbol(const std::size_t sid) const
   { return symbols[sid]; }

  //!Returns the count of all symbols.
  /*!Returns the count of all symbols stored in a \sym_table.
   * The set of symbols in a \sym_table should be the same as the union
   * of sets of all variables, channels, processes and process states
   * in the entire \sys that contains this \sym_table (if there is
   * such a \sys) */
  std::size_t get_symbol_count() const { return symbols.size(); } 
  //!Returns a count of all variables.
  /*!The set of variables in a \sym_table should be the same as
   * the set of variables in the entire \sys that contains this \sym_table
   * (if there is such a \sys) */
  std::size_t get_variable_count() const
    { return variable_symbs.size(); }
  //!Returns a count of all channels.
  /*!The set of channels in a \sym_table should be the same as
   * the set of channels in the entire \sys that contains this \sym_table
   * (if there is such a \sys) */
  std::size_t get_channel_count() const
    { return channel_symbs.size(); }
  //!Returns a count of all variables.
  /*!The set of processes in a \sym_table should be the same as
   * the set of processes in the entire \sys that contains this \sym_table
   * (if there is such a \sys) */
  std::size_t get_process_count() const
    { return process_symbs.size(); }
  //!Returns a count of all variables.
  /*!The set of process states in a \sym_table should be the same as
   * the set of process states in the entire \sys that contains this \sym_table
   * (if there is such a \sys) */
  std::size_t get_state_count() const
    { return state_symbs.size(); }
  //!Returns a pointer to the symbol of variable with \GID `gid'
  dve_symbol_t * get_variable(const std::size_t gid)
    { return variable_symbs[gid]; }
  //!Returns a pointer to the constant symbol of variable with \GID `gid'
  const dve_symbol_t * get_variable(const std::size_t gid) const
    { return variable_symbs[gid]; }
  //!Returns a pointer to the symbol of channel with \GID `gid'
  dve_symbol_t * get_channel(const std::size_t gid)
    { return channel_symbs[gid]; }
  //!Returns a pointer to the constant symbol of channel with \GID `gid'
  const dve_symbol_t * get_channel(const std::size_t gid) const
    { return channel_symbs[gid]; }
  //!Returns a pointer to the symbol of process with \GID `gid'
  dve_symbol_t * get_process(const std::size_t gid)
    { return process_symbs[gid]; }
  //!Returns a pointer to the constant symbol of process with \GID `gid'
  const dve_symbol_t * get_process(const std::size_t gid) const
    { return process_symbs[gid]; }
  //!Returns a pointer to the symbol of process state with \GID `gid'
  dve_symbol_t * get_state(const std::size_t gid)
    { return state_symbs[gid]; }
  //!Returns a pointer to the constant symbol of process state with \GID `gid'
  const dve_symbol_t * get_state(const std::size_t gid) const
    { return state_symbs[gid]; }

  //!Stores given string into the internal structure and returns the pointer
  //! to this (newly created) string.
  const char * save_token(const char * const token)
  { return token_vector.save_token(token); }
  
  //!Appends a symbol of channel to the list of symbols of channels
  void add_channel(dve_symbol_t * const symbol);
  //!Appends a symbol of process to the list of symbols of processes
  /*!\warning Important: If the \sym_table is the part of \sys, <b>DO
   * NOT USE THIS METHOD</b>. It would cause the inconsistencies.
   * To avoid it, please use dve_system_t::add_process(). */
  void add_process(dve_symbol_t * const symbol);
  //!Appends a symbol of variable to the list of symbols of variables
  void add_variable(dve_symbol_t * const symbol);
  //!Appends a symbol of state to the list of symbols of process states
  void add_state(dve_symbol_t * const symbol);

  ~dve_symbol_table_t();
 };

#ifndef DOXYGEN_PROCESSING
} //end of namespace
#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif

