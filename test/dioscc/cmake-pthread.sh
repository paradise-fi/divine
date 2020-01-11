. lib/testcase

cat > CMakeLists.txt <<EOF
cmake_minimum_required( VERSION 3.2 )
find_package( Threads REQUIRED )
EOF

if ! cmake -DCMAKE_C_COMPILER=dioscc -DCMAKE_CXX_COMPILER=dioscc .; then
    cat CMakeFiles/CMakeOutput.log
    cat CMakeFiles/CMakeError.log
    exit 1
fi
