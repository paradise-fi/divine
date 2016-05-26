// __syscall_* and _start taken from MUSL library,
// remaining parts by by Vladimir Still

#include <sys.h>
#include <stdint.h>
#include <syscall.h>

static inline long __syscall_ret_zero( unsigned long r ) { return r > -4096UL ? 0 : r; }
long __syscall_ret( unsigned long r ) { return r > -4096UL ? -1 : r; }

#define MAP_SHARED     0x01
#define MAP_ANON       0x20
#define MAP_ANONYMOUS  MAP_ANON

#define PROT_NONE      0
#define PROT_READ      1
#define PROT_WRITE     2
#define PROT_EXEC      4


void *__native_mmap_anon( size_t len )
{
	if (len >= PTRDIFF_MAX)
        return 0;
	return (void *)__syscall_ret_zero( __syscall( SYS_mmap,
                0, len, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0 ) );
}

_Noreturn void __native_exit( int ec )
{
	__syscall( SYS_exit_group, ec );
	for (;;)
        __syscall( SYS_exit, ec );
}

long __native_write( int fd, const void *buf, size_t count )
{
	return syscall_cp(SYS_write, fd, buf, count);
}

void __native_putStr( const char *buf, size_t count ) {
    __native_write( 1, buf, count );
}

void __native_putErrStr( const char *buf, size_t count ) {
    __native_write( 2, buf, count );
}

static long __syscall_cp_c(syscall_arg_t nr,
                 syscall_arg_t u, syscall_arg_t v, syscall_arg_t w,
                 syscall_arg_t x, syscall_arg_t y, syscall_arg_t z)
{
	return (__syscall)(nr, u, v, w, x, y, z);
}

long (__syscall_cp)(syscall_arg_t nr,
                    syscall_arg_t u, syscall_arg_t v, syscall_arg_t w,
                    syscall_arg_t x, syscall_arg_t y, syscall_arg_t z)
{
	return __syscall_cp_c(nr, u, v, w, x, y, z);
}

__asm__(
".global __syscall\n"
".hidden __syscall\n"
".type __syscall,@function\n"
"__syscall:\n"
"	movq %rdi,%rax\n"
"	movq %rsi,%rdi\n"
"	movq %rdx,%rsi\n"
"	movq %rcx,%rdx\n"
"	movq %r8,%r10\n"
"	movq %r9,%r8\n"
"	movq 8(%rsp),%r9\n"
"	syscall\n"
"	ret\n"
);

// a _start function which alignes stack to 16 B and calls __native_start which
// is expected to take no parameters
__asm__(
".text \n"
".global _start \n"
"_start: \n"
"	xor %rbp,%rbp \n" // zero base pointer (expected by ABI)
"	andq $-16,%rsp \n" // align stack
"	call __native_start\n"
);
