/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc -o ignore:control */
/* CC_OPTS: */

/* Benchmark used to verify Chimdyalwar, Bharti, et al. "VeriAbs: Verification by abstraction (competition contribution)."
International Conference on Tools and Algorithms for the Construction and Analysis of Systems. Springer, Berlin, Heidelberg, 2017.*/

// V: small.10 CC_OPT: -DSIZE=10
// V: small.100 CC_OPT: -DSIZE=100 TAGS: big
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
  unsigned int index1;
  unsigned int index2;
  unsigned int loop_entered = 0;

  index1 =  __VERIFIER_nondet_uint();
  __VERIFIER_assume(index1 < SIZE);
  index2 =  __VERIFIER_nondet_uint();
  __VERIFIER_assume(index2 < SIZE);

  while (index1 < index2) {
    __VERIFIER_assert((index1 < SIZE) && (index2 < SIZE));
    __VERIFIER_assume(array[index1] == array[index2]);
    index1++;
    index2--;
    loop_entered = 1;
  }

  if (loop_entered) {
    while (index2 < index1) {
      __VERIFIER_assert(array[index1] == array[index2]);
      index2++;
      index1--;
    }
  }

}
