#pragma once

#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <sstream>
#include <functional>
#include <tuple>

#include <bits/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/divm.h>
#include <dios.h>
#include <dios/sys/syscall.hpp>
#include <brick-tuple>
#include <brick-hlist>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <dios/sys/stdlibwrap.hpp>
#include <dios/proxy/passthru-types.h>
#include <dios/proxy/replay-parse.h>


using String = __dios::String;
using srtVector = __dios::Vector< String >;


/*
* For work with memory
*/
#ifdef __divine__
# define FS_PROBLEM( msg ) __dios_fault( _VM_Fault::_VM_F_Assert, msg )
# define FS_FREE( x ) __dios::delete_object( x )
#endif


namespace __dios {
namespace fs {

template< typename Next >
struct Replay : public Next {

    template< typename Setup >
    void setup( Setup s )
    {
        traceAlias< Replay >( "{Replay}" );
        if ( !init( "passthrough.out" ) )
        {
            __vm_ctl_flag( 0, _VM_CF_Error );
            return;
        }
        Next::setup( s );
    }

    void finalize()
    {
        while ( !_currents.empty()) {
            SyscallSnapshot *node = *_currents.begin();
            _currents.erase( node );
            if ( node->next != nullptr )
                _currents.insert( node->next );
            __vm_obj_free( node );
        }
        Next::finalize();
    }

    bool init( const String& name ) {
        int fd = -1;
        const char *filename = name.c_str();
        __dios_trace_f( "Opening file: %s... \n", filename );
        auto err = __vm_syscall( _HOST_SYS_open,
                                 _VM_SC_Out | _VM_SC_Int32, &fd,
                                 _VM_SC_In | _VM_SC_Mem, strlen( filename ) + 1, filename,
                                 _VM_SC_In | _VM_SC_Int32, O_RDONLY,
                                 _VM_SC_In | _VM_SC_Int32, 0000 );

        if ( fd < 0 ) {
            __dios_trace_f( "Unable to open file: %s, please check if file exist or permissions \n", filename );
            return false;
        }

        bool success = false;
        success = parseFile( fd );

        if ( !success ) {
            __dios_trace_f( "Error by parsing the file: %s\n", filename );
        }

        int result;
        err = __vm_syscall( _HOST_SYS_close,
                            _VM_SC_Out | _VM_SC_Int32, &result,
                            _VM_SC_In | _VM_SC_Int32, fd );

        return success;

    }

    int readWrap( int fd, char *buffer, int length ) {
        int red, err;
        err = __vm_syscall( _HOST_SYS_read,
                            _VM_SC_Out | _VM_SC_Int32, &red,
                            _VM_SC_In | _VM_SC_Int32, fd,
                            _VM_SC_Out | _VM_SC_Mem, length, buffer,
                            _VM_SC_In | _VM_SC_Int32, length );
        return red;
    }

    bool parseFile( int fd )
    {
        SyscallSnapshot *prev = nullptr;
        SyscallSnapshot *next = nullptr;
        while ( true ) {
            next = new_object< SyscallSnapshot >();
            bool stop = false;
            if ( !getSyscall( fd, next, &stop ))
            {
                __vm_obj_free( next );
                return false;
            }

            if ( stop )
            {
                return true;
            }

            if ( prev == nullptr )
            {
                _currents.insert( next );
                prev = next;
            } else
            {
                prev->next = next;
                prev = next;
            }
        }
    }

    bool getSyscall( int fd, SyscallSnapshot *snapshot, bool *shouldStop )
    {
        ssize_t red = 0;
        size_t length = 8 + sizeof( int ); //8 for SYSCALL:
        char nameBuf[length];
        int sysNumber = 0;

        red = readWrap( fd, nameBuf, length );

        //No data left
        if ( red == 0 )
        {
            *shouldStop = true;
            return true;
        }

        if ( red != length )
        {
            return false;
        }

        if ( strncmp( "SYSCALL:", nameBuf, 8 ))
        {
            return false;
        }

        //NAME
        char *number = nameBuf + 8;
        snapshot->number = *reinterpret_cast< int *>(number);

        //INPUT DATA + OUTPUT
        length = sizeof( size_t ); //for inputData length
        char inputDataLength[length];

        red = readWrap( fd, inputDataLength, length );
        if ( red != length ) {
            return false;
        }

        size_t inputLength = *reinterpret_cast< size_t *>(inputDataLength);
        char inputData[inputLength];

        red = readWrap( fd, inputData, inputLength );
        if ( red != inputLength ) {
            return false;
        }

        snapshot->_inputs = String( inputData, inputLength );

        //ERRNO
        length = sizeof( int );
        char errnoBuf[length];
        red = readWrap( fd, errnoBuf, length );
        if ( red != length )
            return false;
        snapshot->_errno = *reinterpret_cast< int * >(errnoBuf);

        return true;

    }

                //TODO : line 205 + 199 - co urobit ak zle!
    #include <dios/macro/tags_to_class>
    #define SYSCALL_DIOS(...)

    #define SYSCALL( name, schedule, ret, arg ) \
        ret name arg { \
            UnVoid< ret > rv; \
            if(!isProcessible(_HOST_SYS_ ## name))  { \
                __vm_cancel(); \
                return rv.get(); }\
            int outType;                                                 \
            switch (rv.size()) {                                       \
                case 4 :  outType = _VM_SC_Int32 | _VM_SC_Out ; break;   \
                case 8 : outType = _VM_SC_Int64 | _VM_SC_Out ; break;    \
            }                                                            \
            if (!parse(_HOST_SYS_ ## name, rv, _1, _2, _3, _4, _5, _6, _7)) {\
                __vm_cancel(); \
                return rv.get();\
            }\
            return rv.get();\
        }

    #include <sys/syscall.def>

    #undef SYSCALL_DIOS
    #undef SYSCALL

    #include <dios/macro/tags_to_class.cleanup>


        /*
        gives a guess if it is even reasonable to try parsing given system call
        can be extended by some heuristics in the future
        */
        bool isProcessible( int sysNumber )
        {
            for ( auto& item : _currents )
            {
                if ( item->number == sysNumber )
                    return true;
            }
            return false;
        }

        template< typename Out, class... Args >
        bool parse( int sysNumber, UnVoid <Out>& rv, Args ... args )
        {
            for ( auto& item : _currents ) {
                if ( item->number == sysNumber ) {
                    //need to make copy of the string
                    auto copy = item->_inputs;
                    if ( tryParse( copy, rv, args... )) {
                        if ( item->next )
                            _currents.insert( item->next );
                        *__dios_errno() = item->_errno;
                        _currents.erase( item );
                        // __vm_obj_free(item); //TODO delete this memory
                        return true;
                    }
                }
            }
            return false;
        }

        int open( const char *pathname, OFlags flags, mode_t mode )
        {
            UnVoid< int > rv;
            if ( !isProcessible( _HOST_SYS_open ))
            {
                __vm_cancel();
                return rv.get();
            }
            int outType;
            switch ( rv.size())
            {
                case 4 :
                    outType = _VM_SC_Int32 | _VM_SC_Out;
                    break;
                case 8 :
                    outType = _VM_SC_Int64 | _VM_SC_Out;
                    break;
            }
            if ( !( flags & O_CREAT ))
            {
                mode = 0;
            }
            if ( !parse( _HOST_SYS_open, rv, Mem< const char * >( pathname ), flags, mode, _5, _6, _7 ))
            {
                __vm_cancel();
                return rv.get();
            }
            return rv.get();
        }


        int fcntl( int fd, int cmd, va_list *vl )
        {
            UnVoid< int > rv;
            if ( !isProcessible( _HOST_SYS_fcntl ))
            {
                __vm_cancel();
                return rv.get();
            }
            int outType;
            switch ( rv.size())
            {
                case 4 :
                    outType = _VM_SC_Int32 | _VM_SC_Out;
                    break;
                case 8 :
                    outType = _VM_SC_Int64 | _VM_SC_Out;
                    break;
            }
            switch ( cmd )
            {
                case F_DUPFD:
                case F_DUPFD_CLOEXEC:
                case F_SETFD:
                case F_SETFL:
                {
                    int flag = va_arg( *vl, int );
                    va_end( *vl );
                    if ( !parse( _HOST_SYS_fcntl, rv, fd, cmd, flag, _5, _6, _7 ))
                    {
                        __vm_cancel();
                        return rv.get();
                    }
                }
                    break;
                default: {
                    va_end( *vl );
                    if ( !parse( _HOST_SYS_fcntl, rv, fd, cmd, _3, _4, _5, _6, _7 ))
                    {
                        __vm_cancel();
                        return rv.get();
                    }
                }
            }
            return rv.get();
        }


    private:
        syscallSet _currents;
        String _rest;
    };

} // namespace fs
} // namespace __dios __dios_trace_f("here1" # name);\
