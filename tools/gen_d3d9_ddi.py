#!/usr/bin/env python3
import re
import sys

from ddi_table import DdiTable

ROOT_SLOTS = {"pfnDestroyDevice": "destroy_device9"}
RESERVED = re.compile(r"^pfnReserved")
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
