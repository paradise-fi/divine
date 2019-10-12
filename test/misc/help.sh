. lib/testcase
divine help 2>&1 | grep check
divine help check 2>&1 | grep -- --symbolic
not divine help goo 2>&1 | grep -i "unknown command"
divine help version
