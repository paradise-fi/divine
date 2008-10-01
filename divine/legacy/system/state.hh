/*!\file
 * The main contribution of this file is a class state_t and methods
 * for the work with it (especially new_state(), delete_state(),
 * state_pos_to_*() functions *_to_state_pos() functions and functions
 * for comparing states. */
#ifndef DIVINE_STATE_HH
#define DIVINE_STATE_HH

#ifndef DOXYGEN_PROCESSING
#include <iostream>
#include "common/types.hh"

namespace divine
{
#endif //DOXYGEN_PROCESSING
 //!Structure representing the state of the system.
 /*!There are many functions that can work with states. The list and the
  * description of them can be found in a manual page of file
  * \ref state.hh */
 struct state_t
  {
   state_t(); //!<A constructor (only sets ptr to 0).
   char *ptr; //!<Pointer to the piece of memory, where the state is stored in
   std::size_t size; //!<Variable that stores the size of the state
  };
 
 //THE MOST IMPORTANT OPERATIONS WITH STATES:
 //!Returns whether state `arg1' is smaller than `arg2' using function memcmp().
 bool operator< (const state_t& arg1, const state_t& arg2);
 //!Returns whether state `arg1' is larger than `arg2' using function memcmp().
 bool operator> (const state_t& arg1, const state_t& arg2);
 //!Returns whether state `arg1' is different from `arg2' using function memcmp().
 bool operator!= (const state_t& arg1, const state_t& arg2);
 //!Returns whether state `arg1' is the same as `arg2' using function memcmp().
 bool operator== (const state_t& arg1, const state_t& arg2);
 
 //!Creates a copy of state `state' and returns a pointer to it.
 state_t duplicate_state(state_t state);
 //!Fills a memory representing the state with zeros.
 void clear_state(state_t);
 //!Creates a new state and returns a pointer to it.
 state_t new_state(const std::size_t size);
 //!Creates a new state and returns a pointer to it.
 /*!\param state_memory = pointer to the memory representing a state of the
  *                       system - the content will be copied to the
  *                       
  * \param size = size of the memory referenced by `state_memory' in bytes
  */
 state_t new_state(char * const state_memory, const std::size_t size);
 //!Deletes the state
 void delete_state(state_t& state);

 //! Realloc state. Exisiting data blosk is deleted, and replpaced with a new one of given size.
 void realloc_state(state_t &state, size_t new_size); 
 
 /* FUNCTIONS for reading a part of state */
 inline byte_t state_pos_to_byte(const state_t state,
                                  const std::size_t pos)
 { return ((byte_t *)(state.ptr))[pos]; }
 
 inline sshort_int_t state_pos_to_int(const state_t state,
                                const std::size_t pos)
 {
  //fisrt it's addressed and dereferenced as byte, then we gain its address
  //and convert it to int address and then we dereferece it
  return (*((sshort_int_t *)(&((byte_t *)(state.ptr))[pos])));
 }
 
 inline ushort_int_t state_pos_to_uint(const state_t state,
                                  const std::size_t pos)
 {
  //fisrt it's addressed and dereferenced as byte, then we gain its address
  //and convert it to int address and then we dereferece it
  return (*((ushort_int_t *)(&((byte_t *)(state.ptr))[pos])));
 }
 
 inline ulong_int_t state_pos_to_ulong_int(const state_t state,
                                const std::size_t pos)
 { return (*((ulong_int_t *)(&((byte_t *)(state.ptr))[pos]))); }
 
 /* FUNCTIONS for writing a part fo state */
 
 inline void byte_to_state_pos(const state_t state, const std::size_t pos,
                             const byte_t value)
 { ((byte_t *)(state.ptr))[pos] = value; }
 
 inline void int_to_state_pos(const state_t state, const std::size_t pos,
                            const sshort_int_t value)
 { (*((sshort_int_t *)(&((byte_t *)(state.ptr))[pos]))) = value; }
 
 inline void uint_to_state_pos(const state_t state, const std::size_t pos,
                             const ushort_int_t value)
 { (*((ushort_int_t *)(&((byte_t *)(state.ptr))[pos]))) = value; }
 
 inline void ulong_int_to_state_pos(const state_t state,
       const std::size_t pos, const ulong_int_t value)
 { (*((ulong_int_t *)(&((byte_t *)(state.ptr))[pos]))) = value; }
 

 /* TEMPLATE FUNCTIONS for more general access to the functions declared
  * above (types in names of functions are replaced by types which are
  * parameters of templates */
 
 //!Returns the value of type `T' stored in `state' at the position `pos'
 /*!Returns the value of type \a T stored in \a state at the position \a pos
  * It can be instatiated by the following types \a T :
  * - byte_t
  * - sshort_int_t
  * - ushort_int_t
  * - ulong_int_t
  */
 template<class T>
 inline T state_pos_to(const state_t state, const std::size_t pos);
 
 template <>
 inline byte_t state_pos_to<byte_t>(const state_t state,
                                        const std::size_t pos)
 { return state_pos_to_byte(state,pos); }
 
 template <>
 inline sshort_int_t state_pos_to<sshort_int_t>(const state_t state,
                                      const std::size_t pos)
 { return state_pos_to_int(state,pos); }
 
 template <>
 inline ushort_int_t state_pos_to<ushort_int_t>(const state_t state,
                                        const std::size_t pos)
 { return state_pos_to_uint(state,pos); }
 
 template <>
 inline ulong_int_t state_pos_to<ulong_int_t>(const state_t state,
                                                  const std::size_t pos)
 { return state_pos_to_ulong_int(state,pos); }
 
 //!Sets the value of type `T' to `state' at the position `pos'
 /*!Sets the value of type \a T to \a state at position \a pos.
  * It can be instatiated by the following types \a T :
  * - byte_t
  * - sshort_int_t
  * - ushort_int_t
  * - ulong_int_t
  */
 template<class T> void set_to_state_pos(const state_t state,
                                         const std::size_t pos, const T value);
 
 template <>
 inline void set_to_state_pos<byte_t>(const state_t state,
                                 const std::size_t pos, const byte_t value)
 { return byte_to_state_pos(state,pos,value); }
 
 template <>
 inline void set_to_state_pos<sshort_int_t>(const state_t state,
                                 const std::size_t pos, const sshort_int_t value)
 { return int_to_state_pos(state,pos,value); }
 
 template <>
 inline void set_to_state_pos<ushort_int_t>(const state_t state,
                                 const std::size_t pos, const ushort_int_t value)
 { return uint_to_state_pos(state,pos,value); }
 
 template <>
 inline void set_to_state_pos<ulong_int_t>(const state_t state,
                              const std::size_t pos, const ulong_int_t value)
 { return ulong_int_to_state_pos(state,pos,value); }
 

#ifndef DOXYGEN_PROCESSING
} //end of namespace
#endif //DOXYGEN_PROCESSING

#endif
