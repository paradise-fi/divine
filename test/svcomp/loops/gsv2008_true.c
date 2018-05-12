/* TAGS: c sym todo */
/* VERIFY_OPTS: --symbolic */
// Source: Sumit Gulwani, Saurabh Srivastava, Ramarathnam Venkatesan: "Program
// Analysis as Constraint Solving", PLDI 2008.
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
    int x,y;
    x = -50;
    y = __VERIFIER_nondet_int();
    if (!(-1000 < y && y < LARGE_INT)) return 0;
    while (x < 0) {
	x = x + y;
	y++;
    }
    __VERIFIER_assert(y > 0);
    return 0;
}
