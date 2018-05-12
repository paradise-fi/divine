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

int main(void) {
  int i, j;

 L0:
  i = 0;
 L1:
  while( __VERIFIER_nondet_int() && i < LARGE_INT){

    i++;
  }
  if(i >= 100) STUCK: goto STUCK; //  assume( i < 100 );
  j = 0;
 L2:
  while( __VERIFIER_nondet_int() && i < LARGE_INT ){
    i++;
    j++;
  }
  if(j >= 100) goto STUCK; // assume( j < 100 );
  __VERIFIER_assert( i < 200 ); /* prove we don't overflow z */
  return 0;
}
