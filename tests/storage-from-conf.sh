#!/bin/sh

. ./common.sh

setup_test xfail-storage-from-conf
init_config

STORAGE=$TEST_ROOT/storage
mkdir -p "$STORAGE"

cat >"$CONFIG_DIR/probe.conf" <<EOF
conf .defaults
storage $STORAGE
jail storage-from-conf
command /usr/bin/env
EOF

capture_in_dir "$WORKDIR" run_jai -C probe
if [ "$CAPTURE_STATUS" -ne 0 ]; then
  printf '%s\n' 'FAIL: storage set in a .conf file should relocate .jail files and sandbox state' >&2
  printf '%s\n' "$CAPTURE_STDERR" >&2
  exit 1
fi

assert_output_line "JAI_JAIL=storage-from-conf"
assert_path_exists "$STORAGE/storage-from-conf.jail"
assert_path_missing "$CONFIG_DIR/storage-from-conf.jail"
