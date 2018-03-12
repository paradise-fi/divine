#include <rst/tristate.h>
#include <rst/common.h>
#include <rst/formula.h>

extern "C" {

typedef lart::sym::Formula __sym_formula;

__sym_formula **__abstract_sym_alloca( int bitwidth ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_load( __sym_formula **a, int bitwidth ) _ROOT _NOTHROW;
void __abstract_sym_store( __sym_formula *val, __sym_formula **ptr ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_lift( int64_t val, int bitwidth ) _ROOT _NOTHROW;

__sym_formula *__abstract_sym_add( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_sub( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_mul( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_sdiv( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_udiv( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_urem( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_srem( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_and( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_or( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_xor( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_shl( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_lshr( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_ashr( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;

__sym_formula *__abstract_sym_trunc( __sym_formula *a, int bitwidth ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_zext( __sym_formula *a, int bitwidth ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_sext( __sym_formula *a, int bitwidth ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_bitcast( __sym_formula *a ) _ROOT _NOTHROW;

__sym_formula *__abstract_sym_icmp_eq( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_icmp_ne( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_icmp_ugt( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_icmp_uge( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_icmp_ult( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_icmp_ule( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_icmp_sgt( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_icmp_sge( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_icmp_slt( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;
__sym_formula *__abstract_sym_icmp_sle( __sym_formula *a, __sym_formula *b ) _ROOT _NOTHROW;

abstract::Tristate *__abstract_sym_bool_to_tristate( __sym_formula *a ) _ROOT _NOTHROW;

__sym_formula *__abstract_sym_assume( __sym_formula *value,
                                     __sym_formula *constraint,
                                     bool assume ) _ROOT _NOTHROW;

void __sym_formula_dump();
}
