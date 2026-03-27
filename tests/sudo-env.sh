#!/bin/sh

. ./common.sh

setup_test sudo-env
init_config

capture_in_dir "$WORKDIR" run_jai /usr/bin/env
assert_status 0
assert_output_line "HOME=$REAL_HOME"
assert_output_line "USER=$REAL_USER"
assert_output_line "LOGNAME=$REAL_USER"
assert_no_output_line "HOME=/root"
assert_no_output_line "MAIL=/var/mail/root"
