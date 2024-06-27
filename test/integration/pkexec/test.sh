#!/bin/bash

set -eux
set -o pipefail

EXPECT_SCRIPTS="$PWD/expect"
TEST_USER="polkit-testuser"
TEST_USER_PASSWORD="hello-world-$SRANDOM" # notsecret
TMP_DIR="$(mktemp -d)"

at_exit() {
    set +e

    : "Cleanup"
    userdel -rf "$TEST_USER"
    systemctl restart polkit
    rm -rf "$TMP_DIR"
    rm -f /etc/polkit-1/rules.d/00-test.rules
}

trap at_exit EXIT

: "Setup"
useradd "$TEST_USER"
echo "$TEST_USER_PASSWORD" | passwd --stdin "$TEST_USER"
# We need the expect scripts to be somewhere accessible to the $TEST_USER
cp -r "$EXPECT_SCRIPTS"/* "$TMP_DIR/"
chown -R "$TEST_USER" "$TMP_DIR"
# Temporarily allow $TEST_USER to gain root privileges
cat >/etc/polkit-1/rules.d/00-test.rules <<EOF
polkit.addAdminRule(function(action, subject) {
    return ["unix-user:$TEST_USER"];
});
EOF

# This a really ugly hack for a particularly annoying expect's behavior - it closes
# stdout when it gets EOF from stdin, which happens very quickly in CIs where stdin
# is usually closed. The behavior in such case can be reproduced with:
#
#   $ true | setsid --wait ./test.sh | cat
#
# To get around this the easiest way possible (at least known to me ATTOW) let's
# redirect /dev/zero to stdin, which keeps it open and makes expect (and spawned processes)
# behave as expected.
exec 0</dev/zero

: "Basic auth"
sudo -u "$TEST_USER" expect "$TMP_DIR/basic-auth.exp" "$TEST_USER_PASSWORD" bash -xec 'echo "I am now UID $UID"' | tee "$TMP_DIR/basic.log"
grep "I am now UID 0" "$TMP_DIR/basic.log"

: "Environment variables"
FOO=bar \
LANG=C \
LD_PRELOAD="/tmp/$SRANDOM" \
PATH="$PATH:/tmp/nope" \
TERM=linux \
USER=foo \
    sudo -u "$TEST_USER" expect "$TMP_DIR/basic-auth.exp" "$TEST_USER_PASSWORD" env | tee "$TMP_DIR/environment.log"
sed -i 's/\r$//g' "$TMP_DIR/environment.log"
# pkexec preserves a very limited subset of env variables (see environment_variables_to_save
# in pkexec's main())
#
# Variables set by pkexec
grep -E "^HOME=/root$" "$TMP_DIR/environment.log"
grep -E "^LOGNAME=root$" "$TMP_DIR/environment.log"
grep -E "^PKEXEC_UID=$(id -u "$TEST_USER")$" "$TMP_DIR/environment.log"
grep -E "^USER=root$" "$TMP_DIR/environment.log"
# pkexec resets $PATH to a predefined safe list
(! grep -E "^PATH=.*nope.*$" "$TMP_DIR/environment.log")
# Inherited variables
grep -E "^LANG=C$" "$TMP_DIR/environment.log"
grep -E "^TERM=linux$" "$TMP_DIR/environment.log"
# Ignored variables
(! grep -E "^FOO=" "$TMP_DIR/environment.log")
(! grep -E "^LD_PRELOAD=" "$TMP_DIR/environment.log")

: "Don't die with SIGTRAP on EOF in password prompt"
# See https://github.com/polkit-org/polkit/pull/431
sudo -u "$TEST_USER" expect "$TMP_DIR/SIGTRAP-on-EOF.exp"
