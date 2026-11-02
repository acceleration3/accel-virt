#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION=v1.9.2607
ARCHIVE=dxc_2026_07_29.zip
ARCHIVE_SHA256=a1dfb116ba3eeae6a1582291b53a8e7bf65ad760676bd3194685c8f7367cd241
FILES=("bin/x64/dxcompiler.dll 9a5100511e127c6a2fc78edf984f95074a76d35b90c90c4d342430a5ae160e9b"
       "bin/x64/dxil.dll feb57253eff0a622561e29b44cedbe86b89fc9a5bc8dc00fa2f98fafd712c2d8"
       "LICENSE-MS.txt 734f72f239fe7b07b4c7203f294c1a7ce27095687278bab7e56d630d7c672963")

TARGET=third_party/dxc
CACHE=third_party/.cache
if [ -f "$TARGET/VERSION" ] && [ "$(cat "$TARGET/VERSION")" = "$VERSION" ]; then
    echo "dxc: $TARGET already at $VERSION"
    exit 0
fi

mkdir -p "$CACHE"
file="$CACHE/$ARCHIVE"
if [ ! -f "$file" ]; then
    echo "dxc: downloading $VERSION" >&2
    curl -sSfL -o "$file.part" \
        "https://github.com/microsoft/DirectXShaderCompiler/releases/download/$VERSION/$ARCHIVE"
    mv "$file.part" "$file"
fi
got=$(sha256sum "$file" | cut -d' ' -f1)
if [ "$got" != "$ARCHIVE_SHA256" ]; then
    rm -f "$file"
    echo "dxc: $ARCHIVE sha256 $got, want $ARCHIVE_SHA256" >&2
    exit 1
fi

staging=$(mktemp -d third_party/.dxc.XXXXXX)
trap 'rm -rf "$staging"' EXIT
unzip -q "$file" -d "$staging/archive" 2>/dev/null || [ -d "$staging/archive/bin" ]
mkdir -p "$staging/dxc"
for entry in "${FILES[@]}"; do
    read -r inside want <<<"$entry"
    got=$(sha256sum "$staging/archive/$inside" | cut -d' ' -f1)
    if [ "$got" != "$want" ]; then
        echo "dxc: $inside sha256 $got, want $want" >&2
        exit 1
    fi
    cp "$staging/archive/$inside" "$staging/dxc/"
done
printf '%s\n' "$VERSION" > "$staging/dxc/VERSION"
rm -rf "$TARGET"
mv "$staging/dxc" "$TARGET"
echo "dxc: $TARGET ($(du -sh "$TARGET" | cut -f1))"
