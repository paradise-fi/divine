#ifndef _ASSERT_H
#define _ASSERT_H

#include <divine.h>
#define assert( x ) do { if ( !(x) ) __vm_fault( _VM_F_Assert ); } while (0)

#endif // _ASSERT_H
