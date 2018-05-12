/* TAGS: c sym todo */
/* VERIFY_OPTS: --symbolic */
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
  int n,i,k;
  n = __VERIFIER_nondet_int();
  if (!(n <= LARGE_INT)) return 0;
  k = n;
  i = 0;
  while( i < n ) {
    k--;
    i = i + 2;
  }

  int j = 0;

  while( j < n/2 ) {
    __VERIFIER_assert(k > 0);
    k--;
    j++;
  }
  return 0;
}
