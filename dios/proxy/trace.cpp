#include <sys/trace.h>
#include <sys/divm.h>
#include <fcntl.h>
#include <string.h>

static void traceInFile( const char *file, const char *msg, size_t size ) noexcept
{
    int fd;
    auto err = __vm_syscall( _HOST_SYS_open,
            _VM_SC_Out | _VM_SC_Int32, &fd,
            _VM_SC_In | _VM_SC_Mem, strlen( file ) + 1, file,
            _VM_SC_In | _VM_SC_Int32, _HOST_O_WRONLY|_HOST_O_CREAT|_HOST_O_APPEND,
            _VM_SC_In | _VM_SC_Int32, 0666 );

    if (fd == -1)
        __dios_trace_f("Error by opening file");

   ssize_t written;
   // ssize_t toWrite = strlen( msg );

   err =  __vm_syscall( _HOST_SYS_write,
              _VM_SC_Out | _VM_SC_Int64, &written,
              _VM_SC_In | _VM_SC_Int32, fd,
              _VM_SC_In | _VM_SC_Mem, size, msg,
              _VM_SC_In | _VM_SC_Int64, size );

   if (!written)
        __dios_trace_f("Error by writing into file");

        err = __vm_syscall( _HOST_SYS_close,
                _VM_SC_Out | _VM_SC_Int32, &written, //reuse
                _VM_SC_In | _VM_SC_Int32, fd );
   if (written == -1)
        __dios_trace_f("Error by closing file");

}

void __dios_trace_out( const char *msg, size_t size) noexcept
{
    traceInFile("passthrough.out", msg, size);
}

int __dios_clear_file( const char *name )
{
    __vm_syscall( _HOST_SYS_unlink,
                  _VM_SC_In | _VM_SC_Mem, strlen( name ) + 1, name );

    return 1;
}
