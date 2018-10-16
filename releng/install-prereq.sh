#!/bin/sh

# Try to install build prerequisites of DIVINE based on some guesswork aboout
# the distro we are running on.

# (c) 2016 Vladimír Štill <xstill@fi.muni.cz>
# (c) 2017 Petr Ročkai <code@fixp.eu>

# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.

# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

if test -f /etc/os-release; then
    # Ubuntu, Debian & similar have ID or ID_LIKE set to 'debian'
    if grep -q debian /etc/os-release; then
        pmgr=apt-get
    # Fedora, CentOS and hopefully RHEL have ID or ID_LIKE containing 'fedora'
    elif grep -q fedora /etc/os-release; then
        pmgr=yum
    elif grep -q alpine /etc/os-release; then
        pmgr=apk
    elif grep -q arch /etc/os-release; then
        pmgr=pacman
    fi
fi

ninja=ninja

if test $pmgr = "yum"; then
    # ninja is missing in CentOS/RHEL
    if yum search ninja-build; then
        ninja=ninja-build
    else
        ninja=""
    fi
fi

if [ -z $ninja ]; then
    testninja=true
else
    testninja=$ninja
fi

if [ -z $CXX ]; then
    CXX=g++
fi

if [ $pmgr = "apt-get" ]; then
    pkgs="perl make cmake ninja-build python2.7 g++ libedit-dev libncurses5-dev zlib1g-dev"
elif [ $pmgr = "yum" ]; then
    pkgs="perl make cmake $ninja python2 gcc-c++ libedit-devel ncurses-devel zlib-devel"
elif [ $pmgr = "pacman" ]; then
    pkgs="perl make cmake ninja python2 gcc libedit ncurses zlib"
elif [ $pmgr = apk ]; then
    pkgs="perl make cmake ninja python2 g++ libedit-dev ncurses-dev ncurses-static zlib"
fi

if [ -n $1 ] && [ "$1" = "get" ]; then
    echo $pkgs
    exit 0
fi

# check if everything is installed
if make --version 2>&1 > /dev/null \
    && cmake --version 2>&1 > /dev/null \
    && $testninja --version 2>&1 > /dev/null \
    && printf '#include <zlib.h>\nint main() { }\n' | $CXX -x c - -o /dev/null -lcurses -ledit -lz 2>&1 > /dev/null
then
    echo "everything seems to be present and working"
    exit 0
fi

uid=`id -u`

if test -z "$pmgr"; then
    echo "could not detect package manager, please install dependencies manually" 2>&1
    exit 1
fi

if test $pmgr = "apt-get"; then
    cmd="apt-get install -y $pkgs"
elif test $pmgr = "yum"; then
    cmd="yum install $pkgs"
elif test $pmgr = "pacman"; then
    # --needed disables reinstallation of already installed packages
    cmd="pacman -S --needed $pkgs"
elif test $pmgr = apk; then
    cmd="apk add $pkgs"
fi

echo + $cmd

if test $uid -eq 0; then
    exec $cmd
else
    echo "$0 not running as root, will try to use sudo or su" 1>&2
    echo "trying sudo, please enter your password if prompted" 1>&2
    sudo $cmd && exit 0
    echo "sudo failed, trying su, please enter root password when asked" 1>&2
    su -c $cmd && exit 0
    echo
    echo "could not gain root access, plase run $0 or the following command as root manually:" 1>&2
    echo "$cmd" 1>&2
    exit 1
fi

