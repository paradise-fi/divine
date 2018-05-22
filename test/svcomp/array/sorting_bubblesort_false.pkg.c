/* TAGS: c todo */
/* VERIFY_OPTS: --sequential -o nofail:malloc */
/* CC_OPTS: */

// V: v.10 CC_OPT: -DSIZE=10
// V: v.100 CC_OPT: -DSIZE=100
// V: v.1000 CC_OPT: -DSIZE=1000
// V: v.10000 CC_OPT: -DSIZE=10000 TAGS: big
// V: v.100000 CC_OPT: -DSIZE=100000 TAGS: big
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
void __VERIFIER_assert(int cond) { if(!(cond)) { ERROR: __VERIFIER_error(); } }


int main( ) {
  int a[ N ];
  int swapped = 1;
  while ( swapped ) {
    swapped = 0;
    int i = 1;
    while ( i < N ) {
      if ( a[i - 1] < a[i] ) {
        int t = a[i];
        a[i] = a[i - 1];
        a[i-1] = t;
        swapped = 1;
      }
      i = i + 1;
    }
  }

  int x;
  int y;
  for ( x = 0 ; x < N ; x++ ) {
    for ( y = x+1 ; y < N ; y++ ) {
      __VERIFIER_assert(  a[x] <= a[y]  ); /* ERROR */
    }
  }
  return 0;
}
