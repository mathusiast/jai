#!/bin/sh

. ./common.sh

setup_test modes
init_config
ensure_untrusted_user

HOST_HOME_FILE=$REAL_HOME/jai-strict-hidden-$$
STRICT_GRANTED=$WORKDIR/strict-granted.txt

register_cleanup_path "$HOST_HOME_FILE"

printf 'secret' >"$HOST_HOME_FILE"

capture_in_dir "$WORKDIR" run_jai -j named /usr/bin/env
assert_status 0
assert_contains "$CAPTURE_STDOUT" "JAI_MODE=strict"
assert_contains "$CAPTURE_STDOUT" "JAI_JAIL=named"

capture_in_dir "$WORKDIR" run_jai -m strict /bin/sh -c 'id -u'
assert_status 0
assert_eq "$CAPTURE_STDOUT" "$UNTRUSTED_UID"

capture run_jai -D -m bare -j barecase /bin/sh -c 'id -u'
assert_status 0
assert_eq "$CAPTURE_STDOUT" "$REAL_UID"

capture_in_dir "$WORKDIR" run_jai -m strict /bin/sh -c '[ -e "$1" ] && printf visible || printf hidden' sh "$HOST_HOME_FILE"
assert_status 0
assert_eq "$CAPTURE_STDOUT" "hidden"

capture_in_dir "$WORKDIR" run_jai -m strict /bin/sh -c 'printf strict > "$1"' sh "$STRICT_GRANTED"
assert_status 0
assert_file_equals "$STRICT_GRANTED" "strict"
