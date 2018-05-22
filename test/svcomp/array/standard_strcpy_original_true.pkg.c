/* TAGS: c sym todo */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
/* CC_OPTS: */

// V: v.10 CC_OPT: -DSIZE=10
// V: v.100 CC_OPT: -DSIZE=100
// V: v.1000 CC_OPT: -DSIZE=1000 TAGS: ext
// V: v.10000 CC_OPT: -DSIZE=10000 TAGS: big
// V: v.100000 CC_OPT: -DSIZE=100000 TAGS: big
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern int __VERIFIER_nondet_int(void);
void __VERIFIER_assert(int cond) { if(!(cond)) { ERROR: __VERIFIER_error(); } }

int main( ) {
  int src[SIZE];
  int dst[SIZE];
  int i = 0;
  int j = 0;

  while ( j < SIZE ) {
    src[j] = __VERIFIER_nondet_int();
    j = j + 1;
  }

  while ( i < SIZE && src[i] != 0 ) {
    dst[i] = src[i];
    i = i + 1;
  }

  i = 0;
  while ( i < SIZE && src[i] != 0 ) {
    __VERIFIER_assert(  dst[i] == src[i]  );
    i = i + 1;
  }
  return 0;
}

