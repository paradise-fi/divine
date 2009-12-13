#include <limits.h>
#include <sstream>
#include "system/dve/dve_expression.hh"
#include "system/dve/syntax_analysis/dve_symbol_table.hh"
#include "system/dve/syntax_analysis/dve_grammar.hh"
#include "common/deb.hh"

#include <wibble/string.h>

#ifndef DOXYGEN_PROCESSING
using namespace divine;
#endif //DOXYGEN_PROCESSING

using std::string;
using namespace divine;

void compacted_t::create_gid(int _op, size_int_t _gid) const
{
  ptr = static_cast<compacted_viewer_t*>
    (malloc(sizeof(compacted_viewer_t) + sizeof(size_int_t)));
  ptr->size = sizeof(compacted_viewer_t) + sizeof(size_int_t);
  ptr->op = _op;
  ptr->arity = 0;
  ptr->r_offset = 0;  
  //if arity==0 then left() points to the value stored inside expression
  size_int_t* p_val = (size_int_t*)((char*)ptr + sizeof(compacted_viewer_t));
  *p_val=_gid;
}

void compacted_t::create_val(int _op, all_values_t _value) const
{
  ptr =  static_cast<compacted_viewer_t*>
    (malloc(sizeof(compacted_viewer_t) + sizeof(all_values_t)));
  ptr->size = sizeof(compacted_viewer_t) + sizeof(all_values_t);
  ptr->op = _op;
  ptr->arity = 0;
  ptr->r_offset = 0;  
  //if arity==0 then left() points to the value stored inside expression
  all_values_t* p_val = (all_values_t*)((char*)ptr + sizeof(compacted_viewer_t));
  *p_val=_value;
}

void compacted_t::join(int _op, compacted_viewer_t* _left, compacted_viewer_t* _right) const
{
  int new_size = _left->size + sizeof(compacted_viewer_t);

  if (_right != 0) //arity 2
    {
      DEBFUNC(cerr<<"(arity 2) ";);
      new_size += _right->size;
    }
  
  ptr = static_cast<compacted_viewer_t*>(malloc(new_size));
  ptr->size=new_size;
  ptr->op=_op;
  if (_right == 0)
    {
      ptr->arity = 1;
      ptr->r_offset = 0;
    }
  else
    {
      ptr->arity = 2;
      ptr->r_offset=sizeof(compacted_viewer_t) + _left->size;
      memcpy((char *)ptr+ptr->r_offset, _right, _right->size);
    }

  memcpy((char *)ptr+sizeof(compacted_viewer_t),_left,_left->size);
}


std::string compacted_t::to_string()
{
  compacted_t l,r;
  l.ptr = left();
  r.ptr = right();
  string res="[";
  std::ostringstream tmpstr;
  switch(ptr->op){
  case T_ID:
    tmpstr<<"T_ID,gid="<<get_gid() ; 
    break;
  case T_FOREIGN_ID:
    tmpstr<<"T_FOREIGN_ID,gid="<<get_gid() ; 
    break;
  case T_NAT:
    tmpstr<<"T_NAT,gid="<<get_value() ; 
    break;
  case T_PARENTHESIS:
    tmpstr<<"T_PARENTHESIS,"<<l.to_string();
    break;
  case T_FOREIGN_SQUARE_BRACKETS:
    tmpstr<<"T_FOREIGN_SQUARE_BRACKETS,"<<l.to_string()<<","
	  <<r.to_string(); 
    break;
  case T_SQUARE_BRACKETS:
    tmpstr<<"T_SQUARE_BRACKETS,"<<l.to_string()<<","
	  <<r.to_string(); 
    break;
  case T_LT: 
    tmpstr<<"T_LT,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_LEQ:
    tmpstr<<"T_LEQ,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_EQ:
    tmpstr<<"T_EQ,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_NEQ:
    tmpstr<<"T_NEQ,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_GT:
    tmpstr<<"T_GT,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_PLUS:
    tmpstr<<"T_PLUS,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_MINUS:
    tmpstr<<"T_MINUS,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_MULT:
    tmpstr<<"T_MULT,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_DIV:
    tmpstr<<"T_DIV,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_MOD:
    tmpstr<<"T_MOD,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_AND:
    tmpstr<<"T_AND,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_OR:
    tmpstr<<"T_OR,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_XOR:
    tmpstr<<"T_XOR,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_LSHIFT:
    tmpstr<<"T_LSHIFT,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_RSHIFT:
    tmpstr<<"T_RSHIFT,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_BOOL_AND:
    tmpstr<<"T_BOOL_AND,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_BOOL_OR:
    tmpstr<<"T_BOOL_OR,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_DOT:
    tmpstr<<"T_DOT,"<<l.to_string();
    break;
  case T_IMPLY:
    tmpstr<<"T_IMPLY,"<<l.to_string()<<","<<r.to_string(); 
    break;
  case T_UNARY_MINUS:
    tmpstr<<"T_UNARY_MINUS,"<<l.to_string();
    break;
  case T_TILDE:
    tmpstr<<"T_TILDE,"<<l.to_string();
    break;
  case T_BOOL_NOT:
    tmpstr<<"T_BOOL_NOT,"<<l.to_string();
    break;
  case T_ASSIGNMENT:  
    tmpstr<<"T_ASSIGNMENT,"<<l.to_string()<<","<<r.to_string(); 
    break;
  }

  res=res+tmpstr.str();
  res +="]";
  return res;
}

error_vector_t *dve_expression_t::pexpr_terr = &gerr;
dve_parser_t dve_expression_t::expr_parser(*pexpr_terr, (dve_expression_t *)(0));

template< typename T >
void safe_free(T *&ptr) {
    if (ptr != NULL) {
        free(ptr);
        ptr = NULL;
    }
}

void dve_expression_t::compaction()
{  
  compacted_t c;
  int expr_op=get_operator();
  
  if (expr_op == T_NAT || expr_op == T_DOT || expr_op == T_ID || expr_op == T_FOREIGN_ID)
    {
      if (expr_op == T_NAT)
	{
	  c.create_val(expr_op,get_value());
      safe_free(p_compact);
	  p_compact = c.ptr;
	}
      else
	{
	  c.create_gid(expr_op,get_ident_gid());
      safe_free(p_compact);
	  p_compact = c.ptr;	   
	}
      return;
    }

  /* BEWARE of T_SQUARE_BRACKETS
   * --------------------------- */
  /* It is an expression with arity one, but it keeps 2 different values:
   * "gid" of the array and subexpression with the "array index".  For
   * compaction purposes we will consider it as an expression of arity 2.
   */

  if (expr_op==T_SQUARE_BRACKETS || expr_op==T_FOREIGN_SQUARE_BRACKETS)
    {
      c.create_val(T_NAT,static_cast<all_values_t>(get_ident_gid()));
      compacted_viewer_t *tmp=c.ptr;
      if (left()->get_p_compact() == 0)
	{
	  left()->compaction();
	}
      c.join(expr_op,tmp,left()->get_p_compact());
      safe_free(p_compact);
      p_compact = c.ptr;
      free (tmp);
      return;
    }
  
  if (arity()>0 && arity()<=2)
    {
      if (left()->get_p_compact() == 0)
	{
	  left()->compaction();
	}
      if (arity()>1)
	{
	  if (right()->get_p_compact() == 0)
	    {
	      right()->compaction();
	    }	  
	  c.join(get_operator(),
		 left()->get_p_compact(),
		 right()->get_p_compact()
		 );
      safe_free(p_compact);
	  p_compact = c.ptr;
	}
      else
	{
	  c.join(get_operator(),
		 left()->get_p_compact(),
		 0
		 );
      safe_free(p_compact);
	  p_compact = c.ptr;
	}
     }
}


dve_expression_t::dve_expression_t(int expr_op, std::stack<dve_expression_t *> & subexprs_stack, short int ar, system_t * const system): //ar and system are deafautly 0
  expression_t(system), subexprs(0,DVE_MAXIMAL_ARITY)
// { create_from_stack(expr_op, subexprs_stack, ar) }
//void dve_expression_t::create_from_stack(int expr_op, std::stack<dve_expression_t *> & subexprs_stack, short int ar): //ar and p are deafautly 0
{
 DEBFUNC(cerr << "Constructor 2 of dve_expression_t" << endl;)
 p_compact = 0;
 if (expr_op != T_NO_TOKEN)
  {
   op = expr_op;
   for (;ar>0;ar--)
    {
     DEB(cerr << "Extending the list of subexpressions" << endl;)
     subexprs.extend(1);
     DEBAUX(cerr << "subexprs_stack.size() = " <<subexprs_stack.size()<<endl;)
     subexprs.back().swap(*subexprs_stack.top());
     delete subexprs_stack.top();
     subexprs_stack.pop();
    }
  }
 else
  {
   op = T_NO_TOKEN;
   gerr << "Not enough parameters for subexpression." << thr();
  }
}

dve_expression_t::~dve_expression_t()
{
  DEBFUNC(cerr << "Destructor of dve_expression_t" << endl;);
  if (p_compact)
    free (p_compact);
  p_compact=0;
}

void dve_expression_t::assign(const expression_t & expr)
{
 DEBAUX(cerr << "dve_expression_t::assign" << endl;)
 expression_t::assign(expr);
 const dve_expression_t * dve_expr = dynamic_cast<const dve_expression_t*>(&expr);

 set_source_pos(*dve_expr);

 op = dve_expr->op;

 if (op == T_NAT) contain.value = dve_expr->contain.value;
 else contain.symbol_gid = dve_expr->contain.symbol_gid;
 
 subexprs.resize(dve_expr->subexprs.size());
 for (size_int_t i=0; i!=subexprs.size(); i++)
   subexprs[i].assign(dve_expr->subexprs[i]);

 if (p_compact!=0) //apply compact stuff only if compacted
   {
     free (p_compact);
     p_compact=0;   
     size_t auxsize = *((size_t *)(dve_expr->get_p_compact()));
     p_compact = static_cast<compacted_viewer_t*>(malloc(auxsize));
     memcpy (p_compact,dve_expr->get_p_compact(),auxsize);
   }
}

void dve_expression_t::swap(expression_t & expr)
 {
  expression_t::swap(expr);
  dve_expression_t * dve_expr = dynamic_cast<dve_expression_t*>(&expr);
  int aux_op = op; op = dve_expr->op; dve_expr->op = aux_op;
  compacted_viewer_t *aux_p_compact = p_compact;
  p_compact = dve_expr->p_compact; 
  dve_expr->p_compact = aux_p_compact;

  dve_source_position_t source_pos = (*this);
  set_source_pos(*dve_expr);
  dve_expr->set_source_pos(source_pos);
  
  if (op == T_NAT)
   {
    all_values_t aux_value = contain.value;
    contain.value = dve_expr->contain.value;
    dve_expr->contain.value = aux_value;
   }
  else
   {
    size_int_t aux_gid = contain.symbol_gid;
    contain.symbol_gid = dve_expr->contain.symbol_gid;
    dve_expr->contain.symbol_gid = aux_gid;
   }

  subexprs.swap(dve_expr->subexprs) ;
 }

dve_expression_t::dve_expression_t(const std::string & str_expr, system_t * const system, size_int_t process_gid):
   expression_t(system), subexprs(2,DVE_MAXIMAL_ARITY)
   //DEFAULTLY process_gid = NO_ID
{
  DEBFUNC(cerr << "Constructor 6 of dve_expression_t" << endl;)
  p_compact=0;
 std::istringstream str_stream(str_expr);
 if (read(str_stream, process_gid)) gerr << "Error while creating expression"
   << "from string: syntax error" << thr();
 
}

std::string dve_expression_t::to_string() const
{
 std::ostringstream ostr;
 write(ostr);
 return ostr.str();
}

int dve_expression_t::from_string(std::string & expr_str,
                              const size_int_t process_gid)
{
 std::istringstream str_stream(expr_str);
 return read(str_stream, process_gid);
}

int dve_expression_t::read(std::istream & istr, size_int_t process_gid)
{
  //DEB(cerr << "Creating expression by parsing " << str_expr << endl;)
 dve_expr_init_parsing(&expr_parser, pexpr_terr, istr);
 expr_parser.set_dve_expression_to_fill(this);
 expr_parser.set_current_process(process_gid);
 try
  { dve_eeparse(); }
 catch (ERR_throw_t & err)
  { return err.id; } //returns non-zero value
 return 0; //returns zero in case of successful parsing
}

void dve_expression_t::write(std::ostream & ostr) const
{
  //DEBFUNC(cerr << "BEGIN of dve_expression_t::write" << endl;)
 dve_symbol_table_t * parent_table = get_symbol_table();
 if (!parent_table) gerr << "Writing expression: Symbol table not set" << thr();
 switch (op)
  {
   case T_ID:
    { ostr << parent_table->get_variable(contain.symbol_gid)->
                get_name();
      break; }
   case T_FOREIGN_ID:
    { ostr << parent_table->get_process(parent_table->get_variable(contain.symbol_gid)->get_process_gid())->get_name(); //name of process
      ostr<<"->";
      ostr << parent_table->get_variable(contain.symbol_gid)->get_name();
      break; }
   case T_NAT:
    { ostr << contain.value; break; }
   case T_PARENTHESIS:
    { ostr << "("; left()->write(ostr); ostr << ")"; break; }
   case T_SQUARE_BRACKETS:
    { ostr << parent_table->get_variable(contain.symbol_gid)->
                 get_name(); ostr<<"["; left()->write(ostr); ostr<<"]" ;break; }
   case T_FOREIGN_SQUARE_BRACKETS:
    { ostr << parent_table->get_process(parent_table->get_variable(contain.symbol_gid)->get_process_gid())->get_name(); //name of preocess
      ostr<<"->";
      ostr << parent_table->get_variable(contain.symbol_gid)->get_name();
      ostr<<"["; left()->write(ostr); ostr<<"]" ;break; }
   case T_LT: { left()->write(ostr); ostr<<"<"; right()->write(ostr); break; }
   case T_LEQ: { left()->write(ostr); ostr<<"<="; right()->write(ostr); break; }
   case T_EQ: { left()->write(ostr); ostr<<"=="; right()->write(ostr); break; }
   case T_NEQ: { left()->write(ostr); ostr<<"!="; right()->write(ostr); break; }
   case T_GT: { left()->write(ostr); ostr<<">"; right()->write(ostr); break; }
   case T_GEQ: { left()->write(ostr); ostr<<">="; right()->write(ostr); break; }
   case T_PLUS: { left()->write(ostr); ostr<<"+"; right()->write(ostr); break; }
   case T_MINUS: { left()->write(ostr); ostr<<"-"; right()->write(ostr); break; }
   case T_MULT: { left()->write(ostr); ostr<<"*"; right()->write(ostr); break; }
   case T_DIV: { left()->write(ostr); ostr<<"/"; right()->write(ostr); break; }
   case T_MOD: { left()->write(ostr); ostr<<"%"; right()->write(ostr); break; }
   case T_AND: { left()->write(ostr); ostr<<"&"; right()->write(ostr); break; }
   case T_OR: { left()->write(ostr); ostr<<"|"; right()->write(ostr); break; }
   case T_XOR: { left()->write(ostr); ostr<<"^"; right()->write(ostr); break; }
   case T_LSHIFT: { left()->write(ostr); ostr<<"<<"; right()->write(ostr); break; }
   case T_RSHIFT: { left()->write(ostr); ostr<<">>"; right()->write(ostr); break; }
   case T_BOOL_AND: {left()->write(ostr); ostr<<" and ";right()->write(ostr); break;}
   case T_BOOL_OR: {left()->write(ostr); ostr<<" or "; right()->write(ostr); break;}
   case T_DOT:
    { ostr<<parent_table->get_process(parent_table->get_state(contain.symbol_gid)->get_process_gid())->get_name(); ostr<<"."; ostr<<parent_table->get_state(contain.symbol_gid)->get_name(); break; }
   case T_IMPLY: { left()->write(ostr); ostr<<" -> "; right()->write(ostr); break; }
   case T_UNARY_MINUS: { ostr<<"-"; right()->write(ostr); break; }
   case T_TILDE: { ostr<<"~"; right()->write(ostr); break; }
   case T_BOOL_NOT: { ostr<<" not "; right()->write(ostr); break; }
   case T_ASSIGNMENT: { left()->write(ostr); ostr<<" = "; right()->write(ostr); break; }
   default: { gerr << "Problem in expression - unknown operator"
                      " number " << op << psh(); }
  }
 //DEBFUNC(cerr << "END of dve_expression_t::write" << endl;)
}

dve_symbol_table_t * dve_expression_t::get_symbol_table() const
{
 dve_system_t * dve_system;
 if ((dve_system=dynamic_cast<dve_system_t *>(parent_system)))
   return dve_system->get_symbol_table();
 else return 0;
}



