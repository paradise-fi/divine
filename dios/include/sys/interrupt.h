#ifndef __SYS_INTERRUPT_H__
#define __SYS_INTERRUPT_H__

#include <sys/divm.h>
#include <assert.h>

#include <_PDCLIB/cdefs.h>

static const uint64_t _DiOS_CF_Mask       = _VM_CFB_OS << 0;
static const uint64_t _DiOS_CF_Deferred   = _VM_CFB_OS << 1;
static const uint64_t _DiOS_CF_Fairness   = _VM_CFB_OS << 4;

__BEGIN_DECLS
void __dios_suspend( void ) __nothrow;
void __dios_reschedule( void ) __nothrow;
int __dios_mask( int ) __nothrow;
__END_DECLS

#ifdef __cplusplus

namespace __dios
{

template< bool fenced >
struct _InterruptMask
{
    __attribute__((always_inline))
    _InterruptMask() _PDCLIB_nothrow
    {
        _orig_state = __vm_ctl_flag( 0, _DiOS_CF_Mask ) & _DiOS_CF_Mask;
        if ( fenced )
            __sync_synchronize();
    }

    __attribute__((always_inline))
    ~_InterruptMask() _PDCLIB_nothrow
    {
        if ( fenced )
            __sync_synchronize();
        if ( !_orig_state )
        {
            __vm_ctl_flag( _DiOS_CF_Mask, 0 );
            if ( uintptr_t( __vm_ctl_get( _VM_CR_Flags ) ) & _DiOS_CF_Deferred )
                __dios_reschedule();
        }
    }

  private:

    struct Without
    {
        Without( _InterruptMask &self ) _PDCLIB_nothrow : self( self )
        {
            if ( !self._orig_state )
            {
                if ( fenced )
                    __sync_synchronize();
                __vm_ctl_flag( _DiOS_CF_Mask, 0 );
                if ( uintptr_t( __vm_ctl_get( _VM_CR_Flags ) ) & _DiOS_CF_Deferred )
                    __dios_reschedule();
            }
        }

        // acquire mask if not masked already
        ~Without() _PDCLIB_nothrow
        {
            if ( !self._orig_state )
            {
                if ( fenced )
                    __sync_synchronize();
                __vm_ctl_flag( 0, _DiOS_CF_Mask );
            }
        }

        _InterruptMask &self;
    };

  public:

#if __cplusplus >= 201103L
    _InterruptMask( const _InterruptMask & ) = delete;
    _InterruptMask( _InterruptMask && ) = delete;
#else
  private:
    _InterruptMask( const _InterruptMask & );
    _InterruptMask &operator=( const _InterruptMask & );
  public:
#endif

#if __cplusplus >= 201402L
    // break mask (if it is held by this guard), call given function and then
    // return mask to original state
    template< typename Fun >
    auto without( Fun &&f, bool mustBreakMask = false ) _PDCLIB_nothrow
    {
        assert( ( !mustBreakMask || !_orig_state ) &&
                "InterruptMask is required to own the mask in InterruptMask::without" );
        Without _( *this );
        return f();
    }
#endif

    // beware, this is dangerous
    void _setOrigState( bool state ) _PDCLIB_nothrow { _orig_state = state; }
    bool _origState() const _PDCLIB_nothrow { return _orig_state; }

  private:

    bool _orig_state;
};

typedef _InterruptMask< false > InterruptMask;
typedef _InterruptMask< true >  FencedInterruptMask;

}

#endif
#endif
