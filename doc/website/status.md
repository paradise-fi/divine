Once a day, we take patches from our bleeding-edge source repository -- `next`,
run our testsuite and if everything works out, we make those patches available
in our `current` repository (see also [download] [1]). In other words, the
`current` repository is expected to be in a useable state at all times. The
code in `current` is then subject to additional testing and if the results are
satisfactory, a release tarball is made.

[1]: download.html

@report@
