#include <iostream>
#include <cstdlib>
#include "circuit.h"

#include "../divine/generator/custom-api.h"

// An example circuit.

struct Example1 {
    Input< int > input1, input2, input3, input4;
    Gain< int > gain;
    Sum< int > sum;
    Product< int > product;
    MinMax< int > min;

    Output< int > output1, output2;

    Example1() : gain( 2 ), min( MinMax< int > ::Min ) {
        connect( input1, sum.in( 0 ) );
        connect( input2, sum.in( 1 ) );
        connect( sum, output1 );

        connect( input3, gain.in );

        connect( sum, min.a );
        connect( gain, min.b );

        connect( min, product.a );
        connect( input4, product.b );

        connect( product, output2 );
    }

};

template< typename T >
struct Flip : Value< T > {
    In< T > a, b;

    T get() {
        return ( a->get() < b->get() ) ? a->get() : 0;
    }
};

struct Example2 {
    Input< int > a, limit;
    Delay< int > d1, d2;
    Sum< int > add;
    Flip< int > min;

    Example2() {
        connect( a, d1.in );
        connect( d1, add.in( 0 ) );
        connect( d2, add.in( 1 ) );
        connect( add, min.a );
        connect( limit, min.b );
        connect( min, d2.in );
    }

};

void example1()
{
    Example1 ex;
    Input< int > in( 2 );
    connect( in, ex.input1 ); // = 2;
    ex.input2 = 5;
    ex.input3 = 8;
    ex.input4 = 2;

    std::cout << ex.output1.get() << std::endl;
    std::cout << ex.output2.get() << std::endl;
}

#ifdef STANDALONE
int main() {
    example1();
}
#endif

#ifndef STANDALONE
static inline char *make( CustomSetup *setup, char **to )
{
    int total = setup->state_size + setup->slack;
    *to = (char *) pool_allocate_blob( setup->cpool, total );
    memset ((*to) + 4, 0, setup->slack);
    return ((*to) + setup->slack + 4); // FIXME; 4 = BlobHeader size
}

System &system( CustomSetup *setup )
{
    return *(System *)setup->custom;
}

extern "C" bool prop_limit_active( CustomSetup *setup, char *state ) {
    System &sys = system( setup );
    char *in = state + setup->slack + 4;
    sys.read( in );
    Example2 *ex = sys.circuit< Example2 >();
    if (ex->limit.get() == ex->add.get())
        return true;
    return false;
}

extern "C" void get_system_initial( CustomSetup *setup, char **to )
{
    char *v = make( setup, to );
    system( setup ).write( v );
}

extern "C" int get_system_successor( CustomSetup *setup, int handle, char *from, char **to )
{
    /* a in 0-5, limit in 0-5 */
    if ( handle > 6 )
        return 0;

    char *in = from + setup->slack + 4;
    system( setup ).read( in );
    char *v = make( setup, to );
    system( setup ).tick();
    Example2 *ex = system( setup ).circuit< Example2 >();
    ex->a = handle / 2;
    ex->limit = handle % 2;
    system( setup ).write( v );

    return handle + 1;
}

extern "C" char *system_show_node( CustomSetup *setup, char *from, int )
{
    char *in = from + setup->slack + 4;
    system( setup ).read( in );
    std::string s = system( setup ).print();
    char *ret = (char *) malloc( s.length() + 1 );
    std::copy( s.begin(), s.end(), ret );
    ret[ s.length() ] = 0;
    return ret;
}

extern "C" void system_setup( CustomSetup *setup )
{
    Example2 *ex = new Example2();

    ex->a = 5;
    ex->limit = 30;

    System *sys = new System();
    sys->_circuit = ex;
    sys->add( ex->d1 ).add( ex->d2 ).add( ex->a ).add( ex->limit );

    setup->state_size = sys->size();
    setup->custom = sys;
}
#endif
