#!/bin/sh

. ./common.sh

setup_test home-cwd-guard
init_config

capture_in_dir "$REAL_HOME" run_jai /usr/bin/true
assert_status 1
assert_contains "$CAPTURE_STDERR" 'Refusing to grant your entire home directory to jailed code.'

capture grep -F 'If you are in your home directory, you can launch jai with' \
  "$ABS_TOP_SRCDIR/jai.1.md"
assert_status 0
