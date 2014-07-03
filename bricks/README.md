Bricks is a collection of useful C++ code, organised in standalone header
files. Many of the bricks require C++11, although some will work on legacy
C++98 compilers. The idea is that you can take a bunch of bricks you need and
copy them into your project. Of course, you can check "upstream" for updates
and pull them into your copies at your leisure. I have tried to minimise
dependencies, but some bricks inevitably use other bricks (all relevant
#includes are near the top of each file).

Most bricks come with a small demo, always at the bottom of the file enclosed
in an #ifdef DEMO section. You can build and run those demos by running

    $ g++ -DBRICK_DEMO -x c++ -I. brick-something.h

(add -std=c++11 and other flags as needed).
