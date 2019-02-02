# TAGS: min
. lib/testcase

cat > file.cpp <<EOF
#include <atomic>

int main()
{
    std::atomic< int > x{ 42 };
}
EOF

sim file.cpp <<EOF
+ ^# executing __boot at
> start
> step --count 2 --quiet
+ ^# executing main at
> show .x
+ ^attributes
+ type:\s*_Atomic int
+ value:\s*[i32 42 d]
EOF
