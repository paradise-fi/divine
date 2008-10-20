/*!\file
 * Header file of a handling unit. This unit is independent of DiVinE and can be
 * used in an arbitrary project.
 *
 * Extern variable gerr is the global \evector that can be used
 * in arbitrary file that includes error.hh
 *
 * For more informations see \eunit page*/

/// Header file for error handling //
// Author: Pavel Simecek          //
// Last Update: 4.2. 2003         //

/* inferface for work with iternal error vector              */
/* NOTE: each error has its own ID of type ERR_id_t          */
/*       and each header file has its own range of error IDs */
/* RESERVED RANGE: 0-2000                                    */

#ifndef DIVINE_ERROR_HH
#define DIVINE_ERROR_HH

#ifndef DOXYGEN_PROCESSING
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include "common/types.hh"
#include "common/deb.hh"

#include <wibble/string.h>

namespace divine {
#endif //DOXYGEN_PROCESSING

//!String, which is field of characters finished by '\0'
typedef const char * ERR_char_string_t;
typedef const std::string ERR_std_string_t;
//!Integer type used for index of an error in an \evector
typedef unsigned int ERR_nbr_t;
//!Integer type used for identifiers of errors
typedef int ERR_id_t;
//!Integer type used for identifiers of errors
typedef int ERR_type_t;
//!Integer type used to throw or catch exceptions
//! with a parameter of ERR_throw_t type. This parametr is also called
//! error type
struct ERR_throw_t {
  ERR_type_t type;
  ERR_id_t id;
  ERR_throw_t(const ERR_type_t type_arg, const ERR_id_t id_arg):
    type(type_arg), id(id_arg) {}
};

//!Constant that is substituted instead of \error_ID, if you don't
//! specify any \error_ID to the function that has \error_ID as a
//! parameter.
const ERR_id_t ERR_UNKNOWN_ID = 1;
//!Constant that is substituted instead of of integer sent by throwing,
//! if you don't specify any \error_ID to the function that has
//! \error_ID as a parameter.
const ERR_type_t ERR_UNKNOWN_TYPE = 1;

class error_vector_t;

//!Callback function type (for handling of `<< psh()').
/*!Type of callback function for handling commands like 
 * \code terr << psh(); \endcode
 * where `terr' is an instance of error_vector_t.
 * 
 * We will call functions of this type 'warning handling callbacks'.
 */
typedef void (*ERR_psh_callback_t)(error_vector_t & terr, const ERR_throw_t);
//!Callback function type (for `<< thr()').
/*!Type of callback function for handling commands like 
 * \code terr << psh(); \endcode
 * where `terr' is an instance of error_vector_t.
 * 
 * We will call functions of this type 'error handling callback'.
 */
typedef void (*ERR_thr_callback_t)(error_vector_t & terr, const ERR_throw_t);

//!Default 'warning handling callback'
/*!This is a default 'warning handling callback' in error_vector_t.
 * 
 * To exchange this function by another function in an instance of
 * error_vector_t you can call error_vector_t::set_push_callback().
 * 
 * This default callback function has the following behaviour:
 * -# Prints the last kept message on the standard error output.
 * -# Erases printed message from a memory.
 */
void ERR_default_psh_callback(error_vector_t & terr, const ERR_throw_t);
//!Default 'error handling callback'
/*!This is a default 'error handling callback'.
 *
 * To exchange this function by another function in an instance of
 * error_vector_t you can call error_vector_t::set_thr_callback().
 * 
 * This default callback function has the following behaviour:
 * -# Flushes all messages stored in a memory to the stadard error output.
 * -# Clears the memory of messages.
 * -# Throws exception of type \ref ERR_throw_t (parameter \a error type).
 */
void ERR_default_thr_callback(error_vector_t & terr, const ERR_throw_t);

//!Class determined for storing error messages.
/*!Class determined for storing error messages (1 error message in 1 instance
 * of error_string_t).
 *
 * This class should not be explicitly used in usual programs.
 */
class error_string_t
{
 private:
 std::string * s; 
 static divine::size_int_t alloc_str_count;
 template<class T>
 //private template for appending string representation of number
 error_string_t & plus(const T i)
 {
     s->append(wibble::str::fmt( i ));
     return (*this);
 }
 public:
 static divine::size_int_t allocated_strings() { return alloc_str_count; }
 error_string_t() { recreate(); }
 void recreate () { s = new std::string; alloc_str_count++;};
 error_string_t(const error_string_t & s2) { s = s2.s; }
 error_string_t(error_string_t & s2) { s = s2.s; }
// ~error_string_t() { delete s; } //IT CANNOT DELETE ITS CONTENT!!! IT WOULD BE
//                             //DANGEROUS IN FURTHER USE!!!
 void delete_content() //SO DELETE HAS TO BE EXPLICIT!!!               
  { delete s; alloc_str_count--;
    DEB(std::cerr << "error_string_t object deallocated!" << std::endl;) }
 std::string & operator*() { return *s; }
 friend const std::string & operator*(const error_string_t & errstr)
   { return *errstr.s; }
 error_string_t & operator<<(const unsigned long int i)
   { return plus<unsigned long int>(i); }
 error_string_t & operator<<(const signed long int i)
   { return plus<signed long int>(i); }
 error_string_t & operator<<(const unsigned int i)
   { return plus<unsigned long int>(i); }
 error_string_t & operator<<(const int i)
   { return plus<signed long int>(i); }
 friend std::ostream & operator<<(std::ostream & ostr, const error_string_t s)
   { return(ostr << (*s)); }
 error_string_t & operator<<(const ERR_char_string_t second_str)
   { s->append(second_str); return (*this); }
 error_string_t & operator<<(const ERR_std_string_t & second_str)
   { s->append(second_str); return (*this); }
 error_string_t & operator<<(const error_string_t second_str)
   { s->append(*second_str); return (*this); }
 std::string * operator->() { return s; }
};

//!Structure determined for causing storing/printing error messages
// (and defaulty also for causing throwing exceptions - see
//  ERR_default_thr_callback())
/*!This structure should be used as follows:
 * \code terr << "My error mess" << "age" << thr(3,13565) \endcode
 * where "My error message" is an example of error message sent into
 * error_vector_t `terr', 3 is an example of so called error type
 * (see \ref ERR_type_t) and 13565 is an example of \error_ID.
 *
 * Sending thr() to an instance of error_vector_t through an operator `<<'
 * causes the end of assigning of a message. Message is then stored at
 * the end of a list of error messages. Then
 * 'error handling callback' is called (callback that can be set by
 * error_vector_t::set_throw_callback()).
 */
struct thr
 { ERR_id_t c; ERR_type_t t;
   thr(ERR_type_t tt = ERR_UNKNOWN_TYPE, ERR_id_t i = ERR_UNKNOWN_ID):
     c(i),t(tt) {}
 };

//!Structure determined for causing storing/printing error messages
/*!This structure should be used as follows:
 * \code terr << "My warning mess" << "age" << psh(3,13565) \endcode
 * where "My warning message" is an example of error message sent into
 * error_vector_t `terr', 3 is an example of so called error type
 * (see \ref ERR_type_t) and 13565 is an example of \error_ID.
 *
 * Sending psh() to an instance of error_vector_t through an operator `<<'
 * causes the end of assigning of a message. Message is then stored at
 * the end of a list of error messages. Then
 * 'warning handling callback' is called (callback that can be set by
 * error_vector_t::set_push_callback()).
 */
struct psh
 { ERR_id_t c; ERR_type_t t;
   psh(ERR_type_t tt = ERR_UNKNOWN_TYPE, ERR_id_t i = ERR_UNKNOWN_ID):
     c(i),t(tt) {}
 };

//!Structure for storing (usualy constant) error messages in a standardized
//! form.
/*!You can use this structure if you want to store a complete error message
 * including its \error_ID and type.
 */
struct ERR_triplet_t {
 const ERR_char_string_t message;
 const ERR_id_t id;
 const ERR_type_t type;
 ERR_triplet_t(const ERR_char_string_t mes,
               const ERR_type_t tt = ERR_UNKNOWN_TYPE,
	       const ERR_id_t num = ERR_UNKNOWN_ID):
    message(mes), id(num), type(tt) {};
};

//!Definition of constant ERR_triplet_t
typedef const ERR_triplet_t ERR_c_triplet_t; 

//!The main class in \ref error.hh determined for storing
// and handling errors.
/*!This is the main class in \ref error.hh. It is determined to store and
 * handle all errors and warnings in your program.
 * It is a replacement of standard
 * error handling through throw & catch statements, but it defaultly uses
 * throw & catch constructs
 * to jump out of the problematic piece of a program code
 * (that caused a possible error).
 *
 * The main advantage of this class is a user friendly interface formed
 * especially by `<<' operators, which permits to work with error_vector_t
 * similarly as with \a ostream class.
 *
 * The another advantage and the reason why this is a vector instead of
 * single error buffer is that we distinguish between non-fatal errors
 * (we call them 'warnings') and fatal errors (we call them 'errors').
 *
 * The behaviour in a case of storing of error or warning can be selected
 * or changed by functions set_push_callback() and set_throw_callback().
 *
 * For more informations about usage see \eunit.
 */
class error_vector_t
{
 private:
  
  struct Pair {
   error_string_t message;
   ERR_id_t id;
   Pair(error_string_t mes, const ERR_id_t num):
      message(mes), id(num) {};
  };
    
  typedef std::vector<Pair> vector_t;
  
  vector_t err_vector;
  error_string_t string_err;
  ERR_thr_callback_t throw_callback;
  ERR_psh_callback_t push_callback;
  bool is_silent;
  
 public:
  //!A constructor.
  /*!Only initializes callback functions serving creation of error messages
   * private attributes \a throw_callback and \a push_callback.*/
  error_vector_t(): throw_callback(ERR_default_thr_callback),
                    push_callback(ERR_default_psh_callback),
                    is_silent(false) {};
  //!A destructor.
  /*!Only do some deallocation of dynamically allocated memory (in
   * instances of error_string_t)*/
  /* it is necessary to delete content of auxiliary string */
  ~error_vector_t() { DEBAUX(std::cerr << "BEGIN of destructor of error_vector_t" << std::endl;) string_err.delete_content(); clear(); DEBAUX(std::cerr << "END of destructor of error_vector_t" << std::endl;) }
 
  //!Sets a 'warning handling callback'.
  void set_push_callback(const ERR_psh_callback_t func);
  //!Sets a 'error handling callback'.
  void set_throw_callback(const ERR_thr_callback_t func);
 
  /* !!! mes is emptied, but NOT DELETED !!! */
  //!Serves to store standardized error messages
  //! (in triplets \ref ERR_triplet_t).
  /*!
   * \code terr.that(triplet)\endcode
   * is functionally equivalent to
   * \code terr << triplet.message << thr(triplet.type, triplet.id) \endcode
   */
  void that(const ERR_c_triplet_t & st);
  
  /* !!! mes is emptied, but NOT DELETED !!! */
  //!Serves to store standardized error messages given by elements
  /*!\code terr.that(message, id, type)\endcode
   * is functionally equivalent to
   * \code terr << message << thr(type, id) \endcode
   * Note: Parameters \a err_type and \a id are not obligatory.
   */
  void that(error_string_t & mes, const ERR_type_t err_type = ERR_UNKNOWN_TYPE,
            const ERR_id_t id = ERR_UNKNOWN_ID);
  
  //!Inserts an error message to the end of a list of errors.
  /*!Inserts an error message to the end of a list of errors. It does not call
   * any of callback functions (= any of warning/error handling callbacks).
   *
   * Note: parameter \a id is optional.
   */
  void push(error_string_t& mes, const ERR_id_t id = ERR_UNKNOWN_ID);
  
  //!Removes last n errors.
  /*!Removes n error messages from the end of the list of errors.
   * If \a n = 0, then removes all errors from a memory.
   */
  void pop_back(const ERR_nbr_t n);
  
  //!Removes first n errors.
  /*!Removes n error messages from the begining of the list of errors.
   * If \a n = 0, then removes all errors from a memory.
   */
  void pop_front(const ERR_nbr_t n);

  //!Removes 1 error message from the end of the list of errors.
  void pop_back();
  
  //!Removes 1 error message from the beginning of the list of errors.
  void pop_front();
  
  //!Removes `index'-th error
  /*!Removes \a index-th error message from the beginning of the list of
   * errors
   */
  void pop(const ERR_nbr_t index);

  //!Removes interval of errors
  /*!Removes all error messages with ordering number
   * from \a begin (inclusive) to \a end (exclusive) in a list of errors.
   */
  void pop(const ERR_nbr_t begin, const ERR_nbr_t end);
  
  //!Removes all error messages from a memory.
  void clear() { pop_back(0); } 
  
  //!Flushes all stored error messages.
  /*!Prints all error messages stored in a memory to the standard error
   * output and calls clear() to empty the memory.
   */
  void flush();
  
  //!Returns whether there exists any error message in a memory
  bool empty() //is there any error? (in the vector)
  { return bool(err_vector.size()); }
  
  //!Returns a number of error messages in the vector
  ERR_nbr_t count() 
  { return err_vector.size(); }
  
  //!Prints \a mes to the standard error output.
  void print(ERR_char_string_t mes);
  
  //!Prints \a i-th message (from the beginning of the list of errors)
  //! to the standard error output
  void perror(const ERR_nbr_t i);
  
  //!Prints the first message from the beginning of the list of errors
  //! to the standard error output
  void perror_front();

  //!Prints the first message from the end of the list of errors
  //! to the standard error output
  void perror_back();
  
  //!Prints \a mes and \a i-th message
  //! (from the beginning of the list of errors) to the standard error output
  void perror(ERR_char_string_t mes, const ERR_nbr_t i);
  
  //!Prints \a mes and the first message from the beginning of the list of
  //! errors to the standard error output
  void perror_front(ERR_char_string_t mes);

  //!Prints \a mes and the first message from the end of the list of errors
  //! to the standard error output
  void perror_back(ERR_char_string_t mes);
  
  //!Returns the string of the error message from the end of the list of errors
  const error_string_t string_back();

  //!Returns the string of the error message from the beginning of the list
  //! of errors
  const error_string_t string_front();
  
  //!Returns the string of \a i-th error message from the beginning of the list
  //! of errors
  const error_string_t string(const ERR_nbr_t i);
  
  //!Returns \error_ID of the error message from the end of the list of errors
  ERR_id_t id_back();
  
  //!Returns \error_ID of the error message from the beginning of the list
  //! of errors
  ERR_id_t id_front();
  
  //!Returns \error_ID of \a i-th error message from the beginning of the list
  //! of errors
  ERR_id_t id(const ERR_nbr_t i);
 
  //!Finishes creating of an error message and calls 'error handling callback'
  /*!Finishes creating of an error message and inserts prepared error message
   * to the end of the list of errors. Then calls 'error handling callback'
   * (see \ref ERR_thr_callback_t)
   */
  void operator<<(const thr & e);
  //!Finishes creating of an error message and calls 'warning handling callback'
  /*!Finishes creating of an error message and inserts prepared error message
   * to the end of the list of errors. Then calls 'warning handling callback'
   * (see \ref ERR_psh_callback_t)
   */
  void operator<<(const psh & e);
  
  //!Appends \a second_str to the error message creating in the \evector
  error_vector_t & operator<<(const ERR_char_string_t second_str)
    { string_err<<second_str; return (*this); }
  
  //!Appends \a second_str to the error message creating in the \evector
  error_vector_t & operator<<(const ERR_std_string_t & second_str)
    { string_err<<second_str; return (*this); }
  
  //!Appends \a second_str to the error message creating in the \evector
  error_vector_t & operator<<(const error_string_t second_str)
    { string_err<<second_str; return (*this); }
  
  //!Appends string representing \a i to the error message creating in
  //! the \evector
  error_vector_t & operator<<(const unsigned long int i)
  {string_err<<i; return (*this);}
  
  //!Appends string representing \a i to the error message creating in
  //! the \evector
  error_vector_t & operator<<(const signed long int i)
  { string_err<<i; return (*this); }
  
  //!Appends string representing \a i to the error message creating in
  //! the \evector
  error_vector_t & operator<<(const unsigned int i)
  { string_err<<i; return (*this); }
  
  //!Appends string representing \a i to the error message creating in
  //! the \evector
  error_vector_t & operator<<(const int i)
  { string_err<<i; return (*this); }
  
  //!Sets, whether it can write messages to STDERR (true) or not (false)
  void set_silent(const bool be_silent) { is_silent = be_silent; }

  //!Returns, whether it can write messages to STDERR (true) or not (false)
  bool get_silent() const { return is_silent; }
};

//!Global \evector `gerr'
/*!Global \evector \a gerr. You can use it in your programs, but you can
 * also declare your own \evector and use it instead of \a gerr.
 * This global object is proper because of functions which are not wrapped
 * in a structure or class with own \evector.*/
extern error_vector_t gerr;

#ifndef DOXYGEN_PROCESSING
}; // end of namespace
#include "common/undeb.hh"
#endif //DOXYGEN_PROCESSING

#endif

