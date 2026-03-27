#!/bin/sh

. ./common.sh

setup_test teardown
init_config

HOST_HOME_FILE=$REAL_HOME/jai-teardown-home-$$
UPPER_FILE=$CONFIG_DIR/default.changes/$(basename "$HOST_HOME_FILE")

register_cleanup_path "$HOST_HOME_FILE"

printf 'host' >"$HOST_HOME_FILE"

capture_in_dir "$WORKDIR" run_jai /bin/sh -c '
  printf overlay > "$1"
  printf keep > /tmp/keep
' sh "$HOST_HOME_FILE"
assert_status 0

assert_mount_exists "/run/jai/$REAL_USER/default.home"
assert_root_path_exists "/run/jai/$REAL_USER/tmp/default/keep"
assert_path_exists "$UPPER_FILE"

capture run_jai -u
assert_status 0
assert_no_mount "/run/jai/$REAL_USER/default.home"
assert_root_path_missing "/run/jai/$REAL_USER/default.home"
assert_root_path_missing "/run/jai/$REAL_USER/tmp/default"
assert_path_exists "$UPPER_FILE"
