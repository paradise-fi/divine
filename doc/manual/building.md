Installation
============

This section is only relevant when you are installing from source.  We will
assume that you have obtained a source tarball from <http://divine.fi.muni.cz>,
e.g. `divine-@version@.tar.gz`.

DIVINE can be built on Linux and possibly on other POSIX-compatible systems
including macOS (not tested). There is currently no support for DIVINE on
Windows. If you do not want to build DIVINE from sources, you can download a
virtual machine image with pre-built DIVINE.

Prerequisites
-------------

If you use recent Ubuntu, Fedora or Arch Linux (or possibly another distribution
which uses `apt-get`, `yum` or `pacman` as a package manager) the easiest way to
get dependencies of DIVINE is to run `make prerequisites` in the directory with
the sources (you will need to have `make` installed):

    $ tar xvzf divine-@version@.tar.gz
    $ cd divine-@version@
    $ make prerequisites

Otherwise, to build DIVINE, you will need the following:

* A POSIX-compatible operating system,
* `make` (tested with BSD and GNU),
* GNU C++ (4.9 or newer) or clang (3.2 or newer),
* CMake \[[www.cmake.org](http://www.cmake.org)\] 3.2 or newer,
* `libedit` \[[thrysoee.dk/editline](http://thrysoee.dk/editline/)\],
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

    $ tar xvzf divine-@version@.tar.gz
    $ cd divine-@version@

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

This will install DIVINE and its version of LLVM into `/opt/divine/`.

You can also run the test-suite if you like:

    $ make check

