# TAGS: min
. lib/testcase

divine verify -std=c++11 $TESTS/demo/thread.cpp | tee report.txt
cat > expected <<EOF
+   \[0\] starting thread
+   \[0\] incrementing
+   \[1\] thread done
EOF

ordgrep expected < report.txt
