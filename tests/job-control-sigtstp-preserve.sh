#!/bin/sh

. ./common.sh

setup_test job-control-sigtstp-preserve
require_built_helper "$JAI_TEST_PROBE"
require_built_helper "$JAI_TEST_PTY_DRIVER"
init_config

capture run_jai_launcher "$JAI_TEST_PTY_DRIVER" \
  --jai-bin "$JAI_BIN" \
  --helper-bin "$JAI_TEST_PROBE" \
  --config-dir "$CONFIG_DIR" \
  --workdir "$WORKDIR" \
  --user "$REAL_USER" \
  --shell-parent \
  --new-pgrp \
  --foreground \
  --signal=tstp
if [ "$CAPTURE_STATUS" -ne 0 ]; then
  printf '%s\n' "$CAPTURE_STDERR" >&2
  fail 'shell job-control topology should preserve SIGTSTP exactly'
fi
