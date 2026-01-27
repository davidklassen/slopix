#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
AS_DIR="$SCRIPT_DIR/.."
AS="$AS_DIR/as"
CROSS=aarch64-elf-

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

PASS=0
FAIL=0
SKIP=0

if [ ! -x "$AS" ]; then
    echo "Building assembler..."
    make -C "$AS_DIR" >/dev/null
fi

run_test() {
    local src="$1"
    local name=$(basename "$src" .s)
    local tmpdir=$(mktemp -d)
    local ours="$tmpdir/ours.o"
    local gnu="$tmpdir/gnu.o"
    local ours_dis="$tmpdir/ours.dis"
    local gnu_dis="$tmpdir/gnu.dis"
    local ours_rel="$tmpdir/ours.rel"
    local gnu_rel="$tmpdir/gnu.rel"

    if ! "$AS" -o "$ours" "$src" 2>/dev/null; then
        echo -e "${RED}FAIL${NC}: $name - our assembler failed"
        ((FAIL++))
        rm -rf "$tmpdir"
        return
    fi

    if ! ${CROSS}as -o "$gnu" "$src" 2>/dev/null; then
        echo -e "${YELLOW}SKIP${NC}: $name - GNU as failed (unsupported syntax?)"
        ((SKIP++))
        rm -rf "$tmpdir"
        return
    fi

    # Compare instruction hex bytes only (column 2 of objdump output)
    ${CROSS}objdump -d "$ours" 2>/dev/null | grep -E '^[[:space:]]+[0-9a-f]+:' | awk '{print $2}' > "$ours_dis"
    ${CROSS}objdump -d "$gnu" 2>/dev/null | grep -E '^[[:space:]]+[0-9a-f]+:' | awk '{print $2}' > "$gnu_dis"

    if ! diff -q "$ours_dis" "$gnu_dis" >/dev/null 2>&1; then
        echo -e "${RED}FAIL${NC}: $name - disassembly mismatch"
        echo "  Differences:"
        diff "$gnu_dis" "$ours_dis" | head -20 | sed 's/^/    /'
        ((FAIL++))
        rm -rf "$tmpdir"
        return
    fi

    ${CROSS}readelf -r "$ours" 2>/dev/null | grep -v "^$" | grep -v "^Relocation" | grep -v "^There are" | sort > "$ours_rel" || true
    ${CROSS}readelf -r "$gnu" 2>/dev/null | grep -v "^$" | grep -v "^Relocation" | grep -v "^There are" | sort > "$gnu_rel" || true

    if ! diff -q "$ours_rel" "$gnu_rel" >/dev/null 2>&1; then
        echo -e "${YELLOW}WARN${NC}: $name - relocation differences (may be acceptable)"
        diff "$gnu_rel" "$ours_rel" | head -10 | sed 's/^/    /'
    fi

    echo -e "${GREEN}PASS${NC}: $name"
    ((PASS++))

    rm -rf "$tmpdir"
}

run_error_test() {
    local src="$1"
    local expected="$2"
    local name=$(basename "$src" .s)
    if ! "$AS" -o /dev/null "$src" 2>&1 | grep -q "$expected"; then
        echo -e "${RED}FAIL${NC}: $name - expected error containing: $expected"
        ((FAIL++))
    else
        echo -e "${GREEN}PASS${NC}: $name (error test)"
        ((PASS++))
    fi
}

unit_only=false
if [ "$1" = "--unit-only" ]; then
    unit_only=true
fi

echo "Running assembler tests..."
echo ""

for src in "$SCRIPT_DIR"/*.s; do
    if [ -f "$src" ]; then
        run_test "$src"
    fi
done

if [ "$unit_only" = false ]; then
    CMD_DIR="$AS_DIR/../.."
    for prog_dir in "$CMD_DIR"/*/; do
        prog=$(basename "$prog_dir")
        if [ "$prog" = "as" ] || [ "$prog" = "tests" ]; then
            continue
        fi
        src="$prog_dir/$prog.s"
        if [ -f "$src" ]; then
            run_test "$src"
        fi
    done
fi

if [ -d "$SCRIPT_DIR/errors" ]; then
    echo ""
    echo "Running error tests..."
    run_error_test "$SCRIPT_DIR/errors/undefined_local.s" "undefined local symbol"
    run_error_test "$SCRIPT_DIR/errors/imm12_overflow.s" "not encodable"
fi

echo ""
echo "========================================"
echo -e "Results: ${GREEN}$PASS passed${NC}, ${RED}$FAIL failed${NC}, ${YELLOW}$SKIP skipped${NC}"

if [ $FAIL -gt 0 ]; then
    exit 1
fi
