/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
/* CC_OPTS: */

// V: v.10 CC_OPT: -DSIZE=10
// V: v.100 CC_OPT: -DSIZE=100
// V: v.1000 CC_OPT: -DSIZE=1000
// V: v.10000 CC_OPT: -DSIZE=10000 TAGS: big
// V: v.100000 CC_OPT: -DSIZE=100000 TAGS: big
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
void __VERIFIER_assert(int cond) { if(!(cond)) { ERROR: __VERIFIER_error(); } }
extern int __VERIFIER_nondet_int();

int main( )
{
  int a[SIZE];
  int b[SIZE];
  int i = 0;
  int j = 0;
  while( i < SIZE )
  {
	b[i] = __VERIFIER_nondet_int();
    i = i+1;
  }

  i = 1;
  while( i < SIZE )
  {
	a[j] = b[i];
        i = i+6;
        j = j+1;
  }

  i = 1;
  j = 0;
  while( i < SIZE )
  {
	__VERIFIER_assert( a[j] == b[6*j+1] );
        i = i+6;
        j = j+1;
  }
  return 0;
}
