set -e
rm -rf _test
mkdir _test
cd _test
cmake .. -DHAVE_TUT=ON
make
make check
cd ..
rm -rf _test
