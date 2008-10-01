#ifndef DOXYGEN_PROCESSING
#include "system/dve/syntax_analysis/dve_symbol.hh"
#include "common/deb.hh"

using namespace divine;
#endif //DOXYGEN_PROCESSING

const size_int_t dve_symbol_t::CHANNEL_UNUSED = MAX_SIZE_INT;

////METHODS of dve_symbol_t
dve_symbol_t::~dve_symbol_t()
 {
  DEBFUNC(cerr << "BEGIN of destructor of dve_symbol_t" << endl;)
  if (type == SYM_VARIABLE)
   {
    if (info_var.vector)
     {
      if (info_var.init.field != 0)
       {
        for (std::vector<dve_expression_t *>::iterator
             i=info_var.init.field->begin();
             i!=info_var.init.field->end(); ++i)
          delete (*i);
        delete info_var.init.field;
       }
     }
    else if (type == SYM_VARIABLE)
     { if (info_var.init.expr!=0) delete info_var.init.expr; }
   }
  DEBFUNC(cerr << "END of destructor of dve_symbol_t" << endl;)
 }



