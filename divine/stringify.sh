#!/bin/sh
# TODO double quotes...
set -e
sed -e '1i \
namespace divine { const char *'"${1}"'_str = "\\' \
    -e 's,\\,\\\\,g' \
    -e 's,$,\\n\\,' \
    -e 's,",\\",g' \
    -e 's,wibble/test.h,cassert,' \
    -e '$a \
\"\; }' \
    < "$2" > "${1}_str.cpp"
