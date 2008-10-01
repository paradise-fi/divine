/*!\file
 * The main contribution of this file is the abstact interface expression_t.
 */
#ifndef DIVINE_EXPRESSION_HH
#define DIVINE_EXPRESSION_HH

#ifndef DOXYGEN_PROCESSING

#include <string>
#include "common/types.hh"
#include "system/system.hh"

namespace divine { //We want Doxygen not to see namespace `dve'
#endif //DOXYGEN_PROCESSING

//predeclarations of classes:
class system_t;

//!Abstact interface of a class representing an expression
/*!Class expression_t represents expressions interpretable in a \sys
 * given in a constructor expression_t(system_t * const system)
 * of in a method set_parent_system(system_t & system)
 *
 * \note
 * Developer is responsible for correct setting of "parent" \sys
 * (but expressions created during reading of the system from a file
 *  have the "parent" \sys set correctly).
 */
class expression_t
{
 protected:
 system_t * parent_system; //<!Protected attribute storing "parent" \sys

 public:
 //!A constructor
 expression_t() { parent_system=0; }
 //!A constructor
 /*!\param system = "parent" \sys of this expression*/
 expression_t(system_t * const system) { parent_system=system; }
 //!A copy constructor
 expression_t(const expression_t & second)
 { parent_system = second.parent_system; }
 //!A destructor
 virtual ~expression_t() {};
 
 //!Returns a "parent" \sys of this expression
 system_t * get_parent_system() const { return parent_system; }
 //!Sets a "parent" \sys of this expression
 virtual void set_parent_system(system_t & system)
 { parent_system = &system; }
 
 //!Swaps the content of `expr' and this expression (time O(1)).
 virtual void swap(expression_t & expr)
 { system_t * aux_system = parent_system;
   parent_system = expr.parent_system;
   expr.parent_system = aux_system; }
        
         
 /* recurrently constructs a copy of an expression */
 //!Copies the contents of `expr' into this expression (time O(n)).
 virtual void assign(const expression_t & expr)
 { parent_system = expr.parent_system; }

 //!The assignment operator for expressions.
 /*!It copies the content of \a second (the right argument of an operator)
  * to the left argument of an operator.
  *
  * It is implemented simply by assign() method and you can also write
  * <code>left_argument.assign(right_argument)</code> instead of
  * <code>left_argument = right_argument</code>.
  *
  * \return The reference to this object (left argument of an operator after
  * an assignment)*/
 expression_t & operator=(const expression_t & second)
 { assign(second); return (*this); }

 /* methods for printing expressions */
 //!Returns string representation of the expression.
 /*!Implementation issue: This function is slower than write(), because
  * it does a copying of potentially long string */
 virtual std::string to_string() const = 0;
 //!Reads in the expression from a string representation
 /*!\param expr_str = string containing source of an expression
  * \param process_gid = context of a process (default value NO_ID = global
  *                      context)
  * \return ... 0 iff no error occurs, non-zero value in a case of error
  *             during a reading.
  */
 virtual int from_string(std::string & expr_str,
                         const size_int_t process_gid = NO_ID) = 0;
 //!Writes a string representation of the expression to stream
 virtual void write(std::ostream & ostr) const = 0;
 //!Reads in the expression from a string representation in stream
 /*!\param istr = input stream containing source of expression
  * \param process_gid = context of process (default value NO_ID = global
  *                      context)
  * \return ... 0 iff no error occurs, non-zero value in a case of error
  *             during a reading.
  */
 virtual int read(std::istream & istr, size_int_t process_gid = NO_ID) = 0;
};

#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE

#endif //DOXYGEN_PROCESSING

#endif

