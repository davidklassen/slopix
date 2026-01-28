#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$SCRIPT_DIR/../../.."
LD="$SCRIPT_DIR/../ld"
AS="$ROOT_DIR/tools/as/as"
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
    make -C "$ROOT_DIR/tools/as" >/dev/null
fi

if [ ! -x "$LD" ]; then
    echo "Building linker..."
    make -C "$SCRIPT_DIR/.." >/dev/null
fi

test_sections() {
    local src="$1"
    local name=$(basename "$src" .s)
    local tmpdir=$(mktemp -d)
    local obj="$tmpdir/test.o"

    if ! "$AS" -o "$obj" "$src" 2>/dev/null; then
        echo -e "${RED}FAIL${NC}: $name sections - assembler failed"
        ((FAIL++))
        rm -rf "$tmpdir"
        return
    fi

    local our_count=$("$LD" --dump-sections "$obj" 2>/dev/null | tail -n +2 | wc -l | tr -d ' ')
    local gnu_count=$(${CROSS}readelf -S "$obj" 2>/dev/null | grep -E '^\s*\[\s*[0-9]' | wc -l | tr -d ' ')

    if [ "$our_count" != "$gnu_count" ]; then
        echo -e "${RED}FAIL${NC}: $name sections - count mismatch (ours: $our_count, gnu: $gnu_count)"
        ((FAIL++))
        rm -rf "$tmpdir"
        return
    fi

    echo -e "${GREEN}PASS${NC}: $name sections ($our_count sections)"
    ((PASS++))
    rm -rf "$tmpdir"
}

test_symbols() {
    local src="$1"
    local name=$(basename "$src" .s)
    local tmpdir=$(mktemp -d)
    local obj="$tmpdir/test.o"

    if ! "$AS" -o "$obj" "$src" 2>/dev/null; then
        echo -e "${RED}FAIL${NC}: $name symbols - assembler failed"
        ((FAIL++))
        rm -rf "$tmpdir"
        return
    fi

    local our_count=$("$LD" --dump-symbols "$obj" 2>/dev/null | tail -n +2 | wc -l | tr -d ' ')
    local gnu_count=$(${CROSS}readelf -s "$obj" 2>/dev/null | grep '^\s*[0-9]' | wc -l | tr -d ' ')

    if [ "$our_count" != "$gnu_count" ]; then
        echo -e "${RED}FAIL${NC}: $name symbols - count mismatch (ours: $our_count, gnu: $gnu_count)"
        ((FAIL++))
        rm -rf "$tmpdir"
        return
    fi

    echo -e "${GREEN}PASS${NC}: $name symbols ($our_count symbols)"
    ((PASS++))
    rm -rf "$tmpdir"
}

test_error() {
    local testname="$1"
    local input="$2"
    local expected="$3"
    local tmpdir=$(mktemp -d)

    echo "$input" > "$tmpdir/test.txt"

    if "$LD" --dump-sections "$tmpdir/test.txt" 2>&1 | grep -q "$expected"; then
        echo -e "${GREEN}PASS${NC}: $testname (error test)"
        ((PASS++))
    else
        echo -e "${RED}FAIL${NC}: $testname - expected error containing: $expected"
        ((FAIL++))
    fi

    rm -rf "$tmpdir"
}

test_wrong_arch() {
    local testname="wrong_arch"
    local tmpdir=$(mktemp -d)
    local obj="$tmpdir/test.o"

    # Create a minimal x86-64 ELF relocatable object
    printf '\x7fELF\x02\x01\x01\x00' > "$obj"  # ELF magic, 64-bit, little-endian
    printf '\x00\x00\x00\x00\x00\x00\x00\x00' >> "$obj"  # padding
    printf '\x01\x00' >> "$obj"  # e_type = ET_REL
    printf '\x3e\x00' >> "$obj"  # e_machine = EM_X86_64 (0x3e)
    printf '\x01\x00\x00\x00' >> "$obj"  # e_version
    printf '\x00\x00\x00\x00\x00\x00\x00\x00' >> "$obj"  # e_entry
    printf '\x00\x00\x00\x00\x00\x00\x00\x00' >> "$obj"  # e_phoff
    printf '\x40\x00\x00\x00\x00\x00\x00\x00' >> "$obj"  # e_shoff = 64
    printf '\x00\x00\x00\x00' >> "$obj"  # e_flags
    printf '\x40\x00' >> "$obj"  # e_ehsize = 64
    printf '\x00\x00' >> "$obj"  # e_phentsize
    printf '\x00\x00' >> "$obj"  # e_phnum
    printf '\x40\x00' >> "$obj"  # e_shentsize = 64
    printf '\x00\x00' >> "$obj"  # e_shnum = 0
    printf '\x00\x00' >> "$obj"  # e_shstrndx = 0

    if "$LD" --dump-sections "$obj" 2>&1 | grep -q "not an AArch64"; then
        echo -e "${GREEN}PASS${NC}: $testname (error test)"
        ((PASS++))
    else
        echo -e "${RED}FAIL${NC}: $testname - expected error about wrong architecture"
        ((FAIL++))
    fi

    rm -rf "$tmpdir"
}

echo "Running linker tests..."
echo ""

echo "Section parsing tests:"
for src in "$SCRIPT_DIR"/*.s; do
    if [ -f "$src" ]; then
        test_sections "$src"
    fi
done

echo ""
echo "Symbol parsing tests:"
for src in "$SCRIPT_DIR"/*.s; do
    if [ -f "$src" ]; then
        test_symbols "$src"
    fi
done

echo ""
echo "Error handling tests:"
test_not_elf() {
    local testname="not_elf"
    local tmpdir=$(mktemp -d)
    local obj="$tmpdir/test.o"

    # Create a file large enough for ELF header but with wrong magic
    dd if=/dev/zero of="$obj" bs=64 count=1 2>/dev/null

    if "$LD" --dump-sections "$obj" 2>&1 | grep -q "not an ELF"; then
        echo -e "${GREEN}PASS${NC}: $testname (error test)"
        ((PASS++))
    else
        echo -e "${RED}FAIL${NC}: $testname - expected error containing: not an ELF"
        ((FAIL++))
    fi

    rm -rf "$tmpdir"
}

test_not_elf
test_wrong_arch

echo ""
echo "========================================"
echo -e "Results: ${GREEN}$PASS passed${NC}, ${RED}$FAIL failed${NC}, ${YELLOW}$SKIP skipped${NC}"

if [ $FAIL -gt 0 ]; then
    exit 1
fi
