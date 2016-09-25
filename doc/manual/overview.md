Introduction
============

The DIVINE project aims to develop a general-purpose, fast, reliable and
easy-to-use model checker. The roots of the project go back to a
special-purpose, explicit-state, asynchronous system model checking tool for
LTL properties. However, rigorous development processes are in a steady
decline, being displaced by more agile, flexible and dynamic methods. In the
agile world, there is little place for large-scale, long-term planning and
pondering on "paper only" designs, which would favour the use of a traditional
model checker.

The current version of DIVINE strives to keep up with this dynamic world,
bringing "heavy-duty" model checking technology much closer to daily
programming routine. Our major goal is to express model checking problems in a
language which every developer is fluent with: the programming language of
their own project. Even if you don't apply model checking to your resulting
program directly, writing throwaway models makes much more sense in a language
you understand well and use daily.

Current versions of DIVINE provide out-of-the box support for the C (C99) and
C++ (C++14) programming languages, including their respective standard
libraries. Addtional libraries may be rebuilt for use with DIVINE by the user.
