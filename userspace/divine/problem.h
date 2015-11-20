#ifndef DIVINE_PROBLEM_H
#define DIVINE_PROBLEM_H
enum Problem {
    #define PROBLEM(x) x,
    #include <divine/problem.def>
    #undef PROBLEM
};
#endif
