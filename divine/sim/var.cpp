// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-

/*
 * (c) 2016-2017 Petr Ročkai <code@fixp.eu>
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

#include <divine/sim/cli.hpp>

namespace divine::sim
{

DN CLI::get( std::string n, bool silent, std::unique_ptr< DN > start, bool comp )
{
    if ( !start && n[0] != '$' && n[0] != '#' ) /* normalize */
    {
        if ( n[0] == '.' || n[0] == ':' )
            n = "$_" + n;
        else
            n = "$_:" + n;
    }

    auto split = std::min( n.find( '.' ), n.find( ':' ) );
    std::string head( n, 0, split );
    std::string tail( n, split < n.size() ? split + 1 : head.size(), std::string::npos );

    if ( !start )
    {
        auto i = _dbg.find( head );
        if ( i == _dbg.end() && silent )
            return nullDN();
        if ( i == _dbg.end() )
            throw brick::except::Error( "variable " + head + " is not defined" );

        auto dn = i->second;
        switch ( head[0] )
        {
            case '$': dn.relocate( _ctx.snapshot() ); break;
            case '#': break;
            default: UNREACHABLE( "impossible case" );
        }
        if ( split >= n.size() )
            return dn;
        else
            return get( tail, silent, std::make_unique< DN >( dn ), n[split] == '.' );
    }

    std::unique_ptr< DN > dn_next;

    auto lookup = [&]( auto key, auto rel )
                        {
                            if ( head == key )
                                dn_next = std::make_unique< DN >( rel );
                        };

    if ( comp )
        start->components( lookup );
    else
        start->related( lookup );

    if ( silent && !dn_next )
        return nullDN();
    if ( !dn_next )
        throw brick::except::Error( "lookup failed at " + head );

    if ( split >= n.size() )
        return *dn_next;
    else
        return get( tail, silent, std::move( dn_next ), n[split] == '.' );
}

void CLI::update()
{
    set( "$top", frameDN() );

    auto globals = _ctx.get( _VM_CR_Globals ).pointer;
    if ( !get( "$globals", true ).valid() || get( "$globals" ).address() != globals )
    {
        DN gdn( _ctx, _ctx.snapshot() );
        gdn.address( dbg::DNKind::Globals, globals );
        set( "$globals", gdn );
    }

    auto state = _ctx.get( _VM_CR_State ).pointer;
    if ( !get( "$state", true ).valid() || get( "$state" ).address() != state )
    {
        DN sdn( _ctx, _ctx.snapshot() );
        sdn.address( dbg::DNKind::Object, state );
        sdn.type( _ctx._state_type );
        sdn.di_type( _ctx._state_di_type );
        set( "$state", sdn );
    }

    if ( get( "$_" ).kind() == dbg::DNKind::Frame )
        set( "$frame", "$_" );
    else
        set( "$data", "$_" );
    if ( !get( "$frame", true ).valid() )
        set( "$frame", "$top" );
}

}
