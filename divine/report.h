// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#include <iomanip>
#include <sstream>

#include <divine/version.h>
#include <divine/meta.h>

#ifndef DIVINE_REPORT_H
#define DIVINE_REPORT_H

namespace divine {

struct Report
{
    Meta &meta;
    sysinfo::Info sysinfo;

    int m_signal;
    bool m_finished, m_dumped;

    Report( Meta &m ) : meta( m ), m_signal( 0 ), m_finished( false ), m_dumped( false )
    {
    }

    void signal( int s )
    {
        m_finished = false;
        m_signal = s;
    }

    void finished()
    {
        m_finished = true;
    }

    template< typename Mpi >
    void mpiInfo( Mpi &mpi ) {
        meta.execution.mpi = mpi.size();
    }

    void final( std::ostream &o ) {
        if ( m_dumped )
            return;
        m_dumped = true;

        o << BuildInfo() << std::endl;
        o << sysinfo << std::endl;
        o << meta << std::endl;

        o << "Termination-Signal: " << m_signal << std::endl;
        o << "Finished: " << (m_finished ? "Yes" : "No") << std::endl;
    }
};

}

#endif
