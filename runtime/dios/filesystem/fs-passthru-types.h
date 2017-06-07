#ifndef _FS_PASSTHRU_TYPES_H_
#define _FS_PASSTHRU_TYPES_H_

/*
* This file defines basic structures to work with parameters
*  in Passthrough mode, e.g. to know the right format of passing them
*  to __vm_syscall
*/
template< typename T >
struct Mem {
    T value;

    Mem( T value ) : value( value ) { }

    operator T() {
        return value;
    }
};

template< typename T >
struct Count {
    T value;

    Count( T value ) : value( value ) {}

    operator T() {
        return value;
    }
};

template< typename T >
struct Out {
    T value;

    Out( T value ) : value( value ) {}

    operator T() {
        return value;
    }
};

template< typename T >
struct Struct {
    T value;

    Struct( T value ) : value( value ) {}

    operator T() {
        return value;
    }
};


/*
* UnVoid is used for output value
*/
template< typename T >
struct UnVoid {
    bool isVoid = false;
    using type = T;
    T t;

    T *address() {
        return &t;
    }

    T get() {
        return t;
    }

    void store( T val ) {
        t = val;
    }

    int size() {
        return sizeof( t );
    }

    T value() {
        return t;
    }
};

template< >
struct UnVoid< char * > {
    bool isVoid = false;
    using type = char *;
    char *t;

    void store( char *val ) {
        t = val;
    }

    char **address() {
        return &t;
    }

    char *get() {
        return t;
    }

    int size() {
        return sizeof( t );
    }

    int value() {
        return static_cast<int>(*t);
    }
};

template< >
struct UnVoid< void > {
    bool isVoid = true;
    using type = long;
    long unused;

    void get() {
    }

    void store( long ) {
    }

    long *address() {
        return &unused;
    }

    int size() {
        return sizeof( long );
    }

    long value() {
        return unused;
    }
};


template< typename T >
struct IsCout {
    using type = std::false_type;
};

template< typename T >
struct IsCout< Count< T>> {
    using type = std::true_type;
};

#endif // _FS_PASSTHRU_TYPES_H_