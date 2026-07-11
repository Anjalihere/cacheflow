#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="$repo_root/build"

if [[ -f "$build_dir/CMakeCache.txt" ]] && ! grep -Fq "CMAKE_HOME_DIRECTORY:INTERNAL=$repo_root" "$build_dir/CMakeCache.txt"; then
	rm -rf "$build_dir"
fi

cmake -S "$repo_root" -B "$build_dir" -DBUILD_TESTING=ON

cmake --build "$build_dir"

echo "== Benchmark =="
"$build_dir/cacheflow_bench" --orders=5000 --warmup=500

echo
echo "== CLI Demo =="
printf 'add SELL 100 5\nadd BUY 105 5\nbook\ntrades\ncancel 1\nquit\n' | "$build_dir/cacheflow_cli"