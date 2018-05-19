/* TAGS: c */
/* VERIFY_OPTS: */
extern void __VERIFIER_error(void);

void f(int);
void f2(int);

void f(int n) {
  if (n<3) return;
  n--;
  f2(n);
  ERROR: __VERIFIER_error(); /* ERROR */
}

void f2(int n) {
  if (n<3) return;
  n--;
  f(n);
  ERROR: __VERIFIER_error(); /* ERROR */
}

int main(void) {
  f(4);
}
