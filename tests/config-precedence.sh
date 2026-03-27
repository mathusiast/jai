#!/bin/sh

. ./common.sh

setup_test config-precedence
init_config

cat >"$CONFIG_DIR/shared.conf" <<'EOF'
setenv FROM_SHARED=shared
unsetenv KEEP_ME
EOF

cat >"$CONFIG_DIR/probe.conf" <<'EOF'
conf .defaults
conf shared.conf
mode bare
jail from-config
setenv FROM_PROBE=probe
command /usr/bin/env
EOF

capture_in_dir "$WORKDIR" run_jai_with_env KEEP_ME=keep -D probe
assert_status 0
assert_contains "$CAPTURE_STDOUT" "FROM_SHARED=shared"
assert_contains "$CAPTURE_STDOUT" "FROM_PROBE=probe"
assert_contains "$CAPTURE_STDOUT" "JAI_MODE=bare"
assert_contains "$CAPTURE_STDOUT" "JAI_JAIL=from-config"
assert_no_output_line "KEEP_ME=keep"

capture_in_dir "$WORKDIR" run_jai_with_env KEEP_ME=keep --setenv KEEP_ME --setenv CLI_OVERRIDE=cli -j cli-name -m casual probe
assert_status 0
assert_output_line "KEEP_ME=keep"
assert_output_line "CLI_OVERRIDE=cli"
assert_contains "$CAPTURE_STDOUT" "JAI_MODE=casual"
assert_contains "$CAPTURE_STDOUT" "JAI_JAIL=cli-name"

cat >"$CONFIG_DIR/expand.conf" <<'EOF'
conf .defaults
setenv EXPANDED=${SRC_VALUE}
command /usr/bin/env
EOF

capture_in_dir "$WORKDIR" run_jai_with_env SRC_VALUE=expanded -C expand
assert_status 0
assert_output_line "EXPANDED=expanded"
