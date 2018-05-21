/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
/* CC_OPTS: */

/* Benchmark used to verify Chimdyalwar, Bharti, et al. "VeriAbs: Verification by abstraction (competition contribution)."
International Conference on Tools and Algorithms for the Construction and Analysis of Systems. Springer, Berlin, Heidelberg, 2017.*/

// V: small.10 CC_OPT: -DSIZE=10
// V: small.100 CC_OPT: -DSIZE=100
// V: big.1000 CC_OPT: -DSIZE=1000 TAGS: big
// V: big.10000 CC_OPT: -DSIZE=10000 TAGS: big
// V: big.100000 CC_OPT: -DSIZE=100000 TAGS: big

extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern void __VERIFIER_assume(int);
void __VERIFIER_assert(int cond)
{
  if (!(cond)) {
    ERROR: __VERIFIER_error();
  }
  return;
}

unsigned int __VERIFIER_nondet_uint();

int main()
{
  unsigned int array[SIZE];
  unsigned int index;

  for (index = 0; index < SIZE; index++) {
    unsigned int tmp = __VERIFIER_nondet_uint();
    __VERIFIER_assume(tmp > index);
    array[index] = tmp;
  }

  for (index = 0; index < SIZE; index++) {
    __VERIFIER_assert(array[index] > index);
  }

}
