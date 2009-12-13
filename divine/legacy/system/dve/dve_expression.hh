/*!\file
 * The main contribution of this file is the class dve_expression_t.
 */
#ifndef DIVINE_DVE_EXPRESSION_HH
#define DIVINE_DVE_EXPRESSION_HH

#ifndef DOXYGEN_PROCESSING
#include <iostream>
#include <string>
#include <stack>
#include "system/expression.hh"
#include "common/error.hh"
#include "common/types.hh"
#include "common/array.hh"
#include "system/dve/syntax_analysis/dve_commonparse.hh"
#include "system/dve/syntax_analysis/dve_symbol_table.hh"
#include "system/dve/dve_source_position.hh"
#include "common/deb.hh"

//#define LOCAL_DEB

namespace divine { //We want Doxygen not to see namespace `dve'
using std::cerr; using std::endl;
#endif //DOXYGEN_PROCESSING

class dve_symbol_table_t;
class dve_parser_t;
class dve_system_t;

const size_int_t DVE_MAXIMAL_ARITY = 2;


//! Structure of a single compacted expression. In the memory block, this initial structure is followed by the left subexpression (if present), and then by right subexpression (if present). r_offset is an offset to the right subexpression.
struct compacted_viewer_t
{
  int size; 
  int op;
  int arity;
  int r_offset;
};

  //!Class to access compacted expressions
  /*!This class is to view compacted expressions. It uses compacted_viewer_t to
   view the memory block pointed by the member pointer.  */
struct compacted_t {
  //Pointer to a memory block containing the compacted expression.
  mutable compacted_viewer_t * ptr;
    
  //!To string
  /*! Prints compacted expression to string. */
  std::string to_string();
  
  //!Creates a compacted representation of a unary leaf in the syntax tree
  /*!Creates memory block that keeps, in a compacted way, a leaf of type T_NAT
    of the tree of subexpressions.*/
  void create_val(int _op, all_values_t _value) const;
  
  //!Constructor -- Creates s compacted representation of a unary leaf in the syntax tree
  /*!Creates memory block that keeps, in a compacted way, a leaf of type different from T_NAT
    of the tree of subexpressions.*/
  void create_gid(int _op, size_int_t _gid) const;
  
  //!Contructor -- Joins to compacted expression into one with given operator.
  /*!Creates memory block that keeps, concatenation of given subexpression that
    is preceded with the connecting connective in a compacted way.  of the tree
    of subexpressions.*/
  void join(int _op, compacted_viewer_t* _left, compacted_viewer_t* _right) const;
  
  //!Returns pointer to the last subexpression
  /*!Returns pointer to the last subexpression in a given compacted subexpression.*/
  compacted_viewer_t* last() const
  {
    if (ptr->arity > 1)
      {
	//return right();
	return reinterpret_cast<compacted_viewer_t*>((char*)ptr + ptr->r_offset);
      }
    else
      {
	//return left();
	return reinterpret_cast<compacted_viewer_t*>((char*)ptr+sizeof(compacted_viewer_t));  
      }
  }

  //!Returns pointer to the first subexpression
  /*!Returns pointer to the first subexpression in a given compacted subexpression.*/
  compacted_viewer_t* first() const
  {
    return reinterpret_cast<compacted_viewer_t*>((char*)ptr+sizeof(compacted_viewer_t));  
  }

  //!Returns pointers to left subexpression
  /*!Returns pointer to the left subexpression in a given compacted subexpression.*/
  compacted_viewer_t* left() const
  {
    //if arity==0 then left() points to the value stored inside expression
    return reinterpret_cast<compacted_viewer_t*>((char*)ptr+sizeof(compacted_viewer_t));  
  }
  
  //!Returns pointers to right subexpression
  /*!Returns pointer to the right subexpression in a given compacted subexpression.*/
  compacted_viewer_t* right() const
  {
    return reinterpret_cast<compacted_viewer_t*>((char*)ptr+ptr->r_offset);
  }
  
  //!Returns arity
  /*!Returns arity of the compacted expression.*/
  int get_arity() const
  {
    return ptr->arity;
  }
    
  //!Returns operator
  /*!Returns operator of the compacted expression.*/
  int get_operator() const
  {
    return ptr->op;
  }
    
  //!Returns value of a T_NAT leaf
  /*!Returns value of a T_NAT leaf in compacted expression.*/
  all_values_t get_value() const
  {
    return *((all_values_t*)((char*)ptr + sizeof(compacted_viewer_t)));
  }
  
  //!Returns gid of stored in T_ID, T_SQUARE_BRACKET, or T_DOT leaf
  /*!Returns gid of stored in T_ID, T_SQUARE_BRACKET, or T_DOT leaf in compacted expression.*/
  size_int_t get_gid() const
  {
    return *((size_int_t*)((char*)ptr + sizeof(compacted_viewer_t)));
  }

  compacted_t(): ptr(NULL) {}
};


//!Class representing an expression in DVE \sys.
/*!This class implements the abstract interface of expression_t and
 * moreover it provides many methods beside methods of that interface.
 * 
 * Expressions are stored in a structure of a syntax tree. The methods
 * of class dve_expression_t enables to obtain or change the informations
 * about the top-most node in a syntax tree of the expression. To edit
 * the lower levels of the syntax tree you can use methods left(), right()
 * and subexpr().
 *
 * The syntax tree is little different than you are probably used to.
 * The inner nodes may contain following operators:
 * - ()  ... T_PARENTHESIS ... parenthesis,
 * - []  ... T_SQUARE_BRACKETS ... array indexing,
 * - <   ... T_LT,
 * - <=  ... T_LEQ,
 * - =   ... T_EQ,
 * - !=  ... T_NEQ,
 * - >=  ... T_GEQ,
 * - >   ... T_GT,
 * - +   ... T_PLUS,
 * - -   ... T_MINUS,
 * - *   ... T_MULT,
 * - /   ... T_DIV,
 * - %   ... T_MOD,
 * - &   ... T_AND,
 * - |   ... T_OR,
 * - ^   ... T_XOR,
 * - <<  ... T_LSHIFT,
 * - >>  ... T_RSHIFT,
 * - and ... T_BOOL_AND,
 * - or  ... T_BOOL_OR,
 * - ->  ... I_IMPLY,
 * - - ... T_UNARY_MINUS ... unary minus
 * - ~ ... T_TILDE ... bitwise negation
 * - not ... T_BOOL_NOT ... logical negation
 * - = ... T_ASSIGN ... assignment operation (permitted only in effects)
 *
 * The leaf nodes may contain following operators:
 * - . ... T_DOT ... question on a state of process
 * - <number> ... T_NAT ... integer constant
 * - <variable> ... T_ID ... variable
 *
 * \note In a T_DOT node you can use get_ident_gid() method to get a \GID
 * of process state contained in an expression. The identifier of process
 * which owns that process state can be obtained by the function
 * symbol_t::get_process_gid(), e.g.
 * <code>table->get_state(expr->get_ident_gid())->get_process_gid()</code>.
 */
class dve_expression_t: public expression_t, public dve_source_position_t
{
private:
  static dve_parser_t expr_parser;
  static error_vector_t * pexpr_terr;

  /* items for semantics of expression: */
  int op;  //operator -- odpovida symbol_t::type (jen pokud je to operator)
  union {
    all_values_t value; //for numeric constant
   size_int_t symbol_gid; //GID of symbol
  } contain;
  array_t<dve_expression_t> subexprs; //constant list of refs to subexpr.

  compacted_viewer_t *p_compact;
  
public:
  ///// IMPLEMENTATION OF VIRTUAL INTERFACE OF expression_t /////
  //!A destructor.
  virtual ~dve_expression_t();
  //!Implements expression_t::swap() in DVE \sys
  virtual void swap(expression_t & expr);
  //!Implements expression_t::assign() in DVE \sys
  virtual void assign(const expression_t & expr);
  
  //!Implements expression_t::to_string() in DVE \sys
  virtual std::string to_string() const;
  //!Implements expression_t::from_string() in DVE \sys
  virtual int from_string(std::string & expr_str,
                  const size_int_t process_gid = NO_ID);
  //!Implements expression_t::write() in DVE \sys
  virtual void write(std::ostream & ostr) const;
  //!Implements expression_t::read() in DVE \sys
  virtual int read(std::istream & istr, size_int_t process_gid = NO_ID);

  
  /* constructors */
  //!A contructor of empty expression.
  dve_expression_t(system_t * const system = 0):
    expression_t(system), op(0), subexprs(0,DVE_MAXIMAL_ARITY), p_compact(0)
  { DEBFUNC(cerr << "Constructor 1 of dve_expression_t" << endl;) }
  //!A constructor used by a parser.
  dve_expression_t(int expr_op, std::stack<dve_expression_t *> & subexprs_stack, const short int ar = 0, system_t * const system = 0);
  //!A constructor that creates a new expression from 2 expressions and an
  //! operator.
  dve_expression_t(int expr_op, 
               const dve_expression_t & expr_left, const dve_expression_t & expr_right,
               system_t * const system = 0):
    expression_t(system), subexprs(2,DVE_MAXIMAL_ARITY)
   { DEBFUNC(cerr << "Constructor 3 of dve_expression_t" << endl;)
     subexprs.extend(2);
     op = expr_op; left()->assign(expr_left); right()->assign(expr_right);
     p_compact=0;
   }
  //!A constructor that creates a new expression from 1 expression and an
  //! operator.
  dve_expression_t(int expr_op, 
               const dve_expression_t & expr_left,
               system_t * const system = 0):
    expression_t(system), subexprs(2,DVE_MAXIMAL_ARITY), p_compact(0)
   { DEBFUNC(cerr << "Constructor 4 of dve_expression_t" << endl;)
     subexprs.extend(1);
     op = expr_op; left()->assign(expr_left);
     p_compact=0;
   }
  //!A constructor from a string
  /*!\param str_expr = string assigning the expression to represent
   * \param par_table = symbol table containing symbols needed to
   *                    recognize in the expression (e.g. variable names)
   * \param process_gid = \GID of process in which the expression
   *                      is given (NO_ID means no process, that is
   *                      a global environment)
   */
  dve_expression_t(const std::string & str_expr,
               system_t * const system,
               size_int_t process_gid = NO_ID);
  //!A copy constructor.
  dve_expression_t(const dve_expression_t & expr):
    expression_t(expr),dve_source_position_t(),subexprs(expr.arity(),DVE_MAXIMAL_ARITY)
   {
    DEBFUNC(cerr << "Constructor 5 of dve_expression_t" << endl;)
    assign(expr);
   }

  /*! @name DVE system specific methods
   *  These methods are implemented only in DVE system and they
   *  cannot be run using an abstract interface of expression_t
   @{*/

   

  /* methods for informating about this subexpression */
   
  //! Computes compatc version of the expression (single memory block)
  void compaction();
  //! Says whether the expression is compacted 
  bool is_compacted() const { return (p_compact != 0); }

  //!Returns pointer to compacted representation
  compacted_viewer_t *get_p_compact() const { return (p_compact); };

  //!Returns an operator in the top-most node of the syntax tree of
  //! the expression.
  int get_operator() const { return op; }
  //!Sets an operator in the top-most node of the syntax tree of
  //! the expression.
  void set_operator(const int oper) { op = oper; }
  //!Returns a value contained in the top-most node of the syntax tree of
  //! the expression. It can be used only if operator in this node is T_NAT
  //! (number).
  all_values_t get_value() const { return contain.value; }
  //!Sets a value contained in the top-most node of the syntax tree of
  //! the expression. It can be used only if operator in this node is T_NAT
  //! (number).
  void set_value(const all_values_t value) { contain.value=value; }
  //!Returns an identifier contained in the top-most node of the
  //! syntax tree of the expression.
  /*!It can be used only if operator in this
   * node is T_ID (scalar variable), T_SQUARE_BRACKETS (vector variable)
   * or T_DOT (process state).
   *
   * It returns \GID of the object of a certain type - the type is given
   * by get_operator() (values T_ID, T_SQUARE_BRACKETS, T_DOT).
   */
  size_int_t get_ident_gid() const { return contain.symbol_gid; }
  //!Sets an identifier contained in the top-most node of the
  //! syntax tree of the expression.
  /*!It can be used only if operator in this
   * node is T_ID (scalar variable), T_SQUARE_BRACKETS (vector variable)
   * or T_DOT (process state).
   *
   * It returns \GID of the object of a certain type - the type is given
   * by get_operator() (values T_ID, T_SQUARE_BRACKETS, T_DOT).
   */
  void set_ident_gid(const size_int_t gid) { contain.symbol_gid=gid; }
  //!Returns a number of chilren of the top-most node in the syntax tree
  //! of the expression.
  size_int_t arity() const { return subexprs.size(); }
  //!Sets a number of chilren of the top-most node in the syntax tree
  //! of the expression.
  void set_arity(const size_int_t new_arity) { subexprs.resize(new_arity); }
  
  //!Returns a pointer to the \sym_table corresponding to this expression.
  dve_symbol_table_t * get_symbol_table() const;
  
  /* methods for getting subexpression */
  //!Returns the left-most immediate subexpression 
  dve_expression_t * left() { return subexprs.last(); }
  //!Returns the left-most constant immediate subexpression 
  const dve_expression_t * left() const { return subexprs.last(); }
  //!Returns the right-most immediate subexpression
  dve_expression_t * right() { return subexprs.begin(); }
  //!Returns the right-most constant immediate subexpression
  const dve_expression_t * right() const { return subexprs.begin(); }
  //!Returns the `index'-th immediate subexpression
  dve_expression_t * subexpr(size_int_t index) { return &subexprs[index]; }
  //!Returns the `index'-th constant immediate subexpression
  const dve_expression_t * subexpr(size_int_t index) const
  { return &subexprs[index]; }
  /*@}*/

};

#ifndef DOXYGEN_PROCESSING  
} //END of namespace DVE
#include "common/undeb.hh"

#endif //DOXYGEN_PROCESSING

#endif


