set -e
rm -rf _test
mkdir _test
(cd _test && cmake .. -DHAVE_TUT=ON || exit 1)
(cd _test && make || exit 1)
(cd _test && make check || exit 1)
rm -rf _test
