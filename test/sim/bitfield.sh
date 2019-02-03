# TAGS:
. lib/testcase

cat > test.c <<EOF
struct BF
{
    int a:3;
    int b:7;
    int c:30;
    char d:1;
};

int main() {
    struct BF x = { .a = 2, .b = -5, .c = 8, .d = 1 };
    return 0;
}
EOF

sim test.c <<EOF
> start
> step --over
> source
> step
> show .x
+ a:
+ value:.*[i16 2 d]
+ b:
+ value:.*[i16 123 d]
+ c:
+ value:.*[i32 8 d]
+ d:
+ value:.*[i8 1 d]
EOF
