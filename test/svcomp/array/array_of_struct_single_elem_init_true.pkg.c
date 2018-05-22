/* TAGS: c sym todo */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
/* CC_OPTS: */

// V: v.10 CC_OPT: -DSIZE=10
// V: v.100 CC_OPT: -DSIZE=100
// V: v.1000 CC_OPT: -DSIZE=1000
// V: v.10000 CC_OPT: -DSIZE=10000 TAGS: big
// V: v.100000 CC_OPT: -DSIZE=100000 TAGS: big

extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern int __VERIFIER_nondet_int(void);
void __VERIFIER_assert(int cond) { if(!(cond)) { ERROR: __VERIFIER_error(); } }

#define NULL (void *)0

struct S
{
	int p;
	int n;
};

struct S a[SIZE];

int main()
{

	int i;

	//init and update
	for (i = 0; i < SIZE; i++)
	{
		int q = __VERIFIER_nondet_int();
		struct S s;
		if (s.n == 0)
		{
		    s.p =10 ;
		}
		else
		{
	 		s.p = 20;
		}

		a[i] = s;
	}

	a[3].p = 30;
	a[3].n = 40;

	//check 2
	for (i = 0; i < SIZE; i++)
	{
		struct S s1 = a[i];
		if (i != 3 && s1.n == 0)
		{
			__VERIFIER_assert(s1.p == 10);
		}
	}
	return 0;
}

