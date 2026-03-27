#!/bin/sh

. ./common.sh

setup_test xfail-config-dir-hidden

CONFIG_DIR=$REAL_HOME/jai-test-config-hidden-$$
register_cleanup_path "$CONFIG_DIR"
mkdir -p "$CONFIG_DIR"

init_config
ensure_untrusted_user

capture_in_dir "$REAL_HOME" run_jai -m strict -D /usr/bin/true
if [ "$CAPTURE_STATUS" -ne 0 ]; then
  printf '%s\n' 'FAIL: strict mode should not fail just because JAI_CONFIG_DIR is under $HOME and already hidden' >&2
  printf '%s\n' "$CAPTURE_STDERR" >&2
  exit 1
fi
