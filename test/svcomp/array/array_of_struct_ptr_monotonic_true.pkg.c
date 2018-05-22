/* TAGS: c sym todo */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
/* CC_OPTS: */

// V: v.10 CC_OPT: -DSIZE=10
// V: v.100 CC_OPT: -DSIZE=100
// V: v.1000 CC_OPT: -DSIZE=1000
// V: v.10000 CC_OPT: -DSIZE=10000 TAGS: big
// V: v.100000 CC_OPT: -DSIZE=100000 TAGS: big

typedef unsigned int size_t;
extern void *malloc (size_t __size) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__malloc__)) ;
extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern int __VERIFIER_nondet_int(void);
void __VERIFIER_assert(int cond) { if(!(cond)) { ERROR: __VERIFIER_error(); } }

struct S
{
	int t;
	int * p1;
};

struct S* a[SIZE];

int main()
{
	int i;
	for(i = 0; i < SIZE; i = i+2)
	{
		struct S* s = (struct S*) malloc(sizeof(struct S));
		s->t = __VERIFIER_nondet_int();
		if (s->t == 10)
			s->p1 = (int *) malloc(sizeof(int));
		a[i] = s;
	}

	for(i = 0; i < SIZE; i = i+2)
	{
		struct S* u = a[i];
		if (u->t == 10)
		{
			__VERIFIER_assert(u->p1 != (void *) 0);
		}
	}
}
