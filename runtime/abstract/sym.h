#include <abstract/tristate.h>
#include <abstract/common.h>
#include <abstract/formula.h>

extern "C" {

sym::Formula **__abstract_sym_alloca( int bitwidth ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_load( sym::Formula **a, int bitwidth ) _ROOT _NOTHROW;
void __abstract_sym_store( sym::Formula *val, sym::Formula **ptr ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_lift( int64_t val, int bitwidth ) _ROOT _NOTHROW;

sym::Formula *__abstract_sym_add( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_sub( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_mul( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_sdiv( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_udiv( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_urem( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_srem( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_and( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_or( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_xor( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_shl( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_lshr( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_ashr( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;

sym::Formula *__abstract_sym_trunc( sym::Formula *a, int bitwidth ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_zext( sym::Formula *a, int bitwidth ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_sext( sym::Formula *a, int bitwidth ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_bitcast( sym::Formula *a ) _ROOT _NOTHROW;

sym::Formula *__abstract_sym_icmp_eq( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_icmp_ne( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_icmp_ugt( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_icmp_uge( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_icmp_ult( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_icmp_ule( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_icmp_sgt( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_icmp_sge( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_icmp_slt( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;
sym::Formula *__abstract_sym_icmp_sle( sym::Formula *a, sym::Formula *b ) _ROOT _NOTHROW;

abstract::Tristate *__abstract_sym_bool_to_tristate( sym::Formula *a ) _ROOT _NOTHROW;

sym::Formula *__abstract_sym_assume( sym::Formula *value,
                                     sym::Formula *constraint,
                                     bool assume ) _ROOT _NOTHROW;

}
