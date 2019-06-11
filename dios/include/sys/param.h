#include <sys/cdefs.h>

#ifndef _SYS_PARAM_H
#define _SYS_PARAM_H

__BEGIN_DECLS

/*
 * Return number of claimed hardware concurrency units, specified in DiOS boot
 * parameters.
 */
int __dios_hardware_concurrency() __nothrow;

__END_DECLS

#endif
