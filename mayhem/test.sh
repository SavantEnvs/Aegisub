#!/usr/bin/env bash
#
# mayhem/test.sh — run upstream's libaegisub gtest suite (tests/tests/*.cpp, built by build.sh
# with normal flags as mayhem/out/gtest-main) and report CTRF counts from its XML report.
# The suite asserts parsed values (vfr, keyframes, ASS time/colour/uuencode, JSON, options,
# dialogue lexer, audio providers, ...), so a neutered binary fails it.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

RUNNER="$SRC/mayhem/out/gtest-main"
if [ ! -x "$RUNNER" ]; then
  echo "test.sh: $RUNNER missing — run mayhem/build.sh first" >&2
  emit_ctrf gtest 0 1 0
  exit 1
fi

# tests/setup.sh creates the fixture tree (data/...) in the cwd; use a scratch dir.
WORK="$(mktemp -d "${TMPDIR:-/tmp}/aegisub-test.XXXXXX")"
trap 'chmod -R u+rwX "$WORK" 2>/dev/null; rm -rf "$WORK"' EXIT
cd "$WORK"
bash "$SRC/tests/setup.sh"
XML="$WORK/gtest.xml"
AEGISUB_TEST_DATA_DIR="$SRC/tests" "$RUNNER" --gtest_output="xml:$XML"
rc=$?

if [ ! -s "$XML" ]; then
  echo "test.sh: gtest produced no report (exit $rc)" >&2
  emit_ctrf gtest 0 1 0
  exit 1
fi
head_line="$(grep -m1 '<testsuites ' "$XML")"
attr() { echo "$head_line" | sed -n "s/.* $1=\"\([0-9]*\)\".*/\1/p"; }
tests="$(attr tests)"; failures="$(attr failures)"; errors="$(attr errors)"; disabled="$(attr disabled)"
skipped="$(grep -c '<skipped' "$XML" || true)"
: "${tests:=0}" "${failures:=0}" "${errors:=0}" "${disabled:=0}"
failed=$(( failures + errors ))
[ "$rc" -ne 0 ] && [ "$failed" -eq 0 ] && failed=1
passed=$(( tests - failures - errors - skipped ))
[ "$passed" -lt 0 ] && passed=0
[ "$tests" -gt 0 ] || failed=$(( failed + 1 ))
emit_ctrf gtest "$passed" "$failed" "$(( skipped + disabled ))"
