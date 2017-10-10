. lib/testcase

divine verify -std=c++11 $TESTS/demo/1.thread.cpp | tee report.txt
cat > expected <<EOF
+   \[0\] starting thread
+   \[1\] thread started
+   \[0\] incrementing
EOF

ordgrep expected < report.txt
