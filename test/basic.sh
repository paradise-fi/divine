not () { "$@" && exit 1 || return 0; }

divine-mc --version
not divine-mc foobar
