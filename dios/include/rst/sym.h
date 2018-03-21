#pragma once
#include <rst/formula.h>

typedef lart::sym::Formula __sym_formula;

#define DOMAIN_NAME sym
#define DOMAIN_TYPE __sym_formula*
    #include <rst/domain.def>
#undef DOMAIN_NAME
#undef DOMAIN_TYPE

extern "C" void __sym_formula_dump();
