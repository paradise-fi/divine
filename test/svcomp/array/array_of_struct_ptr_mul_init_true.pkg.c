/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
/* CC_OPTS: */

// V: v.10 CC_OPT: -DSIZE=10 TAGS: big
// V: v.100 CC_OPT: -DSIZE=100 TAGS: big
// V: v.1000 CC_OPT: -DSIZE=1000 TAGS: big
// V: v.10000 CC_OPT: -DSIZE=10000 TAGS: big
// V: v.100000 CC_OPT: -DSIZE=100000 TAGS: big

extern void __VERIFIER_error() __attribute__ ((__noreturn__));
void __VERIFIER_assert(int cond) { if(!(cond)) { ERROR: __VERIFIER_error(); } }

struct S {
	unsigned short p;
	unsigned short q;
} a[SIZE];

short __VERIFIER_nondet_short();
unsigned char __VERIFIER_nondet_uchar();
int main()
{
	unsigned char k;

	int i;
	for (i  = 0; i < SIZE ; i++)
	{
		a[i].p = i;
		a[i].q = i ;
	}

	for (i = 0; i < SIZE; i++)
	{
		if ( __VERIFIER_nondet_short())
		{
			k = __VERIFIER_nondet_uchar();
			a[i].p = k;
			a[i].q = k * k ;
		}
	}

	for (i = 0; i < SIZE; i++)
	{
		__VERIFIER_assert(a[i].p == a[i].q || a[i].q == a[i].p * a[i].p);
	}

	return 0;
}

