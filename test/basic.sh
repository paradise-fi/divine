set -vex
not () { "$@" && exit 1 || return 0; }

divine --version
divine --version | grep '2\.[0-9]'
not divine foobar

which divine
