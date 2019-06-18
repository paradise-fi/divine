// // -*- C++ -*- (c) 2016 Katarina Kejstova

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
#include <dios/sys/stdlibwrap.hpp>
#include <sys/resource.h>

#include <dios/proxy/passthru-types.h>
#include <dios/proxy/passthru-process.h>

/*
* For work with memory
*/
#ifdef __divine__
# define FS_MALLOC( x ) __vm_obj_make( x )
# define FS_PROBLEM( msg ) __dios_fault( _VM_Fault::_VM_F_Assert, msg )
# define FS_FREE( x ) __dios::delete_object( x )
#endif

namespace __dios
{

using String = __dios::String;
using brick::hlist::TypeList;

static size_t getPos( size_t num ) {
    return 7 - num + 1;
}

template< typename InputTuple, class... Args >
int parse( __dios::Vector <String>& _out, InputTuple currTup, Pad, Args ... )
{
    return brick::tuple::pass( __vm_syscall, currTup );
}

template< typename InputTuple >
int parse( __dios::Vector <String>& _out, InputTuple currTup, Pad )
{
    return brick::tuple::pass( __vm_syscall, currTup );
}


template< typename InputTuple, typename Type, class... Args >
int parse( __dios::Vector <String>& _out, InputTuple tuple, Type process, Args ... args );


//parsing if Type is not Count<X>
template< typename InputTuple, typename Type, class... Args >
int parse( std::false_type, __dios::Vector <String>& _out, InputTuple currTup, Type process, Args ...args )
{
    auto toAdd = getArg( process );

    auto res = std::tuple_cat( currTup, toAdd );
    auto result = parse( _out, res, args... );
    processResult( getPos( sizeof...( args )), _out, process );
    return result;
}

//parsing if Type is Count<X>
template< typename InputTuple, typename Type, class... Args >
int parse( std::true_type, __dios::Vector <String>& _out, InputTuple currTup, Type process, Args ...args )
{
    auto toAdd = getArg( process );
    auto res = std::tuple_cat( currTup, toAdd );
    auto result = parseWithCount( _out, res, args... );
    processResult( getPos( sizeof...( args )), _out, process );
    return result;
}

//parsing if previous was Count<X> and thus next argument defines size argument for it
template< typename InputTuple, typename Type, class... Args >
int parseWithCount( __dios::Vector <String>& _out, InputTuple currTup, Type process, Args ...args )
{
    auto toAdd = getArg( process );

    std::get< std::tuple_size< InputTuple >::value - 2 >( currTup ) = std::get< 1 >( toAdd );
    char out[std::get< 1 >( toAdd ) + 1];


    auto res = std::tuple_cat( currTup, toAdd );
    auto result = parse( _out, res, args... );
    processResult( getPos( sizeof...( args )), _out, process );
    return result;
}

template< typename InputTuple, typename Type, class... Args >
int parse( __dios::Vector <String>& _out, InputTuple tuple, Type process, Args ... args )
{
    return parse( typename IsCount< Type >::type(), _out, tuple, process, args... );
}

namespace fs {

    template< typename Next >
    struct PassThrough : public Next {

        template< typename Setup >
        void setup( Setup s )
        {
            traceAlias< PassThrough >( "{PassThrough}" );

            if ( __dios_clear_file( "passthrough.out" ) == 0 )
                __vm_ctl_flag( 0, _VM_CF_Error );

            Next::setup( s );
        }

        void finalize()
        {
            Next::finalize();
        }

        template< typename ret >
        void writeOut( UnVoid <ret>& output )
        {
            // for parsing we need to know the  length of data first
            size_t fullFize = 0;
            for ( auto& word : _out )
            {
                fullFize += word.size();
            }

            fullFize += sizeof( *output.address());
            __dios_trace_out( reinterpret_cast<const char *>(&fullFize), sizeof( &fullFize ));

            for ( auto it = _out.rbegin(); it != _out.rend(); ++it )
            {
                __dios_trace_out( it->data(), it->size());
            }
            _out.clear();
            __dios_trace_out( reinterpret_cast<const char *>(output.address()), sizeof( *output.address()));
        }


    #include <dios/macro/tags_to_class>

    #define SYSCALL_DIOS(...)
    #define SYSCALL( name, schedule, ret, arg ) \
            ret name arg { \
                UnVoid< ret > rv; \
                int outType;                                                 \
                switch (rv.size()) {                                       \
                    case 4 :  outType = _VM_SC_Int32 | _VM_SC_Out ; break;   \
                    case 8 : outType = _VM_SC_Int64 | _VM_SC_Out ; break;    \
                }                                                            \
                __dios_trace_out("SYSCALL:", 8); \
                int sysnum = static_cast<int>(_HOST_SYS_ ## name);\
                __dios_trace_out(reinterpret_cast<const char *>(&sysnum), sizeof(int)); \
                auto input =  std::make_tuple(_HOST_SYS_ ## name, outType, rv.address()); \
                 *__dios_errno() = 0;\
                *__dios_errno() =  parse(_out, input, _1, _2, _3, _4, _5, _6, _7); \
                writeOut(rv);\
                __dios_trace_out(reinterpret_cast<const char *>(__dios_errno()), sizeof(*__dios_errno())); \
                return rv.get();\
            }

    #include <sys/syscall.def>

    #undef SYSCALL_DIOS
    #undef SYSCALL

    #include <dios/macro/tags_to_class.cleanup>

        int open( const char *pathname, OFlags flags, mode_t mode )
        {
            UnVoid< int > rv;
            int outType;
            switch ( rv.size()) {
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
            __dios_trace_out( "SYSCALL:", 8 );
            int sysnum = static_cast<int>(_HOST_SYS_open);
            __dios_trace_out( reinterpret_cast<const char *>(&sysnum), sizeof( int ));
            auto input = std::make_tuple( _HOST_SYS_open, outType, rv.address());
            *__dios_errno() = 0;
            *__dios_errno() = parse( _out, input, Mem< const char * >( pathname ), flags, mode, _4 );
            writeOut( rv );
            __dios_trace_out( reinterpret_cast<const char *>(__dios_errno()), sizeof( *__dios_errno()));
            return rv.get();
        }

        int fcntl( int fd, int cmd, va_list *vl )
        {
            UnVoid< int > rv;
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
            __dios_trace_out( "SYSCALL:", 8 );
            int sysnum = static_cast<int>(_HOST_SYS_fcntl);
            __dios_trace_out( reinterpret_cast<const char *>(&sysnum), sizeof( int ));
            *__dios_errno() = 0;
            switch ( cmd )
            {
                case F_DUPFD:
                case F_DUPFD_CLOEXEC:
                case F_SETFD:
                case F_SETFL:
                {
                    int flag = va_arg( *vl, int );
                    va_end( *vl );
                    auto input = std::make_tuple( _HOST_SYS_fcntl, outType, rv.address());
                    *__dios_errno() = parse( _out, input, fd, cmd, flag, _4 );
                }
                    break;
                default:
                {
                    auto input = std::make_tuple( _HOST_SYS_fcntl, outType, rv.address());
                    *__dios_errno() = parse( _out, input, fd, cmd, _3 );
                    va_end( *vl );
                }
            }
            writeOut( rv );
            __dios_trace_out( reinterpret_cast<const char *>(__dios_errno()), sizeof( *__dios_errno()));
            return rv.get();
        }

    private:
        __dios::Vector< String > _out;
    };


} // namespace fs
} // namespace __dios
