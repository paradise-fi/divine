#ifndef _FS_PASSTHRU_PROCESS_H_
#define _FS_PASSTHRU_PROCESS_H_

#include <dios/sys/stdlibwrap.hpp>
#include <tuple>

/*
* This file defines basic operations over structures from fs-passthru-types
*  for Passthrough mode. Provides functions for storing an type with flags into tuple
*  needed by __vm_syscall (getArg) and storing the value of system call that 
*  is currently being passed through in readable form (processResult).
*/
template< typename Type >
std::tuple< int, Type > getArg( Out <Type> process ) 
{
    switch ( sizeof( *Type( process ))) {
        case 8:
            return std::make_tuple( _VM_SC_Out | _VM_SC_Int64, Type( process ));
            break;
        case 4:
            return std::make_tuple( _VM_SC_Out | _VM_SC_Int32, Type( process ));
            break;
    }
}

template< typename Type >
std::tuple< int, size_t, Type > getArg( Mem <Type> process ) 
{

    using NoPointer = std::remove_pointer< Type >;
    if ( std::is_const< typename NoPointer::type >::value ) 
    {
        return std::make_tuple( _VM_SC_In | _VM_SC_Mem, strlen( reinterpret_cast<const char *>(Type( process ))) + 1,
                                process );
    } else 
    {
        return std::make_tuple( _VM_SC_Out | _VM_SC_Mem, strlen( reinterpret_cast<const char *>(Type( process ))) + 1,
                                process );
    }
}


template< typename Type >
std::tuple< int, size_t, Type > getArg( Count <Type> process ) 
{

    using NoPointer = std::remove_pointer< Type >;
    if ( std::is_const< typename NoPointer::type >::value ) 
    {
        return std::make_tuple( _VM_SC_In | _VM_SC_Mem, 0, Type( process ));
    } else 
    {
        return std::make_tuple( _VM_SC_Out | _VM_SC_Mem, 0, Type( process ));
    }
}

template< typename Type >
std::tuple< int, size_t, Type > getArg( Struct <Type> process ) 
{
    const size_t length =
            Type( process ) == nullptr ? 0
                                       : ( std::is_pointer< Type >::value ? sizeof( std::remove_pointer < Type > )
                                                                           : sizeof( Type ));

    using NoPointer = std::remove_pointer< Type >;
    if ( std::is_const< typename NoPointer::type >::value ) 
    {
        return std::make_tuple( _VM_SC_In | _VM_SC_Mem, length, Type( process ));
    } else 
    {
        return std::make_tuple( _VM_SC_Out | _VM_SC_Mem, length, Type( process ));
    }

}

template< typename Type >
std::tuple< int, Type > getArg( Type process ) 
{
    switch ( sizeof( Type )) 
    {
        case 8:
            return std::make_tuple( _VM_SC_In | _VM_SC_Int64, process );
            break;
        case 4:
            return std::make_tuple( _VM_SC_In | _VM_SC_Int32, process );
            break;
    }
}

using String = __dios::String;

/*
* _out is a vector of strings, where every argument is store in it's binary form
*  in case of strings, their size if passed first in 8 bytes (e.g. sizeof size_t)
*/

template< typename Type >
void processResult( size_t position, __dios::Vector <String>& _out, Mem <Type> process ) 
{
    if ( Type( process ) == nullptr || Type( process ) == NULL ) 
    {
        int len = 0;
        String out( reinterpret_cast<const char *>( &len ), sizeof( int ));
        _out.push_back( std::move( out ));
        return;
    }
    const char *val = reinterpret_cast<const char *>(Type( process ));
    int len = strlen( val );
    String out( reinterpret_cast<const char *>( &len ), sizeof( int ));
    out += String( val, len );
    _out.push_back( std::move( out ));
}

template< typename Type >
void processResult( size_t position, __dios::Vector <String>& _out, Count <Type> process ) 
{
    int size = *reinterpret_cast<const int *>(_out[ _out.size() - 1 ].c_str());
    if ( Type( process ) == nullptr || Type( process ) == NULL )
        size = -1;
    String out( reinterpret_cast<const char *>(&size), sizeof( int ));
    if ( size <= 0 ) 
    {
        _out.push_back( std::move( out ));
        return;
    }
    auto val = reinterpret_cast<const char *>(Type( process ));
    out += String( val, size );
    _out.push_back( std::move( out ));
}

template< typename Type,
        typename = std::enable_if_t <std::is_pointer< Type >::value>
>
void processResult( size_t position, __dios::Vector <String>& _out, Struct <Type> process ) 
{
    Type output = Type( process );
    bool isNull = ( output == nullptr );
    String out( reinterpret_cast<char *>(&isNull), sizeof( bool ));
    if ( !isNull )
        out += String( reinterpret_cast<const char *>(output), sizeof( *output ));
    _out.push_back( std::move( out ));
}

template< typename Type,
        typename = std::enable_if_t< ( !std::is_pointer< Type >::value ) >
>
void processResult( size_t position, __dios::Vector <String>& _out, Struct <Type> process, bool = false ) 
{
    _out.push_back( String( reinterpret_cast<const char *>(&Type( process )), sizeof( *Type( process ))));
}


template< typename Type >
void processResult( size_t position, __dios::Vector <String>& _out, Out <Type> process ) 
{
    Type output = Type( process );
    bool isNull = ( output == nullptr );
    String out( reinterpret_cast<char *>(&isNull), sizeof( bool ));
    if ( !isNull )
        out += String( reinterpret_cast<char *>(output), sizeof( *output ));
    _out.push_back( std::move( out ));
}

template< typename Type >
void processResult( size_t position, __dios::Vector <String>& _out, Type process ) 
{
    _out.push_back( String( reinterpret_cast<char *>(&process), sizeof( process )));
}

#endif //_FS_PASSTHRU_PROCESS_H_
