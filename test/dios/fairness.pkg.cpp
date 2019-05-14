// TAGS: min
// VERIFY_OPTS: --liveness -o nofail:malloc

#include <dios.h>
#include <thread>
#include <atomic>
#include <sys/monitor.h>

// V: unfair V_OPT: --dios-config default
// V: fair   V_OPT: --dios-config fair

bool checkX();
void __buchi_accept();

struct BuchiAutomaton : public __dios::Monitor
{
  BuchiAutomaton() : state(0) {}
  void step()
  {
    switch(state)
    {
      case 0: {
        unsigned nd0 = __vm_choose(2);
        if ((nd0 == 0) && true)
        {
          state = 0;
        }
        else if ((nd0 == 1) && !checkX())
        {
          state = 1;
        }
        break;
      }
      case 1: {
        if (!checkX())
        {
          __buchi_accept(); /* ERR_unfair */
          state = 1;
        }
        else
        {
          __vm_cancel();
        }
        break;
      }
    }
  }

  unsigned state;
};

void __buchi_accept()
{
    __vm_ctl_flag( 0, _VM_CF_Accepting );
}

std::atomic_int x;

bool checkX() { return x == 1; }

int main() {
    __dios::register_monitor( new BuchiAutomaton() );

    __dios_trace_t( "Monitor registered" );

    auto t = std::thread( [&]{
        while ( true ) {
            __dios_trace_t( "Set 1" );
            x = 1;
            __dios_suspend();
        }
    } );

    __dios_trace_t( "Thread started" );

    while ( true ) {
        __dios_trace_t( "Set 0" );
        x = 0;
        __dios_suspend();
    }
}
