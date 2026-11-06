#!/usr/bin/env bash
# Make a Rust toolchain (>= 1.85) available on PATH.
#
# Source this file, do not run it: rustup installs into $HOME/.cargo and adds it
# to PATH only for the current shell, so a build script that runs in a later
# process needs this done again.
#
# Usage: . ci/install-rust.sh
set -euo pipefail

export PATH="$HOME/.cargo/bin:$PATH"

NEED_RUST=false
if ! command -v cargo &>/dev/null || ! command -v rustc &>/dev/null; then
  NEED_RUST=true
elif [ "$(rustc --version | awk '{print $2}')" \< "1.85.0" ]; then
  echo "==> Rust $(rustc --version | awk '{print $2}') is too old, upgrading..."
  NEED_RUST=true
fi

if [ "$NEED_RUST" = true ]; then
  echo "==> Installing Rust"
  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal
  # shellcheck source=/dev/null
  if [ -f "$HOME/.cargo/env" ]; then
    source "$HOME/.cargo/env"
  fi
fi
