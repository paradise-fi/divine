// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>
#include <wibble/sys/thread.h>
#include <wibble/test.h> // for assert
#include <iostream>
#include <fstream>
#include <divine/datastruct.h>

#ifndef DIVINE_CONFIG_H
#define DIVINE_CONFIG_H

// configuration stuff, may need refactoring later
struct Config {
    int m_workers;
    int m_initialTable;
    bool m_verbose, m_ce, m_report;

    std::string m_algorithm, m_generator;

    void setAlgorithm( std::string p ) { m_algorithm = p; }
    void setGenerator( std::string p ) { m_generator = p; }

    std::string m_input;

    void setWorkers( int t ) { m_workers = t; }
    int workers() { return m_workers; }

    int initialTableSize() { return m_initialTable; }
    void setInitialTableSize( int it ) { m_initialTable = it; }

    void setInput( std::string in ) { m_input = in; }

    std::string input() { return m_input; }

    void setVerbose( bool v ) { m_verbose = v; }
    void setReport( bool v ) { m_report = v; }
    bool verbose() { return m_verbose; }
    bool report() { return m_report; }

    bool generateCounterexample() { return m_ce; }

    std::ostream &dump( std::ostream &o ) {
        o << "Algorithm: " << m_algorithm << std::endl;
        o << "Generator: " << m_generator << std::endl;
        o << "Workers: " << workers() << std::endl;
        o << "Input-File: " << input() << std::endl;
        o << "Trail-File: " << m_trailFile << std::endl;
        return o;
    }

    std::string m_trailFile, m_ceFile;
    std::ostream *m_trailStream, *m_ceStream;

    std::ostream &ceStream() {
        if ( !m_ceStream ) {
            if ( m_ceFile.empty() )
                m_ceStream = new std::stringstream();
            else
                m_ceStream = new std::ofstream( m_ceFile.c_str() );
        }
        return *m_ceStream;
    }

    std::ostream &trailStream() {
        if ( !m_trailStream ) {
            if ( m_trailFile.empty() )
                m_trailStream = new std::stringstream();
            else
                m_trailStream = new std::ofstream( m_trailFile.c_str() );
        }
        return *m_trailStream;
    }

    void setTrailFile( std::string f ) {
        if ( f == "-" )
            m_trailStream = &std::cerr;
        m_trailFile = f;
    }

    void setCeFile( std::string f ) {
        if ( f == "-" )
            m_ceStream = &std::cerr;
        m_ceFile = f;
    }

    void setGenerateCounterexample( bool e ) {
        m_ce = e;
    }

    Config() : m_workers( 2 ), m_initialTable( 4096 ),
               m_verbose( false ), m_ce( true ),
               m_trailStream( 0 ), m_ceStream( 0 )
    {}

    ~Config() {
        if ( ( m_trailStream != &std::cerr ) && ( m_trailStream != &std::cout ) )
            divine::safe_delete( m_trailStream );
        if ( ( m_ceStream != &std::cerr ) && ( m_ceStream != &std::cout ) )
            divine::safe_delete( m_ceStream );
    }
};

#endif
