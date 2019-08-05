// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2012-2019 Petr Ročkai <code@fixp.eu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once
#include <divine/vm/lx-slot.hpp>
#include <type_traits>

namespace divine::vm
{
   struct NoOp { template< typename T > NoOp( T ) {} };

   template< typename To > struct Convertible
   {
      template< typename From >
      static constexpr decltype( To( std::declval< From >() ), true ) value( unsigned ) { return true; }
      template< typename From >
      static constexpr bool value( int ) { return false ; }

      template< typename From >
      struct Guard {
         static const bool value = Convertible< To >::value< From >( 0u );
      };
   };

   template< typename To > struct SignedConvertible
   {
      template< typename From >
      struct Guard {
         static const bool value = std::is_arithmetic< typename To::Cooked >::value &&
            std::is_arithmetic< typename From::Cooked >::value &&
            Convertible< To >::template Guard< From >::value;
      };
   };

   template< typename T > struct IsIntegral
   {
      static const bool value = std::is_integral< typename T::Cooked >::value;
   };

   template< typename T > struct IsFloat
   {
      static const bool value = std::is_floating_point< typename T::Cooked >::value;
   };

   template< typename T > struct IntegerComparable
   {
      static const bool value = std::is_integral< typename T::Cooked >::value ||
         std::is_same< typename T::Cooked, GenericPointer >::value;
   };

   template< typename T > struct IsArithmetic
   {
      static const bool value = std::is_arithmetic< typename T::Cooked >::value;
   };

   template< typename > struct Any { static const bool value = true; };
}
