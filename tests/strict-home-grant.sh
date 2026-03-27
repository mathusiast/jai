#!/bin/sh

. ./common.sh

setup_test strict-home-grant
init_config
ensure_untrusted_user

READ_FILE=$REAL_HOME/jai-strict-home-read-$$
WRITE_FILE=$REAL_HOME/jai-strict-home-write-$$

register_cleanup_path "$READ_FILE"
register_cleanup_path "$WRITE_FILE"

printf 'visible-from-home' >"$READ_FILE"
rm -f "$WRITE_FILE"

capture_in_dir "$REAL_HOME" run_jai -m strict -D /bin/sh -c \
  '[ -e "$1" ] && printf visible || printf hidden' sh "$READ_FILE"
assert_status 0
assert_eq "$CAPTURE_STDOUT" "hidden"

capture_in_dir "$REAL_HOME" run_jai -m strict -D -d "$REAL_HOME" /bin/sh -c '
  pwd
  cat "$1"
  printf rewritten > "$2"
' sh "$READ_FILE" "$WRITE_FILE"
assert_status 0
assert_output_line "$REAL_HOME"
assert_output_line "visible-from-home"
assert_file_equals "$WRITE_FILE" "rewritten"
