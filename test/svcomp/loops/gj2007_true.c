/* TAGS: c */
/* VERIFY_OPTS: */
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

int main() {
    int x = 0;
    int y = 50;
    while(x < 100) {
	if (x < 50) {
	    x = x + 1;
	} else {
	    x = x + 1;
	    y = y + 1;
	}
    }
    __VERIFIER_assert(y == 100);
    return 0;
}
