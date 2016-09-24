Installation
============

This section is only relevant when you are installing from source.  We will
assume that you have obtained a source tarball from http://divine.fi.muni.cz,
eg. `divine-4.0.tar.gz`.

Prerequisites
-------------

To build DIVINE from source, you will need the following:

* A POSIX-compatible operating system,
* GNU C++ (4.9 or newer) or clang (3.2 or newer),
* CMake \[[www.cmake.org](http://www.cmake.org)\] 3.2 or newer,
* about 12GB of disk space and 4GB of RAM (18GB for both release and debug
  builds),

Additionally, DIVINE can make use of the following optional components:

* ninja build system \[[ninja-build.org](https://ninja-build.org)\] for faster
  builds,
* pandoc \[[pandoc.org](http://pandoc.org)\] for formatting the manual (HTML
  and PDF with pdflatex).

Building & Installing
---------------------

First, unzip the distribution tarball and enter the newly created directory

    $ tar xvzf divine-4.0.tar.gz
    $ cd divine-4.0

The build is driven by a Makefile, and should be fully automatic. You only need
to run:

    $ make

This will first build a C++14 toolchain and a runtime required to build DIVINE
itself, then proceed to compile DIVINE. After a while, you should obtain the
main DIVINE binary. You can check that this is indeed the case by running:

    $ ./_build.release/tools/divine help

You can now run DIVINE from its build directory, or you can optionally install
it by issuing

    $ make install

You can also run the test-suite if you like:

    $ make check

