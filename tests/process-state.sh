#!/bin/sh

. ./common.sh

setup_test process-state
require_built_helper "$JAI_TEST_PROBE"
init_config

capture_in_dir "$WORKDIR" run_jai_no_tty "$JAI_TEST_PROBE" probe
assert_status 0
assert_output_line "pid=2"
assert_output_line "ppid=1"
assert_output_line "tty=no"
assert_output_line "fg_pgrp=-1"
assert_output_line "tmp_same_inode=yes"
assert_output_line "proc_pids=1,2"
