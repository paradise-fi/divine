#ifndef _SYS_MONITOR_H_
#define _SYS_MONITOR_H_

#include <_PDCLIB_aux.h>

/* TODO: add a C-compatible interface (register only 'step', a plain C function */
/* TODO: add helper functions for marking accepting transitions, ignoring/cancelling
 *       transitions, non-determinism and so on */
/* TODO: how should this works vis-a-vis processes? */

#ifdef __cplusplus
namespace __dios {

struct Context;

/*
 * Custom monitors can be created by overriding the step method. This method is
 * called everytime an interrupt is triggered and therefore, can e.g. validate a
 * global state of the program (and set appropriate flags if neccessary).
 */
struct Monitor
{
    virtual void step( Context& ) = 0;

    void run( Context& c )
    {
        step( c );
        if ( next )
            next->run( c );
    }

    Monitor *next = nullptr;
};

/*
 * Register a monitor. The monitor is called after each interrupt. Multiple
 * monitors can be registered and they are called in the same order as they
 * were added.
 */
void register_monitor( Monitor *monitor ) _PDCLIB_nothrow;

} // namespace __dios
#endif

#endif
