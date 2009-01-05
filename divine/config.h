// -*- C++ -*-
#include <wibble/sys/thread.h>
#include <wibble/test.h> // for assert
#include <iostream>
#include <fstream>

#ifndef DIVINE_CONFIG_H
#define DIVINE_CONFIG_H

// configuration stuff, may need refactoring later
struct Config {
    int m_workers;
    int m_storageInitial;
    int m_storageFactor;
    int m_handoff;
    bool m_verbose, m_ce, m_report;

    std::string m_algorithm, m_generator, m_order, m_partitioning, m_storage;

    void setPartitioning( std::string p ) { m_partitioning = p; }
    void setAlgorithm( std::string p ) { m_algorithm = p; }
    void setGenerator( std::string p ) { m_generator = p; }
    void setOrder( std::string p ) { m_order = p; }
    void setStorage( std::string p ) { m_storage = p; }

    std::string m_input;

    void setWorkers( int t ) { m_workers = t; }
    int workers() { return m_workers; }

    void setStorageInitial( int i ) { assert( i > 0 ); m_storageInitial = i; }
    void setStorageFactor( int f ) { assert( f > 1 ); m_storageFactor = f; }
    void setInput( std::string in ) { m_input = in; }

    int storageInitial() { return m_storageInitial; }
    int storageFactor() { return m_storageFactor; }
    std::string input() { return m_input; }

    int handoff() {
        return m_handoff;
    }

    void setHandoff( int h ) {
        m_handoff = h;
    }

    void setVerbose( bool v ) { m_verbose = v; }
    void setReport( bool v ) { m_report = v; }
    bool verbose() { return m_verbose; }
    bool report() { return m_report; }

    bool generateCounterexample() { return m_ce; }

    std::ostream &dump( std::ostream &o ) {
        o << "Workers: " << workers() << std::endl;
        o << "Initial-Storage-Size: " << storageInitial() << std::endl;
        o << "Storage-Growth-Factor: " << storageFactor() << std::endl;
        o << "Handoff-Threshold: " << handoff() << std::endl;
        o << "Input-File: " << input() << std::endl;
        o << "Trail-File: " << m_trailFile << std::endl;
        o << "Algorithm: " << m_algorithm << std::endl;
        o << "Order: " << m_order << std::endl;
        o << "Partitioning: " << m_partitioning << std::endl;
        o << "Generator: " << m_generator << std::endl;
        o << "Storage: " << m_storage << std::endl;
        return o;
    }

    std::string m_trailFile;
    std::ostream *m_trailStream;

    std::ostream &trailStream() {
        if ( !m_trailStream ) {
            m_trailStream = new std::ofstream( m_trailFile.c_str() );
        }
        return *m_trailStream;
    }

    void setTrailFile( std::string f ) {
        if ( f == "-" )
            m_trailStream = &std::cout;
        m_trailFile = f;
    }

    void setGenerateCounterexample( bool e ) {
        m_ce = e;
    }

    Config() : m_workers( 2 ),
               m_storageInitial( 4097 ), m_storageFactor( 2 ), m_handoff( 50 ),
               m_verbose( false ), m_ce( true ), m_trailStream( 0 )
    {}
};

#endif
