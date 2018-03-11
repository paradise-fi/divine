#ifndef _SYS_ARGPAD_
#define _SYS_ARGPAD_

namespace __dios
{

static struct Pad {} _1, _2, _3, _4, _5, _6, _7;

#define __inline __attribute__((always_inline))

template< typename F, typename A, typename B, typename C, typename D, typename E, typename G >
__inline auto unpad( F f, A a, B b, C c, D d, E e, G g ) { return f( a, b, c, d, e, g ); }
template< typename F, typename A, typename B, typename C, typename D, typename E >
__inline auto unpad( F f, A a, B b, C c, D d, E e, Pad ) { return f( a, b, c, d, e ); }
template< typename F, typename A, typename B, typename C, typename D >
__inline auto unpad( F f, A a, B b, C c, D d, Pad, Pad ) { return f( a, b, c, d ); }
template< typename F, typename A, typename B, typename C >
__inline auto unpad( F f, A a, B b, C c, Pad, Pad, Pad ) { return f( a, b, c ); }
template< typename F, typename A, typename B >
__inline auto unpad( F f, A a, B b, Pad, Pad, Pad, Pad ) { return f( a, b ); }
template< typename F, typename A >
__inline auto unpad( F f, A a, Pad, Pad, Pad, Pad, Pad ) { return f( a ); }
template< typename F >
__inline auto unpad( F f, Pad, Pad, Pad, Pad, Pad, Pad ) { return f(); }

}

#endif
