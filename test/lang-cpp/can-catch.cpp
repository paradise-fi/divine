/* TAGS: min c++ */
/* CC_OPTS: -I/dios */
#include <libcxxabi/src/private_typeinfo.h>
#include <memory>
#include <exception>
#include <cassert>

using namespace __cxxabiv1;

int main()
{
    std::exception e;
    std::bad_alloc ba;
    int i;

    auto &ba_t = typeid( ba );
    auto &e_t = typeid( e );
    auto &int_t = typeid( 7 );

    const __shim_type_info *ba_st = reinterpret_cast< const __shim_type_info * >( &ba_t );
    const __shim_type_info *e_st = reinterpret_cast< const __shim_type_info * >( &e_t );
    const __shim_type_info *int_st = reinterpret_cast< const __shim_type_info * >( &int_t );

    void *adj_ba = &ba;
    void *adj_i = &i;
    void *adj_e = &e;

    assert( int_st->can_catch( int_st, adj_i ) );
    assert( e_st->can_catch( e_st, adj_e ) );
    assert( e_st->can_catch( ba_st, adj_ba ) );

    assert( !int_st->can_catch( e_st, adj_e ) );
    assert( !ba_st->can_catch( e_st, adj_e ) );
    assert( !ba_st->can_catch( int_st, adj_i ) );
}
