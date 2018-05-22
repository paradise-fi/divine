/* TAGS: c todo */
/* VERIFY_OPTS: --sequential -o nofail:malloc */
/* CC_OPTS: */

// V: v.10 CC_OPT: -DSIZE=10
// V: v.100 CC_OPT: -DSIZE=100
// V: v.1000 CC_OPT: -DSIZE=1000
// V: v.10000 CC_OPT: -DSIZE=10000 TAGS: big
// V: v.100000 CC_OPT: -DSIZE=100000 TAGS: big
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
void __VERIFIER_assert(int cond) { if(!(cond)) { ERROR: __VERIFIER_error(); /* ERROR */ } }

int main( ) {
  int a[ N ];
  int i = 0;
  int x;
  int y;
  while ( i < N ) {
    int k = i + 1;
    int s = i;
    while ( k < N ) {
      if ( a[k] < a[s] ) {
        s = k;
      }
      k = k+1;
    }
    if ( s != i ) {
      int tmp = a[s];
      a[s] = a[i];
      a[i] = tmp;
    }

    for ( x = 0 ; x < i ; x++ ) {
      for ( y = x + 1 ; y < i ; y++ ) {
        __VERIFIER_assert(  a[x] <= a[y]  );
      }
    }
    for ( x = 0 ; x < N ; x++ ) {
      __VERIFIER_assert(  a[x] >= a[i]  );
    }

    i = i+1;
  }

  for ( x = 0 ; x < N ; x++ ) {
    for ( y = x + 1 ; y < N ; y++ ) {
      __VERIFIER_assert(  a[x] <= a[y]  );
    }
  }
  return 0;
}
