. lib/testcase

cat > CMakeLists.txt <<EOF
cmake_minimum_required( VERSION 3.2 )
find_package( Threads REQUIRED )
EOF

cmake -DCMAKE_C_COMPILER=divcc -DCMAKE_CXX_COMPILER=divcc
