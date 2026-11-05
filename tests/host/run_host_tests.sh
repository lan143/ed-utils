#!/bin/sh
set -eu

cd "$(dirname "$0")/../.."

c++ -std=c++11 -Wall -Wextra -pedantic -o /tmp/log_format_test \
    tests/host/test_log_format.cpp src/log/log_format.cpp

/tmp/log_format_test
