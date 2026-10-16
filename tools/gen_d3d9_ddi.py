#!/usr/bin/env python3
"""The Direct3D 9 device-function table's thunks, from the WDK's own header.

The table d3d9.dll hands a user-mode driver is D3DDDI_DEVICEFUNCS (d3dumddi.h):
some 130 function pointers, each typed by its own PFN typedef. As for Direct3D
11 (tools/gen_d3d11_ddi.py), clang reads the kit's header for the Windows
target and this writes, from the types it reports:

  src/um/d3d/generated/d3d9_device_thunks.cpp   one ABI thunk per slot, each one
                                                 line into D3d9Device, and the
                                                 function that fills the table
  src/um/d3d/generated/d3d9_device_ddi.inc       the D3d9Device methods they
                                                 call, included in its class

Every slot is the device's own: a Direct3D 9 device is named by the HANDLE
CreateDevice handed back, which is the D3d9Device itself.

    tools/gen_d3d9_ddi.py            # regenerate
    tools/gen_d3d9_ddi.py --check    # fail if the committed files differ
"""
import re
import sys

from ddi_table import DdiTable

# Slots the driver answers rather than the device: a device cannot give
# itself back to the pool it lives in.
ROOT_SLOTS = {"pfnDestroyDevice": "destroy_device9"}
# Reserved slots, typed void *, which nothing calls.
RESERVED = re.compile(r"^pfnReserved")
# Slots left NULL on purpose, because d3d9.dll reads a NULL as "feature off"
# and a filled slot as a promise (docs/research/d3d9/reverse-engineering.md
# §1.4, §3.3): capture, async locks, the video, authenticated-channel, crypto
# and content-protection groups (each all-or-nothing), CreateResource2 and the
# 1-suffixed blts (the driver answers DriverVersion 0x2003, where the old slots
# are the ones used and filling both fails CreateDevice above 0x4001), the
# WDDM 1.3 present path, residency trimming and the WDDM 2.1 sync DDIs.
NULL_SLOTS = {
    "pfnGetCaptureAllocationHandle", "pfnCaptureToSysMem", "pfnLockAsync", "pfnUnlockAsync", "pfnRename",
    "pfnCreateVideoProcessor", "pfnSetVideoProcessBltState", "pfnGetVideoProcessBltStatePrivate",
    "pfnSetVideoProcessStreamState", "pfnGetVideoProcessStreamStatePrivate", "pfnVideoProcessBltHD",
    "pfnDestroyVideoProcessor", "pfnCreateAuthenticatedChannel", "pfnAuthenticatedChannelKeyExchange",
    "pfnQueryAuthenticatedChannel", "pfnConfigureAuthenticatedChannel", "pfnDestroyAuthenticatedChannel",
    "pfnCreateCryptoSession", "pfnCryptoSessionKeyExchange", "pfnDestroyCryptoSession", "pfnEncryptionBlt",
    "pfnGetPitch", "pfnStartSessionKeyRefresh", "pfnFinishSessionKeyRefresh", "pfnGetEncryptionBltKey",
    "pfnDecryptionBlt", "pfnCreateResource2", "pfnVolBlt1", "pfnBufBlt1", "pfnTexBlt1", "pfnFlush1",
    "pfnCheckCounterInfo", "pfnCheckCounter", "pfnUpdateSubresourceUP", "pfnPresent1", "pfnSetMarker",
    "pfnSetMarkerMode", "pfnTrimResidencySet", "pfnAcquireResource", "pfnReleaseResource",
}


TABLE = DdiTable(
    script="tools/gen_d3d9_ddi.py",
    header="d3dumddi.h",
    probe="""#include <windows.h>
typedef _Return_type_success_(return >= 0) LONG NTSTATUS;
#include <d3d9types.h>
#include <d3d9caps.h>
#include <d3dumddi.h>
D3DDDI_DEVICEFUNCS *probe;
""",
    table="D3DDDI_DEVICEFUNCS",
    record="_D3DDDI_DEVICEFUNCS",
    device_type="HANDLE",
    skip=lambda name: bool(RESERVED.match(name)) or name in NULL_SLOTS,
    root_slots=ROOT_SLOTS,
    root_argument="device",
    device="*static_cast<D3d9Device *>(device)",
    device_class="D3d9Device",
    namespace="av::d3d9",
    prefix="d3d9",
    fill="fill_device_funcs9",
    includes=["core/trace/function_scope.hpp", "icd/d3d/umd_binding.hpp", "icd/d3d/umd_root.hpp", "um/d3d/d3d9_ddi.hpp",
              "um/d3d/d3d9_device.hpp", "um/d3d/device_call.hpp", "um/d3d/d3d9_tables.hpp"],
    preamble=["/* The ABI boundary C requires: the runtime calls each slot with the device",
              " * handle CreateDevice handed back and nothing else, so each thunk is one line",
              " * into the device that handle is, written to the call trace, and a call",
              " * that fails says so. */"],
    note_failures=True,
    trace_calls=True,
)


if __name__ == "__main__":
    raise SystemExit(TABLE.main())
