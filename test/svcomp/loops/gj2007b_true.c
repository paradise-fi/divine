/* TAGS: c sym todo */
/* VERIFY_OPTS: --symbolic */
// Source: Sumit Gulwani, Nebosja Jojic: "Program Verification as
// Probabilistic Inference", POPL 2007.
extern void __VERIFIER_error(void);
extern void __VERIFIER_assume(int);
void __VERIFIER_assert(int cond) {
  if (!(cond)) {
      ERROR: __VERIFIER_error();
  }
  return;
}
int __VERIFIER_nondet_int();
#define LARGE_INT 1000000

int main() {
    int x = 0;
    int m = 0;
    int n = __VERIFIER_nondet_int();
    while(x < n) {
	if(__VERIFIER_nondet_int()) {
	    m = x;
	}
	x = x + 1;
    }
    __VERIFIER_assert((m >= 0 || n <= 0));
    __VERIFIER_assert((m < n || n <= 0));
    return 0;
}
