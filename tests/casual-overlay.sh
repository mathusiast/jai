#!/bin/sh

. ./common.sh

setup_test casual-overlay
init_config

HOST_HOME_FILE=$REAL_HOME/jai-test-home-$$
CWD_FILE=$WORKDIR/granted.txt
UPPER_FILE=$CONFIG_DIR/default.changes/$(basename "$HOST_HOME_FILE")

register_cleanup_path "$HOST_HOME_FILE"

printf 'host-home' >"$HOST_HOME_FILE"
printf 'host-cwd' >"$CWD_FILE"

capture_in_dir "$WORKDIR" run_jai /bin/sh -c '
  printf "%s\n%s\n" "$JAI_MODE" "$JAI_JAIL"
  printf overlay > "$1"
  printf granted > "$2"
  printf private > /tmp/private-file
  cat /var/tmp/private-file
' sh "$HOST_HOME_FILE" "$CWD_FILE"

assert_status 0
assert_contains "$CAPTURE_STDOUT" "casual"
assert_contains "$CAPTURE_STDOUT" "default"
assert_contains "$CAPTURE_STDOUT" "private"
assert_file_equals "$HOST_HOME_FILE" "host-home"
assert_file_equals "$CWD_FILE" "granted"
assert_path_exists "$UPPER_FILE"
assert_file_equals "$UPPER_FILE" "overlay"

mount_line=$(get_mount_line "/run/jai/$REAL_USER/default.home")
assert_contains "$mount_line" "upperdir=$CONFIG_DIR/default.changes"
assert_root_file_equals "/run/jai/$REAL_USER/tmp/default/private-file" "private"

capture_in_dir "$WORKDIR" run_jai /bin/sh -c 'printf denied >/etc/jai-test-denied'
assert_status 1
assert_contains "$CAPTURE_STDERR" "Read-only file system"
