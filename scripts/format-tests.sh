#!/bin/sh
set -e

tmpfile=$(mktemp)
trap 'rm -f "$tmpfile"' EXIT

tee "$tmpfile"

kernel_passed=0
kernel_failed=0
user_passed=0
user_failed=0
report=0

while IFS= read -r line; do
    line=$(printf '%s' "$line" | tr -d '\r')
    case "$line" in
        *"=== Test Report ==="*)
            report=$((report + 1))
            ;;
        *passed)
            n=$(printf '%s' "$line" | awk '{print $1}')
            if [ "$report" -eq 1 ]; then
                kernel_passed=$n
            elif [ "$report" -eq 2 ]; then
                user_passed=$n
            fi
            ;;
        *failed)
            n=$(printf '%s' "$line" | awk '{print $1}')
            if [ "$report" -eq 1 ]; then
                kernel_failed=$n
            elif [ "$report" -eq 2 ]; then
                user_failed=$n
            fi
            ;;
    esac
done < "$tmpfile"

total_passed=$((kernel_passed + user_passed))
total_failed=$((kernel_failed + user_failed))

printf "\n=== Summary ===\n"
printf "kernel:    %d passed, %d failed\n" "$kernel_passed" "$kernel_failed"
printf "userspace: %d passed, %d failed\n" "$user_passed" "$user_failed"
printf "total:     %d passed, %d failed\n" "$total_passed" "$total_failed"

if [ "$total_failed" -gt 0 ]; then
    exit 1
fi
