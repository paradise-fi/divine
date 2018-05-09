/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic */
extern void __VERIFIER_error(void);

void f(int n) {
  if (n<3) return;
  n--;
  f(n);
  ERROR: __VERIFIER_error(); /* ERROR */
}

int main(void) {
  f(4);
}
