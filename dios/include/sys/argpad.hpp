#ifndef _SYS_ARGPAD_
#define _SYS_ARGPAD_

namespace __dios
{

static struct Pad {} _1, _2, _3, _4, _5, _6, _7;

#define __inline __attribute__((always_inline))

template< typename T1, typename T2, typename R,
          typename FA, typename FB, typename FC, typename FD, typename FE, typename FF,
          typename PA, typename PB, typename PC, typename PD, typename PE, typename PF >
__inline auto unpad( T1 *t, R (T2::*f)( FA, FB, FC, FD, FE, FF ),
                     PA a, PB b, PC c, PD d, PE e, PF g )
{
    return (t->*f)( a, b, c, d, e, g );
}

template< typename T1, typename T2, typename R,
          typename FA, typename FB, typename FC, typename FD, typename FE,
          typename PA, typename PB, typename PC, typename PD, typename PE >
__inline auto unpad( T1 *t, R (T2::*f)( FA, FB, FC, FD, FE ),
                     PA a, PB b, PC c, PD d, PE e, Pad )
{
    return (t->*f)( a, b, c, d, e );
}

template< typename T1, typename T2, typename R,
          typename FA, typename FB, typename FC, typename FD,
          typename PA, typename PB, typename PC, typename PD >
__inline auto unpad( T1 *t, R (T2::*f)( FA, FB, FC, FD ),
                     PA a, PB b, PC c, PD d, Pad, Pad )
{
    return (t->*f)( a, b, c, d );
}

template< typename T1, typename T2, typename R,
          typename FA, typename FB, typename FC,
          typename PA, typename PB, typename PC >
__inline auto unpad( T1 *t, R (T2::*f)( FA, FB, FC ),
                     PA a, PB b, PC c, Pad, Pad, Pad )
{
    return (t->*f)( a, b, c );
}

template< typename T1, typename T2, typename R,
          typename FA, typename FB,
          typename PA, typename PB >
__inline auto unpad( T1 *t, R (T2::*f)( FA, FB ),
                     PA a, PB b, Pad, Pad, Pad, Pad )
{
    return (t->*f)( a, b );
}

template< typename T1, typename T2, typename R,
          typename FA,
          typename PA >
__inline auto unpad( T1 *t, R (T2::*f)( FA ),
                     PA a, Pad, Pad, Pad, Pad, Pad )
{
    return (t->*f)( a );
}

template< typename T1, typename T2, typename R >
__inline auto unpad( T1 *t, R (T2::*f)(), Pad, Pad, Pad, Pad, Pad, Pad )
{
    return (t->*f)();
}

}

#endif
