#! /bin/bash

DIR=`dirname "$0"`

$DIR"/benchmark.sh" eratosthenes MAX 4 30
$DIR"/benchmark.sh" leader N 3 6
$DIR"/benchmark.sh" leader2 N 3 4
$DIR"/benchmark.sh" loops N 3 10
$DIR"/benchmark.sh" peterson_N N 2 3
$DIR"/benchmark.sh" pftp
$DIR"/benchmark.sh" snoopy
$DIR"/benchmark.sh" sort N 5 6
