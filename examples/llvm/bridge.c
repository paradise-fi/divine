/*
 * This program is more or less just a demonstration of one possible usage of
 * __divine_choice builtin.
 * You can use this builtin for writing non-deterministic algorithms and then
 * let DiVinE to do a complete search in order to find out if there is any
 * computational path leading to a correct solution.
 *
 * Task: Solve puzzle of soldiers going over bridge.
 *   Input: N soldiers and their individual times + overall time limit.
 *   Output: "Yes" if it is possible for soldiers to get to the other
 *           side within the time limit, "No" otherwise.
 *
 * Rules: Four men (soldiers) have to cross a bridge at night. The bridge is old
 * and dilapidated and can hold at most two people at a time. There are no railings,
 * and the men have only one flashlight. Any party who crosses, either
 * one or two men, must carry the flashlight with them. The flashlight must be
 * walked back and forth; it cannot be thrown, etc. Each man walks at a
 * different speed. One takes 5 minute to cross, another 10 minutes, another 20,
 * and the last 25 minutes. If two men cross together, they must walk at the
 * slower man's pace. Can they get to the other side in 60 minutes?
 * (The program is generalized to larger number of men.)
 *
 * Solve with:
 *  $ divine compile --llvm [--cflags=" < flags > "] bridge.c
 *  $ divine verify -p assert bridge.bc [-d]
 *
 * Output is rather unintuitive. We actually verify if it holds that
 * there is no solution:
 *    Property HOLDS        = there is no solution
 *    Property DOESN'T HOLD = there is a solution
 *    Counterexample        = solution
 */

// Number of soldiers.
#define N  4

// Overall time limit.
#define TIME_LIMIT  60

// Individual times. First four values are classical, others are rather
// arbitrary, feel free to change or add new ones and adjust macro N accordingly.
int times[] = {5, 10, 20, 25, 30, 30, 40, 45};

#include <pthread.h>

int total_time = 0; // Elapsed time.

// Soldiers go from left to right.
int right = 0;  // How many of them is on the right side.

// Where are/is soldiers/torch? left = 0, right = 1
int where[N];
int where_is_torch = 0;

int main() {
    int i,j;
    for ( i = 0; i < N; i++ ) {
        where[i] = 0;
    }

    for (;;) {
        int max_time = 0;

        // First non-deterministic choice: Should we send one or two soldiers
        // to go to the other side of the bridge?
        int count = __divine_choice( 2 ) + 1;
        for ( j = 0; j < count; j++ ) {

            // Second non-deterministic choice: Who should go to the other side?
            int who = __divine_choice( where_is_torch ? right : N - right ) + 1;

            // Find index of selected soldier.
            i = -1;
            while ( who ) {
                ++i;
                if ( where[i] == where_is_torch )
                    --who;
            }

            // Soldier is going to the other side.
            where[i] = 1 - where[i];
            if ( where_is_torch == 1 )
                --right;
            else
                ++right;

            if ( max_time < times[i] )
                max_time = times[i];
        }

        where_is_torch = 1 - where_is_torch;
        total_time += max_time;
        if ( total_time > TIME_LIMIT )
            return 0; // Time limit was reached.

        assert( right != N ); // When this fails, we have a solution.
    }

    return 0;
}
