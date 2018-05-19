/* TAGS: c */
/* VERIFY_OPTS: */
extern void __VERIFIER_error(void);

void f(int n) {
  if (n<3) return;
  n--;
  f(n);
  ERROR: __VERIFIER_error();
}

int main(void) {
  f(2);
}
