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

TMP_OUT=$(mktemp)
TMP_BIN=$(mktemp -u /tmp/emojilang_test_bin_XXXXXX)
trap 'rm -f "$TMP_OUT" "$TMP_BIN"' EXIT

echo "======================================================="
echo "       Running EmojiLang Test Suite (Dual-Mode)        "
echo "======================================================="

# --- Mode 1: Interpreter (Phase 1) ---
echo ""
echo ">>> [MODE 1] Phase 1 Tree-Walk Interpreter"
echo "-------------------------------------------------------"
for test_file in tests/*.emoji; do
    [ -f "$test_file" ] || continue
    test_name=$(basename "$test_file" .emoji)
    expected_file="tests/expected/${test_name}.out"
    TOTAL=$((TOTAL + 1))

    if [ ! -f "$expected_file" ]; then
        echo "[-] FAIL: [interp] $test_name (missing expected output file: $expected_file)"
        FAILED=$((FAILED + 1))
        continue
    fi

    if "$EMOJILANG" "$test_file" > "$TMP_OUT" 2>&1; then
        if diff -u "$TMP_OUT" "$expected_file" > /dev/null; then
            echo "[+] PASS: [interp] $test_name"
            PASSED=$((PASSED + 1))
        else
            echo "[-] FAIL: [interp] $test_name (output mismatch)"
            echo "--- Diff ---"
            diff -u "$expected_file" "$TMP_OUT" || true
            echo "------------"
            FAILED=$((FAILED + 1))
        fi
    else
        echo "[-] FAIL: [interp] $test_name (execution error)"
        cat "$TMP_OUT"
        FAILED=$((FAILED + 1))
    fi
done

# --- Mode 2: AOT Native Compilation (Phase 2) ---
echo ""
echo ">>> [MODE 2] Phase 2 AOT Compilation (Native Executable)"
echo "-------------------------------------------------------"
for test_file in tests/*.emoji; do
    [ -f "$test_file" ] || continue
    test_name=$(basename "$test_file" .emoji)
    expected_file="tests/expected/${test_name}.out"
    TOTAL=$((TOTAL + 1))

    # Verify LLVM IR validity via llvm-as first
    if ! "$EMOJILANG" --ir "$test_file" 2>/dev/null | llvm-as -o /dev/null 2>/dev/null; then
        echo "[-] FAIL: [aot] $test_name (invalid LLVM IR rejected by llvm-as)"
        FAILED=$((FAILED + 1))
        continue
    fi

    rm -f "$TMP_BIN"
    if "$EMOJILANG" --aot "$test_file" -o "$TMP_BIN" >/dev/null 2>&1; then
        if "$TMP_BIN" > "$TMP_OUT" 2>&1; then
            if diff -u "$TMP_OUT" "$expected_file" > /dev/null; then
                echo "[+] PASS: [aot]    $test_name"
                PASSED=$((PASSED + 1))
            else
                echo "[-] FAIL: [aot]    $test_name (output mismatch)"
                echo "--- Diff ---"
                diff -u "$expected_file" "$TMP_OUT" || true
                echo "------------"
                FAILED=$((FAILED + 1))
            fi
        else
            echo "[-] FAIL: [aot]    $test_name (runtime error)"
            cat "$TMP_OUT"
            FAILED=$((FAILED + 1))
        fi
    else
        echo "[-] FAIL: [aot]    $test_name (AOT compilation/linking error)"
        FAILED=$((FAILED + 1))
    fi
done

echo ""
echo "======================================================="
echo "Results: $PASSED / $TOTAL passed"
if [ "$FAILED" -eq 0 ]; then
    echo "All tests passed successfully in BOTH modes!"
    exit 0
else
    echo "$FAILED test(s) failed."
    exit 1
fi
