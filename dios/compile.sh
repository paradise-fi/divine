#!/bin/sh

compile() {
    clang -c -D__divine__ -D_POSIX_C_SOURCE=2008098L -D_LITTLE_ENDIAN=1234 -D_BYTE_ORDER=1234 \
          -emit-llvm -Qunused-arguments -nobuiltininc -nostdlibinc -nostdinc -Xclang \
          -nostdsysteminc -nostdinc++ \
          -I../libm -I../pdclib -I../libcxxabi/include -I../libcxx/std -I.. \
          -g -D__unix -U__APPLE__ -U_WIN32 "$@"
}

set -e
rm -rf obj.{libcxxabi,libcxx,pdclib,divine}
mkdir obj.{libcxxabi,libcxx,pdclib,divine}

(cd obj.divine && compile -std=c++11 ../divine/*.cpp ../filesystem/*.cpp)
(cd obj.pdclib && compile ../pdclib/*/*.c ../pdclib/functions/*/*.c ../pdclib/opt/*/*.c)
(cd obj.libcxxabi && compile -std=c++11 ../libcxxabi/src/*.cpp)
(cd obj.libcxx && compile -std=c++11 ../libcxx/src/*.cpp)
