#ifndef __SYS_FAULT_H__
#define __SYS_FAULT_H__

#include <sys/divm.h>
#include <_PDCLIB_aux.h>

enum _DiOS_Fault
{
    _DiOS_F_Threading = _VM_F_Last,
    _DiOS_F_Assert,
    _DiOS_F_Config,
    _DiOS_F_Last
};

enum _DiOS_SimFail
{
    _DiOS_SF_Malloc = _DiOS_F_Last,
    _DiOS_SF_Last
};

enum _DiOS_FaultConfig
{
    _DiOS_FC_EInvalidCfg = -3,
    _DiOS_FC_EInvalidFault = -2,
    _DiOS_FC_ELocked = -1,
    _DiOS_FC_Ignore = 0,
    _DiOS_FC_Report,
    _DiOS_FC_Abort,
    _DiOS_FC_NoFail,
    _DiOS_FC_SimFail,
};

_PDCLIB_EXTERN_C

/*
 * Configures given fault or symfail and returns original value. Possible
 * configurations are _DiOS_FC_{Ignore, Report, Abort, NoFail, SimFail}.
 *
 * Return original configuration or error.
 * Possible errors:
 *   - _DiOS_FC_ELocked if user forced cofiguration,
 *   - _DiOS_FC_EInvalidFault if invalid fault number was specified
 *   - _DiOS_FC_EInvalidCfg if simfail configuration was passed for fault or
 *     fault confiugation was passed for simfail.
 */
int __dios_configure_fault( int fault, int cfg ) _PDCLIB_nothrow;

/*
 * Return fault configuration. Possible configurations are
 * _DiOS_FC_{Ignore, Report, Abort, NoFail, SimFail}.
 *
 * Can return _DiOS_FC_EInvalidFault if invalid fault was specified.
 */
int __dios_get_fault_config( int fault ) _PDCLIB_nothrow;

/*
 * Cause program fault by calling fault handler. Remaining arguments are
 * ignored, but can be examined by the fault handler, since it is able to obtain
 * a pointer to the call instruction which invoked __vm_fault by reading the
 * program counter of its parent frame.
 */
void __dios_fault( int f, const char *msg, ... ) _PDCLIB_nothrow __attribute__(( __noinline__ ));

_PDCLIB_EXTERN_END

#ifdef __cplusplus

namespace __dios {

struct DetectFault
{
    DetectFault( int fault ) _PDCLIB_nothrow :
        _fault( fault ), _orig( __dios_configure_fault( fault, _DiOS_FC_Report ) )
    { }

#if __cplusplus >= 201103L
    DetectFault( const DetectFault & ) = delete;
#endif

    ~DetectFault() _PDCLIB_nothrow {
        __dios_configure_fault( _fault, _orig );
        __vm_control( _VM_CA_Bit, _VM_CR_Flags, _VM_CF_Error, uintptr_t( 0 ) );
    }

    static bool triggered() {
        return uintptr_t( __vm_control( _VM_CA_Get, _VM_CR_Flags ) ) & _VM_CF_Error;
    }

  private:
    int _fault;
    int _orig;
};

}

#endif

#endif
