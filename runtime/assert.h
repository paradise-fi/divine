#ifndef _ASSERT_H
#define _ASSERT_H

#include <dios.h>
#define assert( x ) do { if ( !(x) ) __dios_fault( _VM_F_Assert, "Assertion failed" ); } while (0)

#endif // _ASSERT_H
