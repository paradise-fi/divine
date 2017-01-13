// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef _DIOS_MONITOR_H_
#define _DIOS_MONITOR_H_

namespace __dios {

struct Context;

/*
 * Custom monitors can be created by overriding step method. This method is
 * called everytime an interrupt is triggered and therefore, can e.g. validate a
 * global state of program (and set appropriate flags if neccessary).
 */
struct Monitor {
    virtual void step( Context& ) = 0;

    void run( Context& c ) {
        step( c );
        if ( next )
            next->run( c );
    }

    Monitor *next = nullptr;
};

} // namespace __dios

#endif // _DIOS_MONITOR_H
