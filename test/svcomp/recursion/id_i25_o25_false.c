/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic */
extern int __VERIFIER_nondet_int();
extern void __VERIFIER_error(void);

int id(int x) {
  if (x==0) return 0;
  return id(x-1) + 1;
}

int main(void) {
  int input = 25;
  int result = id(input);
  if (result == 25) {
    ERROR: __VERIFIER_error(); /* ERROR */
  }
}