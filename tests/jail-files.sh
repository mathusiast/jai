#!/bin/sh

. ./common.sh

setup_test jail-files
init_config
ensure_untrusted_user

cat >"$CONFIG_DIR/jail-include.conf" <<'EOF'
setenv FROM_JAIL_INCLUDE=include
EOF

cat >"$CONFIG_DIR/probe.conf" <<'EOF'
conf .defaults
mode strict
jail from-conf
command /usr/bin/env
EOF

cat >"$CONFIG_DIR/from-conf.jail" <<'EOF'
conf jail-include.conf
mode bare
setenv FROM_DOTJAIL=yes
EOF

capture_in_dir "$WORKDIR" run_jai -D -C probe
assert_status 0
assert_output_line "FROM_DOTJAIL=yes"
assert_output_line "FROM_JAIL_INCLUDE=include"
assert_contains "$CAPTURE_STDOUT" "JAI_MODE=bare"
assert_contains "$CAPTURE_STDOUT" "JAI_JAIL=from-conf"

capture_in_dir "$WORKDIR" run_jai -C probe -m strict
assert_status 0
assert_contains "$CAPTURE_STDOUT" "JAI_MODE=strict"
assert_contains "$CAPTURE_STDOUT" "JAI_JAIL=from-conf"

capture_in_dir "$WORKDIR" run_jai --jail created /usr/bin/env
assert_status 0
assert_contains "$CAPTURE_STDOUT" "JAI_MODE=strict"
assert_contains "$CAPTURE_STDOUT" "JAI_JAIL=created"
assert_path_exists "$CONFIG_DIR/created.jail"
grep -Fx 'mode strict' "$CONFIG_DIR/created.jail" >/dev/null ||
  fail "created.jail should record strict mode by default"

cat >"$CONFIG_DIR/bad.conf" <<'EOF'
conf .defaults
jail bad
command /usr/bin/true
EOF

cat >"$CONFIG_DIR/bad.jail" <<'EOF'
jail nope
EOF

capture_in_dir "$WORKDIR" run_jai -C bad
assert_status 1
assert_contains "$CAPTURE_STDERR" "cannot set name from a .jail file or include"
