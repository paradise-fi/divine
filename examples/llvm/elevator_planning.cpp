/*
 * This program is more or less just a demonstration of one possible usage of
 * __divine_choice builtin.
 * You can use this builtin for writing non-deterministic algorithms and then
 * let DiVinE to do a complete search in order to find out if there is any
 * computational path leading to a correct solution.
 *
 * Task: Planning of elevator strategy under several constraints.
 *       (example from AIPS 2000 contest: http://www.cs.toronto.edu/aips2000/)
 *   Input: a number of floors, passangers and their initial locations;
 *          several restricting constraints (capacity of the elevator, conflicts
            between pasangers, etc.)
 *   Output: "Yes" if it is possible to transport all passangers from their initial
 *           locations to the zeroth floor, "No" otherwise.
 *
 *
 * Solve with:
 *  $ divine compile --llvm --cflags="-std=c++11 "[" < flags > "] elevator_planning.cpp
 *  $ divine verify -p assert elevator_planning.bc [-d]
 *
 * Output is rather unintuitive. We actually verify if it holds that
 * there is no solution:
 *    Property HOLDS        = there is no solution
 *    Property DOESN'T HOLD = there is a solution
 *    Counterexample        = solution
 *
 * Run and watch some computational path with:
 *  $ clang++ -std=c++11 [ < flags > ] -lpthread -lstdc++ -o elevator_planning.exe elevator_planning.cpp
 *  $ ./elevator_planning.exe
 */

// Which version to consider. There are four versions available: 1, 2, 3, 4.
// Feel free to add some new ones.
// Meaning of each parameter and constraint is explained in the definition block
// of the first version.
#define VERSION  1

#if ( VERSION == 1 )

// A number of floors.
#define FLOORS    4

// Capacity of the elevator.
#define CAPACITY  2

// A number of persons.
#define N         3

// Initial position (floor number) of every person.
#define INIT      { 0, 1, 2 }

// Conflicts between passangers.
// Expressed by means of two disjunctive subsets of persons: A, B.
// If one person is in A and the other in B, then these two persons
// are in mutual exclusion in terms of elevator transportation.
// They mustn't occur in the elevator at the same time.
// If the i-th person is in the set A, then the i-th digit of an array
// CONFLICT_A has value of 1, otherwise 0. Similarly for the set B.
#define CONFLICT_A  { 1, 0, 0 }
#define CONFLICT_B  { 0, 1, 0 }

// Persons who cannot travel alone (marked by value of 1).
#define NOT_ALONE   { 1, 0, 0 }

#elif ( VERSION == 2 )

#define FLOORS      5
#define CAPACITY    3
#define N           5
#define INIT        { 0, 1, 2, 3, 4 }
#define CONFLICT_A  { 1, 0, 0, 1, 1 }
#define CONFLICT_B  { 0, 1, 1, 0, 0 }
#define NOT_ALONE   { 1, 1, 0, 0, 0 }

#elif ( VERSION == 3 )

#define FLOORS      7
#define CAPACITY    3
#define N           7
#define INIT        { 0, 1, 2, 3, 4, 0, 3 }
#define CONFLICT_A  { 1, 0, 0, 1, 1, 0, 0 }
#define CONFLICT_B  { 0, 1, 1, 0, 0, 1, 0 }
#define NOT_ALONE   { 1, 1, 0, 0, 0, 0, 0 }

#elif ( VERSION == 4 )

#define FLOORS      8
#define CAPACITY    4
#define N           4
#define INIT        { 7, 2, 6, 5 }
#define CONFLICT_A  { 0, 0, 0, 0 }
#define CONFLICT_B  { 0, 0, 0, 0 }
#define NOT_ALONE   { 0, 0, 0, 0 }

#endif

#include <pthread.h>
#include <cstdlib>

// For native execution.
#ifndef DIVINE
#include <iostream>
#include <unistd.h>
#define __divine_choice( x ) ( rand() % ( x ) )

template <typename T>
void _info(const T& value) {
    std::cout << value << std::endl;
}

template <typename U, typename... T>
void _info(const U& head, const T&... tail) {
    std::cout << head;
    _info( tail... );
}
#endif

template <typename... T>
void info( const T&... args) {
#ifndef DIVINE
    _info( args... );
    usleep( 500000 );
#endif
}

// Program constant, do not change.
#define IN  -1

const int init[] = INIT;
const bool conflictA[] = CONFLICT_A;
const bool conflictB[] = CONFLICT_B;
const bool not_alone[] = NOT_ALONE;

struct Elevator {
    int persons[N];
    int current, at_zero;
    int in, inA, inB;
    int alone;

    int _can_get_in( int person ) {
        if ( persons[person] == current  &&  in < CAPACITY  &&
             ( conflictA[person] == 0 || inB == 0 )  &&
             ( conflictB[person] == 0 || inA == 0 )  &&
             ( !not_alone[person] || in ) )
            return 1;
        return 0;
    }

    int _can_get_out( int person ) {
        if ( persons[person] == IN  &&
             ( in > 2 || ( alone - not_alone[person] == 0 ) ) )
            return 1;
        return 0;
    }

    void start() {
        int i, action, floor, passanger;
        int can_in, can_out;

        for (;;) {
#ifdef DIVINE
            assert( at_zero != N ); // When this fails, we have a solution.
#else
            if ( at_zero == N ) {
                info( "Successfully transported all persons to the zeroth floor." );
                return;
            }
#endif
            can_in = can_out = 0;
            for ( i = 0; i < N; i++ ) {
                if ( _can_get_in(i) )
                    ++can_in;
                if ( _can_get_out(i) )
                    ++can_out;
            }

            // Non-deterministically select next action.
            action = __divine_choice( 1 + ( can_in ? 1 : 0 ) + ( can_out ? 1 : 0 ) );

            if ( action == 0 ) {
                // moving up/down

                // Non-deterministically select a floor to go to.
                floor = __divine_choice( FLOORS - 1 );
                if ( floor >= current )
                    ++floor;
                current = floor;
                info( "The elevator moved to the ", floor, ". floor." );
            } else if ( action == 1 && can_in ) {
                // getting in

                // Non-deterministically select a new passanger.
                passanger = __divine_choice( can_in ) + 1;
                i = -1;
                while( passanger ) {
                    i++;
                    if ( _can_get_in( i ) )
                        --passanger;
                }
                persons[i] = IN;
                ++in;
                inA += conflictA[i];
                inB += conflictB[i];
                alone += not_alone[i];
                if ( current == 0 )
                    --at_zero;
                info( "Person ", i, " entered the elevator." );
            } else {
                // getting out

                // Non-deterministically select a person that will leave the elevator.
                passanger = __divine_choice( can_out ) + 1;
                i = -1;
                while( passanger ) {
                    i++;
                    if ( _can_get_out( i ) )
                        --passanger;
                }
                persons[i] = current;
                --in;
                inA -= conflictA[i];
                inB -= conflictB[i];
                alone -= not_alone[i];
                if ( current == 0 )
                    ++at_zero;
                info( "Person ", i, " leaved the elevator" );
            }
        }
    }

    Elevator() : current( 0 ), at_zero( 0 ), in( 0 ), inA( 0 ), inB( 0 ), alone( 0 ) {
        for ( int i = 0; i < N; i++ ) {
            persons[i] = init[i];
            info( "Person ",i," is initialy at the ", init[i], ". floor." );
            if ( persons[i] == 0 )
                ++at_zero;
        }
    }

};

int main() {
    // Create and start the elevator.
    Elevator elevator;
    elevator.start();
    return 0;
}
