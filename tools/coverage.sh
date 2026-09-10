#!/usr/bin/env bash
# Branch coverage gate. Constitution III.1: 100% of branches in src/core,
# src/device and src/proto, or an AV_UNTESTABLE mark with a justification.
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD=${BUILD:-build}
MIN=${AV_COVERAGE_MIN:-100}

cmake -S . -B "$BUILD" -G Ninja -DCMAKE_C_COMPILER=clang -DAV_COVERAGE=ON >/dev/null
cmake --build "$BUILD" >/dev/null
rm -rf "$BUILD/coverage"; mkdir -p "$BUILD/coverage"
ctest --test-dir "$BUILD" --output-on-failure >/dev/null

mapfile -t BINS < <(find "$BUILD" -maxdepth 1 -type f -name 'test_*' -executable)
llvm-profdata merge -sparse "$BUILD"/coverage/*.profraw -o "$BUILD/coverage/all.profdata"

# llvm-cov takes the first binary positionally; the rest via -object. Any
# remaining positionals are source filters.
MAIN="${BINS[0]}"
OBJ=()
for b in "${BINS[@]:1}"; do OBJ+=(-object "$b"); done

# Only scan source dirs that exist and hold code; llvm-cov errors on an empty
# path and set -e would then hide the real report.
SRCS=()
for d in src/core src/device src/proto; do
  while IFS= read -r f; do SRCS+=("$f"); done < <(find "$d" -name "*.c" 2>/dev/null)
done

llvm-cov report "$MAIN" "${OBJ[@]}" -instr-profile="$BUILD/coverage/all.profdata" \
    -show-branch-summary "${SRCS[@]}" | tee "$BUILD/coverage/report.txt"

llvm-cov show "$MAIN" "${OBJ[@]}" -instr-profile="$BUILD/coverage/all.profdata" \
    -show-branches=percent -format=html -output-dir="$BUILD/coverage/html" \
    "${SRCS[@]}" >/dev/null 2>&1 || true

pct=$(llvm-cov export "$MAIN" "${OBJ[@]}" -instr-profile="$BUILD/coverage/all.profdata" \
        "${SRCS[@]}" |
      python3 -c 'import json,sys
d = json.load(sys.stdin)["data"][0]["totals"]["branches"]
print("%.2f" % d["percent"])')

untestable=$(grep -ho 'AV_UNTESTABLE(' "${SRCS[@]}" 2>/dev/null | wc -l)

echo
echo "branch coverage: ${pct}%   (gate: ${MIN}%)"
echo "AV_UNTESTABLE marks: ${untestable}"
awk -v p="$pct" -v m="$MIN" 'BEGIN { exit !(p+0 >= m+0) }' || {
    echo "FAIL: branch coverage below gate" >&2; exit 1; }
echo "PASS"
