#!/usr/bin/env bash
# Fetch the Windows kit subset the driver builds against into
# third_party/windows-kit/. The WDK licence allows use for developing drivers,
# not redistribution, so the kit is pinned by hash and fetched, never committed.
set -euo pipefail
cd "$(dirname "$0")/.."

WDK_ID=Microsoft.Windows.WDK.x64
WDK_VERSION=10.0.26100.6584
WDK_SHA256=c393d03dfb640b5c92f546b32f6770ef68cd3aaf691956e7d66d8e2c28a1b55e
SDK_ID=Microsoft.Windows.SDK.CPP
SDK_VERSION=10.0.26100.9169
SDK_SHA256=475269434dcd808a67853773272f972c3229c0e10c3ddc821290e70cc0f6904d
KIT_INCLUDE=c/Include/10.0.26100.0
KIT_LIB=c/Lib/10.0.26100.0/km/x64
LIBRARIES=(ntoskrnl.lib hal.lib displib.lib BufferOverflowK.lib)
# The user-mode headers rvktest.exe reaches through a lean windows.h and
# d3dkmthk.h (src/guest/win32), and nothing else: the um directory is 120 MB.
UM_HEADERS=(apiquery2.h consoleapi.h consoleapi2.h consoleapi3.h datetimeapi.h debugapi.h enclaveapi.h
            errhandlingapi.h fibersapi.h fileapi.h fileapifromapp.h handleapi.h heapapi.h ime_cmodes.h imm.h
            interlockedapi.h ioapiset.h jobapi.h jobapi2.h libloaderapi.h mcx.h memoryapi.h minwinbase.h
            namedpipeapi.h namespaceapi.h processenv.h processthreadsapi.h processtopologyapi.h profileapi.h
            realtimeapiset.h reason.h securityappcontainer.h securitybaseapi.h stringapiset.h synchapi.h
            sysinfoapi.h systemtopologyapi.h threadpoolapiset.h threadpoollegacyapiset.h timezoneapi.h
            utilapiset.h verrsrc.h WinBase.h wincon.h wincontypes.h Windows.h wingdi.h winnetwk.h WinNls.h
            winnt.h winreg.h winsvc.h WinUser.h winver.h wow64apiset.h
# What the D3D probes reach (feature 047, T028): d3d11.h and d3d12.h, their
# COM closure, and the D3D10 and debug-layer headers d3d11.h includes of its
# own accord. d3d9.h, dxgi.h and the RPC headers are in shared/, which is
# taken whole. Together 5.6 MB of the um directory's 120.
            combaseapi.h objbase.h objidl.h objidlbase.h Unknwn.h Unknwnbase.h oaidl.h ocidl.h oleidl.h
            oleauto.h ole2.h PropIdl.h PropIdlBase.h servprov.h coml2api.h cguid.h rpcnsi.h rpcnsip.h
            urlmon.h msxml.h d3dcommon.h d3d11.h d3d11sdklayers.h d3d12.h d3d12sdklayers.h d3d10.h
            d3d10_1.h d3d10_1shader.h d3d10effect.h d3d10misc.h d3d10sdklayers.h d3d10shader.h
# The DXGI DDI rvkumd.dll fills at CreateDevice (feature 055). dxgitype.h and
# dxgiformat.h, which it names, are in shared/, which is taken whole.
            dxgiddi.h)
# What rvkumd.dll is written against (feature 055): the Direct3D user-mode
# driver DDI. These are the WDK's um/ directory, not the SDK's, and are named
# one by one for the same reason - that directory is 120 MB and these are 1.6.
WDK_UM_HEADERS=(d3d10umddi.h d3d12umddi.h d3dumddi.h)

KIT=third_party/windows-kit
CACHE=third_party/.cache
PINNED="$WDK_ID $WDK_VERSION
$SDK_ID $SDK_VERSION
headers km shared um-closure-2 umddi-1"

if [ -f "$KIT/VERSION" ] && [ "$(cat "$KIT/VERSION")" = "$PINNED" ]; then
    echo "windows kit: $KIT already at pinned versions"
    exit 0
fi

# Prints the path of a package in the cache whose SHA-256 matches, downloading
# it if absent. A mismatch deletes the file: a truncated or republished package
# must not survive to the next run.
package() {
    local id=$1 version=$2 want=$3
    local lower=${id,,}
    local file="$CACHE/$lower.$version.nupkg"
    mkdir -p "$CACHE"
    if [ ! -f "$file" ]; then
        echo "windows kit: downloading $id $version" >&2
        curl -sSfL -o "$file.part" \
            "https://api.nuget.org/v3-flatcontainer/$lower/$version/$lower.$version.nupkg"
        mv "$file.part" "$file"
    fi
    local got
    got=$(sha256sum "$file" | cut -d' ' -f1)
    if [ "$got" != "$want" ]; then
        rm -f "$file"
        echo "windows kit: $id $version sha256 $got, want $want" >&2
        return 1
    fi
    echo "$file"
}

wdk=$(package "$WDK_ID" "$WDK_VERSION" "$WDK_SHA256")
sdk=$(package "$SDK_ID" "$SDK_VERSION" "$SDK_SHA256")

mkdir -p third_party
staging=$(mktemp -d third_party/.windows-kit.XXXXXX)
trap 'rm -rf "$staging"' EXIT

unzip -q "$wdk" "$KIT_INCLUDE/km/*" "$KIT_INCLUDE/shared/*" LICENSE.txt \
      "${LIBRARIES[@]/#/$KIT_LIB/}" "${WDK_UM_HEADERS[@]/#/$KIT_INCLUDE/um/}" -d "$staging/wdk"
unzip -q "$sdk" "$KIT_INCLUDE/shared/*" "${UM_HEADERS[@]/#/$KIT_INCLUDE/um/}" -d "$staging/sdk"

mkdir -p "$staging/kit/include" "$staging/kit/lib"
mv "$staging/wdk/$KIT_INCLUDE/km"     "$staging/kit/include/km"
mv "$staging/wdk/$KIT_INCLUDE/shared" "$staging/kit/include/wdk-shared"
mv "$staging/sdk/$KIT_INCLUDE/shared" "$staging/kit/include/sdk-shared"
mv "$staging/sdk/$KIT_INCLUDE/um"     "$staging/kit/include/um"
# The WDK's UMD DDI headers sit beside the SDK's um/ headers: d3d10umddi.h
# includes dxgiddi.h by that name, so one directory has to hold both.
mv "$staging/wdk/$KIT_INCLUDE/um/"*.h "$staging/kit/include/um/"
mv "$staging/wdk/$KIT_LIB"            "$staging/kit/lib/x64"
mv "$staging/wdk/LICENSE.txt"         "$staging/kit/LICENSE.txt"
python3 -B tools/windows_kit_overlay.py "$staging/kit"
printf '%s\n' "$PINNED" > "$staging/kit/VERSION"

rm -rf "$KIT"
mv "$staging/kit" "$KIT"
echo "windows kit: $KIT ($(du -sh "$KIT" | cut -f1))"
