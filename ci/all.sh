#!/usr/bin/env bash
# Full CI pipeline: install deps, build OpenVAF, build VACASK, test, bundle.
# This is the "run everything" wrapper that calls the individual scripts.
#
# Usage: ci/all.sh [--no-test] [--no-bundle] [--openvaf-dir DIR]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OPENVAF_DIR=""
NO_TEST=""
NO_BUNDLE=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-test)     NO_TEST="--no-test"; shift ;;
    --no-bundle)   NO_BUNDLE=true; shift ;;
    --openvaf-dir) OPENVAF_DIR="$2"; shift 2 ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
done

OPENVAF_ARGS=()
BUILD_ARGS=()
BUNDLE_ARGS=()

if [ -n "$OPENVAF_DIR" ]; then
  OPENVAF_ARGS+=("$OPENVAF_DIR")
  BUILD_ARGS+=(--openvaf-dir "$OPENVAF_DIR")
  BUNDLE_ARGS+=(--openvaf-dir "$OPENVAF_DIR")
fi

if [ -n "$NO_TEST" ]; then
  BUILD_ARGS+=(--no-test)
fi

"$SCRIPT_DIR/install-deps.sh"
"$SCRIPT_DIR/build-openvaf.sh" "${OPENVAF_ARGS[@]+"${OPENVAF_ARGS[@]}"}"
"$SCRIPT_DIR/build-vacask.sh" "${BUILD_ARGS[@]+"${BUILD_ARGS[@]}"}"

if [ "$NO_BUNDLE" = false ]; then
  "$SCRIPT_DIR/bundle.sh" "${BUNDLE_ARGS[@]+"${BUNDLE_ARGS[@]}"}"
fi

echo "==> All done"
