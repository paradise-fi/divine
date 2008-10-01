/* -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>
  observers for visitors

  principle: the Visitor classes above encapsulate graph-visiting
  algorithms (DFS and BFS), and at each state (either due to
  transition being unfolded or that the state has been removed from
  the stack/queue) call into an assigned observer
 */

#ifndef DIVINE_OBSERVER_H
#define DIVINE_OBSERVER_H

namespace divine {
namespace observer {

template< typename Bundle, typename Self >
struct Common { // a NOOP observer
    typedef typename Bundle::State State;
    Self &self() { return *static_cast< Self* >( this ); }

    // by default, just follow every transition and do nothing else
    State transition( State from, State to ) {
        return follow( to );
    }

    // tell visitor to follow this transition
    State follow( State st ) {
        return self().visitor().follow( st );
    }

    // tell visitor to NOT follow this transition
    State ignore( State st ) {
        return self().visitor().ignore( st );
    }

    void expanding( State st ) {}
    void finished( State st ) {}
    void errorState( State st, int err ) {}

    void parallelTermination() {}
    void serialTermination() {}
};


}
}

#endif
