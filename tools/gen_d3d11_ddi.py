#!/usr/bin/env python3
"""The Direct3D 11 device-function table's thunks, from the WDK's own header.

The table the runtime hands a user-mode driver at WDDM 3.2's interface is
D3DWDDM2_6DDI_DEVICEFUNCS: 176 function pointers, each typed by a PFN typedef
that is itself one of several versions of the same DDI. Writing 176 thunks by
hand is 176 chances to get a signature subtly wrong, and a wrong signature is a
stack the runtime and the driver disagree about. So clang reads the kit's
header for the Windows target, and this writes, from the types it reports:

  src/um/d3d/generated/d3d11_device_thunks.cpp   one ABI thunk per slot, each one
                                                  line into D3d11Device, and the
                                                  function that fills the table
  src/um/d3d/generated/d3d11_device_ddi.inc       the D3d11Device methods they
                                                  call, included in its class

    tools/gen_d3d11_ddi.py            # regenerate
    tools/gen_d3d11_ddi.py --check    # fail if the committed files differ
"""
import sys

from ddi_table import DdiTable

# Slots the root answers rather than the device. The runtime destroys a Direct3D
# 11 device through its device table, not its adapter table, and the device
# cannot give itself back: the root tears it down and forgets its block.
ROOT_SLOTS = {"pfnDestroyDevice": "destroy_device"}


TABLE = DdiTable(
    script="tools/gen_d3d11_ddi.py",
    header="d3d10umddi.h",
    probe="""#include <windows.h>
typedef _Return_type_success_(return >= 0) LONG NTSTATUS;
#include <d3d10umddi.h>
D3DWDDM2_6DDI_DEVICEFUNCS *probe;
""",
    table="D3DWDDM2_6DDI_DEVICEFUNCS",
    record="D3DWDDM2_6DDI_DEVICEFUNCS",
    device_type="D3D10DDI_HDEVICE",
    skip=lambda name: False,
    root_slots=ROOT_SLOTS,
    root_argument="device.pDrvPrivate",
    device="DeviceHandle(device.pDrvPrivate).device()",
    device_class="D3d11Device",
    namespace="av::d3d11",
    prefix="d3d11",
    fill="fill_device_funcs",
    includes=["core/trace/function_scope.hpp", "icd/d3d/umd_binding.hpp", "icd/d3d/umd_root.hpp", "um/d3d/d3d11_ddi.hpp",
              "um/d3d/d3d11_device.hpp", "um/d3d/d3d11_dxgi_thunks.hpp", "um/d3d/device_call.hpp",
              "um/d3d/device_handle.hpp"],
    preamble=["/* The ABI boundary C requires: the runtime calls each slot with the device",
              " * handle it was given and nothing else, so each thunk is one line into the",
              " * device the handle's block names. */"],
)


if __name__ == "__main__":
    raise SystemExit(TABLE.main())
