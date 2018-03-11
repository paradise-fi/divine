#ifndef _FS_REPLAY_PARSE_H_
#define _FS_REPLAY_PARSE_H_

#include <dios/core/stdlibwrap.hpp>
#include <tuple>
#include <cstring>
#include <type_traits>

namespace __dios
{

struct SyscallSnapshot 
{
    int number;
    String _inputs;
    int _errno;
    SyscallSnapshot *next;
};

using syscallSet = __dios::Set< SyscallSnapshot * >;

template< typename Type >
bool getArg( String& _inputs, Out <Type> process ) 
{
    int cut = sizeof( bool );
    if ( _inputs.size() < cut ) 
    {
        return false;
    }

    auto validity = _inputs.substr( 0, cut );
    bool isNull = *reinterpret_cast< const bool * >( validity.data());
    _inputs = _inputs.substr( cut );

    if ( isNull )
        return true;

    cut = sizeof( *Type( process ));
    if ( _inputs.size() < cut )
        return false;

    auto data = _inputs.substr( 0, cut );
    using T = typename std::remove_pointer< Type >::type;
    Type out = Type( process );
    *out = *reinterpret_cast<const T *>(data.data());
    _inputs = _inputs.substr( cut );
    return true;
}

template< typename Type >
bool getArg( String& _inputs, Mem <Type> process ) 
{
    int cut = sizeof( int );
    if ( _inputs.size() < cut ) 
    {
        return false;
    }

    auto lengthData = _inputs.substr( 0, cut );
    int length = *reinterpret_cast<const int *>(lengthData.data());
    _inputs = _inputs.substr( cut );

    if ( length <= 0 )
        return true;

    if ( _inputs.size() < length ) 
        return false;

    auto data = _inputs.substr( 0, length );
    Type memory = Type( process );
    _inputs = _inputs.substr( length );
    return std::strncmp( data.data(), memory, length ) == 0;
}

template< typename Type,
        class NoPointer = std::remove_pointer <Type>,
        typename = std::enable_if_t< ( !std::is_const< typename NoPointer::type >::value ) >
>
bool getArg( String& _inputs, Count <Type> process, bool = true ) 
{
    int cut = sizeof( int );
    if ( _inputs.size() < cut )
        return false;

    auto lengthData = _inputs.substr( 0, cut );
    int length = *reinterpret_cast<const int *>(lengthData.data());
    _inputs = _inputs.substr( cut );

    if ( length <= 0 ) {
        return true;
    }

    if ( _inputs.size() < length )
        return false;

    auto data = _inputs.substr( 0, length );
    char *memory = reinterpret_cast<char *>(Type( process ));
    std::memcpy( memory, data.data(), length );
    _inputs = _inputs.substr( length );
    return true;
}


template< typename Type,
        class NoPointer = std::remove_pointer <Type>,
        typename = std::enable_if_t <std::is_const< typename NoPointer::type >::value>
>
bool getArg( String& _inputs, Count <Type> process ) 
{
    const char *data = reinterpret_cast<const char * >(Type( process ));
    return getArg( _inputs, Mem< const char * >( data ));
}

template< typename Type,
        typename = std::enable_if_t< ( !std::is_pointer< Type >::value ) >
>
bool getArg( String& _inputs, Struct <Type> process, bool = false ) 
{
    int cut = sizeof( Type );
    if ( _inputs.size() < cut )
        return false;

    auto data = _inputs.substr( 0, cut );
    const char *structure = reinterpret_cast<const char *>(&Type( process ));
    return std::strncmp( data.data(), structure, cut ) == 0;

}

template< typename Type,
        typename = std::enable_if_t <std::is_pointer< Type >::value>
>
bool getArg( String& _inputs, Struct <Type> process ) 
{
    int cut = sizeof( bool );
    if ( _inputs.size() < cut ) 
    {
        return false;
    }

    auto lengthData = _inputs.substr( 0, cut );
    bool isNull = *reinterpret_cast<const bool *>(lengthData.data());
    _inputs = _inputs.substr( cut );

    if ( isNull )
        return true;

    int length = sizeof( *Type( process ));
    if ( _inputs.size() < length )
        return false;

    auto data = _inputs.substr( 0, length );
    char *structure = reinterpret_cast<char *>(Type( process ));
    memcpy( structure, data.data(), length );
    _inputs = _inputs.substr( length );
    return true;

}

template< typename Type >
bool getArg( String& _inputs, Type process ) 
{
    int cut = sizeof( Type );
    if ( _inputs.size() < cut ) {
        return false;
    }

    auto data = _inputs.substr( 0, cut );
    const Type *variable = reinterpret_cast<const Type *>(data.data());
    _inputs = _inputs.substr( cut );
    return *variable == process;
}


template< typename Out, class... Args >
bool tryParse( String& _inputs, UnVoid <Out>& rv, Pad, Args ... ) 
{
    if ( _inputs.size() != rv.size())
        return false;
    if ( rv.isVoid )
        return true;
    *rv.address() = *reinterpret_cast<const typename UnVoid< Out >::type *>(_inputs.data());
    return true;
}

template< typename Out >
bool tryParse( String& _inputs, UnVoid <Out>& rv, Pad ) 
{
    if ( _inputs.size() != rv.size())
        return false;
    if ( rv.isVoid )
        return true;
    rv.store( *reinterpret_cast<const typename UnVoid< Out >::type *>(_inputs.data()));
    return true;
}

template< typename Out, typename Type, class... Args >
bool tryParse( String& _inputs, UnVoid <Out>& rv, Type process, Args ... args ) 
{
    if ( !getArg( _inputs, process ))
        return false;
    return tryParse( _inputs, rv, args... );
}

}

#endif //_FS_REPLAY_PARSE_H_
