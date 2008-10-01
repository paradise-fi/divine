/*!\file
 * The main contribution of this file is the class bymoc_expression_t.
 */
#ifndef DIVINE_BYMOC_EXPRESSION_HH
#define DIVINE_BYMOC_EXPRESSION_HH

#ifndef DOXYGEN_PROCESSING
#include <iostream>
#include <string>
#include <stack>
#include "system/expression.hh"
#include "common/error.hh"
#include "common/types.hh"
#include "common/array.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

class bymoc_system_t;

//!Class representing an expression in BYMOC \sys
/*!BYMOC \sys does not support expressions. This class is here only
 * for abstract interface compatibility reasons.
 */
class bymoc_expression_t: public expression_t
 {
 public:
  ///// IMPLEMENTATION OF VIRTUAL INTERFACE OF expression_t /////
  //!Not implemented in BYMOC \sys - throws error message
  virtual void swap(expression_t & expr);
  //!Not implemented in BYMOC \sys - throws error message
  virtual void assign(const expression_t & expr);

  /* methods for printing expressions */
  //!Not implemented in BYMOC \sys - throws error message
  virtual std::string to_string() const;
  //!Not implemented in BYMOC \sys - throws error message
  virtual int from_string(std::string & expr_str,
                  const size_int_t process_gid = NO_ID);
  //!Not implemented in BYMOC \sys - throws error message
  virtual void write(std::ostream & ostr) const;
  //!Not implemented in BYMOC \sys - throws error message
  virtual int read(std::istream & istr, size_int_t process_gid = NO_ID);
  //!Not implemented in BYMOC \sys - throws error message
  virtual ~bymoc_expression_t();
  
  /* constructors and destructor */
  bymoc_expression_t(system_t * const system = 0): expression_t(system) { }
 };

#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE

#endif //DOXYGEN_PROCESSING

#endif

