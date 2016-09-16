/*
 * Simple IO example
 * =================
 *
 *  Small program which loads two strings from configuration
 *  file and select a configuration base on data on standard
 *  input. This example demonstrates hot to use snaphots in
 *  the virtual file system.
 *
 * Verification
 * ------------
 *
 *      $ divine compile --llvm --snapshot=conf --stdin=simple_io_stdin.txt simple_io.cpp
 *      $ divine verify -p assert simple_io.bc
 *
 */
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cassert>

std::vector< std::string > loadConfig() {
    const int possibleConfigurations = 2;

    std::vector< std::string > cfg;
    std::ifstream config( "config.txt" );

    for ( int i = 0; i < possibleConfigurations; ++i ) {
        cfg.push_back( std::string() );
        config >> cfg.back();
    }
    return cfg;
}

int main() {
    try {
        int selectedConfig;
        std::vector< std::string > cfg = loadConfig();

        std::cin >> selectedConfig;
        assert( selectedConfig > 0 && selectedConfig <= cfg.size() );

        std::cout << cfg[ selectedConfig - 1 ] << std::endl;

    } catch ( std::bad_alloc & ) {
    }
    return 0;
}
