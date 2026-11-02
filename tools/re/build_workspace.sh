#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."

: "${GHIDRA_INSTALL_DIR:?set GHIDRA_INSTALL_DIR, e.g. /opt/ghidra}"
: "${JAVA_HOME:?set JAVA_HOME to a JDK 21 or newer}"
AV_PYGHIDRA=${AV_PYGHIDRA:-$HOME/.local/share/uv/tools/pyghidra-mcp/bin/python}
BIN=re/bin
mkdir -p "$BIN" re/ghidra-proj

while read -r name sha256; do
    case "$name" in ''|'#'*) continue ;; esac
    base=${name%.*}
    if [ -f "$BIN/$name" ] && [ "$(sha256sum "$BIN/$name" | cut -d' ' -f1)" = "$sha256" ] \
       && [ -f "$BIN/$base.pdb" ]; then
        echo "-- $name present"
        continue
    fi
    staging=$(mktemp -d re/.fetch.XXXXXX)
    python3 -B tools/re/msfetch.py get "$name" --sha256 "$sha256" \
            --outdir "$staging" --pdbdir "$staging/pdb"
    pdb=$(find "$staging/pdb" -maxdepth 1 -iname "$base.pdb" | head -1)
    if [ -z "$pdb" ]; then
        echo "!! $name: no PDB named $base.pdb was published" >&2
        rm -rf "$staging"
        exit 1
    fi
    mv "$staging/$name" "$BIN/$name"
    mv "$pdb" "$BIN/$base.pdb"
    rm -rf "$staging"
done < tools/re/binaries.txt

mapfile -t targets < <(grep -vE '^\s*(#|$)' tools/re/binaries.txt | awk '{print "re/bin/" $1}')
"$AV_PYGHIDRA" tools/re/import.py re/ghidra-proj wddm "${targets[@]}"
