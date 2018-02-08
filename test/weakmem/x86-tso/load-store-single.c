/* TAGS: c tso min ext */
/* VERIFY_OPTS: --relaxed-memory tso */

/* this test is derived from the paper x86-TSO: A Rigorous and Usable
   Programmer’s Model for x86 Multiprocessors by Peter Sewell, Susmit Sarkar,
   Scott Owens, Francesco Zappa Nardelli, and Magnus O. Myreen;
   URL: https://www.cl.cam.ac.uk/~pes20/weakmemory/
   figure 8-4
*/

#include <assert.h>

volatile int x;

int main() {
    x = 1;
    assert( x == 1 );
}
