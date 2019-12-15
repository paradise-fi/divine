// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#include <rst/mstring.hpp>

#include <rst/common.hpp>
#include <rst/term.hpp>
#include <rst/constant.hpp>

#include <cstdint>

namespace __dios::rst::abstract {

    #define INSTANTIATE(type, index, character) \
        using type = mstring_t< index, character >; \
        template type::mstring_ptr type::lift_any( unsigned size ) noexcept; \
        template type::mstring_ptr type::lift_one( const char * str ) noexcept; \
        \
        template type::character_t type::op_load( type::mstring_ptr, bitwidth_t ) noexcept; \
        template void type::op_store( abstract_value_t, abstract_value_t, bitwidth_t ) noexcept; \
        template type::mstring_ptr type::op_gep( size_t, abstract_value_t, abstract_value_t ) noexcept; \
        \
        template type::index_t type::fn_strlen( type::mstring_ptr ) noexcept; \
        template type::character_t type::fn_strcmp( abstract_value_t, abstract_value_t ) noexcept; \
        template type::mstring_ptr type::fn_strcat( abstract_value_t, abstract_value_t ) noexcept; \
        template type::mstring_ptr type::fn_strcpy( abstract_value_t, abstract_value_t ) noexcept; \
        template type::mstring_ptr type::fn_strchr( abstract_value_t, abstract_value_t ) noexcept;


    using symbolic_value_t = term_t;
    INSTANTIATE( sym_mstring, symbolic_value_t, symbolic_value_t )

    using constant_value_t = constant_t;
    INSTANTIATE( const_mstring, constant_value_t, constant_value_t )

    extern "C" {
        _LART_AGGREGATE
        char * __mstring_from_string( const char * buff )
        {
            return stash_abstract_value< char * >( const_mstring::lift_one( buff ) );
        }

        _LART_AGGREGATE
        char * __mstring_val( unsigned len )
        {
            return stash_abstract_value< char * >( const_mstring::lift_any( len ) );
        }
    }

} // namespace __dios::rst::abstract