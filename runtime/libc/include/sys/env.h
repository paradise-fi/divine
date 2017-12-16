#ifndef _SYS_ENV_H
#define _SYS_ENV_H

_PDCLIB_EXTERN_C

const struct _VM_Env *__sys_env_get() __attribute__((__annotate__( "divine.link.always" )));

_PDCLIB_EXTERN_END

#endif
