#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Locate emojilang executable
if [ -x "./build/emojilang" ]; then
    EMOJILANG="./build/emojilang"
elif [ -x "./emojilang" ]; then
    EMOJILANG="./emojilang"
else
    echo "Error: emojilang executable not found. Please build first using 'cmake --build build'." >&2
    exit 1
fi

TOTAL=0
PASSED=0
FAILED=0

echo "========================================="
echo "       Running EmojiLang Test Suite      "
echo "========================================="

TMP_OUT=$(mktemp)
trap 'rm -f "$TMP_OUT"' EXIT

for test_file in tests/*.emoji; do
    [ -f "$test_file" ] || continue
    test_name=$(basename "$test_file" .emoji)
    expected_file="tests/expected/${test_name}.out"
    TOTAL=$((TOTAL + 1))

    if [ ! -f "$expected_file" ]; then
        echo "[-] FAIL: $test_name (missing expected output file: $expected_file)"
        FAILED=$((FAILED + 1))
        continue
    fi

    if "$EMOJILANG" "$test_file" > "$TMP_OUT" 2>&1; then
        if diff -u "$TMP_OUT" "$expected_file" > /dev/null; then
            echo "[+] PASS: $test_name"
            PASSED=$((PASSED + 1))
        else
            echo "[-] FAIL: $test_name (output mismatch)"
            echo "--- Diff ---"
            diff -u "$expected_file" "$TMP_OUT" || true
            echo "------------"
            FAILED=$((FAILED + 1))
        fi
    else
        echo "[-] FAIL: $test_name (execution error)"
        cat "$TMP_OUT"
        FAILED=$((FAILED + 1))
    fi
done

echo "========================================="
echo "Results: $PASSED / $TOTAL passed"
if [ "$FAILED" -eq 0 ]; then
    echo "All tests passed successfully!"
    exit 0
else
    echo "$FAILED test(s) failed."
    exit 1
fi
