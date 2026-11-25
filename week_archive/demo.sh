#!/usr/bin/env bash

set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cd "$script_dir"

pages=4
frames=2
reference_string='R0,R1,W1,R0,R2,W2,R0,R3,W2'
output_dir=$(mktemp -d)
pager_pid=''

cleanup() {
    if [[ -n "$pager_pid" ]] && kill -0 "$pager_pid" 2>/dev/null; then
        kill "$pager_pid" 2>/dev/null || true
        wait "$pager_pid" 2>/dev/null || true
    fi
    rm -rf "$output_dir"
}
trap cleanup EXIT INT TERM

printf 'Compiling pager.c and mmu.c...\n'
gcc -std=c11 -Wall -Wextra -Wpedantic pager.c -o pager
gcc -std=c11 -Wall -Wextra -Wpedantic mmu.c -o mmu

printf 'Running the demonstration with %d pages and %d frames.\n' "$pages" "$frames"
printf 'Reference string: %s\n\n' "$reference_string"

./pager "$pages" "$frames" >"$output_dir/pager.log" &
pager_pid=$!

for ((attempt = 0; attempt < 50; ++attempt)); do
    if [[ -e /tmp/ex2/pagetable ]]; then
        break
    fi
    if ! kill -0 "$pager_pid" 2>/dev/null; then
        printf 'The pager terminated before creating the page table.\n' >&2
        cat "$output_dir/pager.log" >&2
        exit 1
    fi
    sleep 0.1
done

if [[ ! -e /tmp/ex2/pagetable ]]; then
    printf 'Timed out while waiting for the pager to initialize.\n' >&2
    exit 1
fi

./mmu "$pages" "$reference_string" "$pager_pid" >"$output_dir/mmu.log"
wait "$pager_pid"
pager_pid=''

printf '%s\n' '===== PAGER OUTPUT ====='
cat "$output_dir/pager.log"
printf '\n%s\n' '===== MMU OUTPUT ====='
cat "$output_dir/mmu.log"
