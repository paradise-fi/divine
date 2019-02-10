# TAGS: min
. lib/testcase

cat > file.c <<EOF
#include <sys/divm.h>

void print_hello()
{
    __vm_trace( _VM_T_Text, "hello world" );
}

void die()
{
    static int *bad;
    *bad = 0;
}

void has_args( int i ) {}

int main() {}
EOF

sim file.c <<EOF
+ ^# executing __boot at
> call print_hello
+ hello world
> call die
+ W: null pointer dereference.*in debug mode.*abandoned
> start
+ executing main()
> call print_hello
+ hello world
> call has_args
+ ERROR: the function must not take any arguments
> call not_defined
+ ERROR: the function 'not_defined' is not defined
EOF
