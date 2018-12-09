# TAGS: min
. lib/testcase

check_help()
{
    divine exec -o help -o config:$1 test.c > trace.out
    if ! grep "config: run" trace.out; then
        echo "# unexpected help output for configuration $1:"
        cat trace.out
        result=1
    fi
}

cat > test.c <<EOF
int main() {}
EOF

result=0
check_help "default"
#check_help "passthrough"
check_help "synchronous"
#check_help "replay"

exit $result
