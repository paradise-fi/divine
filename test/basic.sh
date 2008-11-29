set -vex
not () { "$@" && exit 1 || return 0; }

divine-mc --version
divine-mc --version | grep 2.0
not divine-mc foobar

which divine-mc
