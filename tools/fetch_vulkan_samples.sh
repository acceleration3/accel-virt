#!/usr/bin/env bash
# Fetch the Vulkan samples the report runs (tools/vulkan_report.py), pinned by
# hash: Sascha Willems' examples as forked at Voultapher/Vulkan-Samples - the
# set samples/d3d11 was ported from - and the assimp they load models through.
# The samples land in third_party/vulkan-samples/Vulkan-Samples with the
# patches in tools/vulkan-samples-patches applied (a headless swap chain and
# the hooks samples/vulkan's runner drives); assimp lands unmodified beside it.
set -euo pipefail
cd "$(dirname "$0")/.."

SAMPLES_COMMIT=360ce4281e35bb6277f704ba0846b396fb741dbb
SAMPLES_SHA256=77d976f733d74cbe93ef85d196a74df20ec0471bf4dc11cfcaa3f0eb7ca7c062
SAMPLES_URL="https://github.com/Voultapher/Vulkan-Samples/archive/$SAMPLES_COMMIT.tar.gz"
ASSIMP_VERSION=5.4.3
ASSIMP_SHA256=66dfbaee288f2bc43172440a55d0235dfc7bf885dda6435c038e8000e79582cb
ASSIMP_URL="https://github.com/assimp/assimp/archive/refs/tags/v$ASSIMP_VERSION.tar.gz"

CACHE=third_party/.cache
TREE=third_party/vulkan-samples
PATCHES=tools/vulkan-samples-patches
PINNED="$SAMPLES_COMMIT assimp-$ASSIMP_VERSION $(cat "$PATCHES"/*.patch | sha256sum | cut -d' ' -f1)"

if [ -f "$TREE/VERSION" ] && [ "$(cat "$TREE/VERSION")" = "$PINNED" ]; then
    echo "vulkan samples: $TREE already at $SAMPLES_COMMIT, assimp $ASSIMP_VERSION"
    exit 0
fi

# Prints the cached archive's path, downloading it if absent; a hash mismatch
# deletes it, so a truncated download never survives to the next run.
archive() {
    local file=$CACHE/$1 url=$2 want=$3
    mkdir -p "$CACHE"
    if [ ! -f "$file" ]; then
        echo "vulkan samples: downloading $url" >&2
        curl -sSfL -o "$file.part" "$url"
        mv "$file.part" "$file"
    fi
    local got
    got=$(sha256sum "$file" | cut -d' ' -f1)
    if [ "$got" != "$want" ]; then
        rm -f "$file"
        echo "vulkan samples: $1 sha256 $got, want $want" >&2
        return 1
    fi
    echo "$file"
}

samples=$(archive "vulkan-samples-$SAMPLES_COMMIT.tar.gz" "$SAMPLES_URL" "$SAMPLES_SHA256")
assimp=$(archive "assimp-$ASSIMP_VERSION.tar.gz" "$ASSIMP_URL" "$ASSIMP_SHA256")

mkdir -p "$TREE"
staging=$(mktemp -d "$TREE/.unpack.XXXXXX")
trap 'rm -rf "$staging"' EXIT
tar -xzf "$samples" -C "$staging"
tar -xzf "$assimp" -C "$staging"
for patch in "$PATCHES"/*.patch; do
    patch -d "$staging/Vulkan-Samples-$SAMPLES_COMMIT" -p1 --quiet --forward --no-backup-if-mismatch < "$patch"
done
# The samples load each shader's SPIR-V, shipped beside its GLSL, so a shader
# a patch fixes is compiled again over it.
for shader in $(sed -n 's#^+++ b/\(data/shaders/.*\.\(vert\|frag\|geom\|tesc\|tese\|comp\)\)$#\1#p' "$PATCHES"/*.patch); do
    glslangValidator -V --quiet -o "$staging/Vulkan-Samples-$SAMPLES_COMMIT/$shader.spv" \
        "$staging/Vulkan-Samples-$SAMPLES_COMMIT/$shader"
done
rm -rf "$TREE/Vulkan-Samples" "$TREE/assimp"
mv "$staging/Vulkan-Samples-$SAMPLES_COMMIT" "$TREE/Vulkan-Samples"
mv "$staging/assimp-$ASSIMP_VERSION" "$TREE/assimp"
echo "$PINNED" > "$TREE/VERSION"
echo "vulkan samples: $TREE"
