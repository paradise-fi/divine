. lib/testcase

check_help() {
    cat > test.c <<EOF
int main() {}
EOF

    divine run -ohelp -oconfiguration:$1 test.c &> run.out
    cat run.out | grep 'I:' | cut -c 4- > trace.out
    if ! perl -e 'local $/; my $stdin = <>; exit 0 if $stdin =~ /help:\n\s*supported commands:/; exit 1' trace.out; then
        echo "# Unexpected help output for configuration $1:"
        cat trace.out
        result=1
    fi
}

result=0
check_help "standard"

exit $result