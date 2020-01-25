#include <stdint.h>
#include <stdbool.h>
#include <sys/cdefs.h>

typedef struct { void *ptr; } __lamp_ptr;

typedef   float __lamp_f32;
typedef  double __lamp_f64;
typedef uint8_t __lamp_bw;

__BEGIN_DECLS

bool     __lamp_lift_i1 ( bool     v );
uint8_t  __lamp_lift_i8 ( uint8_t  v );
uint16_t __lamp_lift_i16( uint16_t v );
uint32_t __lamp_lift_i32( uint32_t v );
uint64_t __lamp_lift_i64( uint64_t v );
__lamp_f32 __lamp_lift_f32( __lamp_f32 v );
__lamp_f64 __lamp_lift_f64( __lamp_f64 v );

void *__lamp_lift_arr( void *v, int s );
char *__lamp_lift_str( char *s );

__lamp_ptr __lamp_wrap_i1 ( bool     v );
__lamp_ptr __lamp_wrap_i8 ( uint8_t  v );
__lamp_ptr __lamp_wrap_i16( uint16_t v );
__lamp_ptr __lamp_wrap_i32( uint32_t v );
__lamp_ptr __lamp_wrap_i64( uint64_t v );
__lamp_ptr __lamp_wrap_f32( __lamp_f32 v );
__lamp_ptr __lamp_wrap_f64( __lamp_f64 v );

__lamp_ptr   __lamp_wrap_ptr( void *v );
void *__lamp_lift_ptr( void *v );

uint8_t  __lamp_any_i8();
uint16_t __lamp_any_i16();
uint32_t __lamp_any_i32();
uint64_t __lamp_any_i64();
char    *__lamp_any_array();

void  __lamp_freeze( void *val, void *addr );
void *__lamp_melt( void *addr_ );

__lamp_ptr __lamp_add ( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_sub ( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_mul ( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_sdiv( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_udiv( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_srem( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_urem( __lamp_ptr a, __lamp_ptr b );

__lamp_ptr __lamp_fadd( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_fsub( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_fmul( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_fdiv( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_frem( __lamp_ptr a, __lamp_ptr b );

__lamp_ptr __lamp_shl ( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_ashr( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_lshr( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_and ( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_or  ( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_xor ( __lamp_ptr a, __lamp_ptr b );

__lamp_ptr __lamp_eq ( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_ne ( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_ugt( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_uge( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_ult( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_ule( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_sgt( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_sge( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_slt( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_sle( __lamp_ptr a, __lamp_ptr b );

__lamp_ptr __lamp_foeq( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_fogt( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_foge( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_folt( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_fole( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_ford( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_funo( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_fueq( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_fugt( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_fuge( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_fult( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_fule( __lamp_ptr a, __lamp_ptr b );

__lamp_ptr __lamp_ffalse( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_ftrue ( __lamp_ptr a, __lamp_ptr b );

__lamp_ptr __lamp_concat ( __lamp_ptr a, __lamp_ptr b );
__lamp_ptr __lamp_trunc  ( __lamp_ptr a, __lamp_bw  b );
__lamp_ptr __lamp_fptrunc( __lamp_ptr a, __lamp_bw  b );
__lamp_ptr __lamp_sitofp ( __lamp_ptr a, __lamp_bw  b );
__lamp_ptr __lamp_uitofp ( __lamp_ptr a, __lamp_bw  b );
__lamp_ptr __lamp_zext   ( __lamp_ptr a, __lamp_bw  b );
__lamp_ptr __lamp_sext   ( __lamp_ptr a, __lamp_bw  b );
__lamp_ptr __lamp_fpext  ( __lamp_ptr a, __lamp_bw  b );
__lamp_ptr __lamp_fptosi ( __lamp_ptr a, __lamp_bw  b );
__lamp_ptr __lamp_fptoui ( __lamp_ptr a, __lamp_bw  b );

uint8_t __lamp_to_tristate( __lamp_ptr v );
__lamp_ptr __lamp_assume( __lamp_ptr a, __lamp_ptr b, bool c );
__lamp_ptr __lamp_extract( __lamp_ptr a, __lamp_bw s, __lamp_bw e );
bool __lamp_decide( uint8_t tristate );

__END_DECLS
