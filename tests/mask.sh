#!/bin/sh

. ./common.sh

setup_test mask
init_config

TARGET_NAME=jai-mask-target-$$
TARGET_PATH=$REAL_HOME/$TARGET_NAME

register_cleanup_path "$TARGET_PATH"

printf 'mask-me' >"$TARGET_PATH"

cat >"$CONFIG_DIR/mask-on.conf" <<EOF
conf .defaults
mode casual
jail masked
mask $TARGET_NAME
EOF

cleanup_jai
capture_in_dir "$WORKDIR" run_jai -C mask-on /bin/sh -c '[ -e "$1" ] && printf visible || printf hidden' sh "$TARGET_PATH"
assert_status 0
assert_eq "$CAPTURE_STDOUT" "hidden"

cat >"$CONFIG_DIR/mask-off.conf" <<EOF
conf .defaults
mode casual
jail unmasked
mask $TARGET_NAME
unmask $TARGET_NAME
EOF

cleanup_jai
capture_in_dir "$WORKDIR" run_jai -C mask-off /bin/sh -c '[ -e "$1" ] && printf visible || printf hidden' sh "$TARGET_PATH"
assert_status 0
assert_eq "$CAPTURE_STDOUT" "visible"
assert_file_equals "$TARGET_PATH" "mask-me"
