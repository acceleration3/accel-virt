#!/usr/bin/env python3
import sys

from ddi_table import DdiTable

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
    device="d3d::DeviceBlock<D3d11Device>(device.pDrvPrivate).device()",
    device_class="D3d11Device",
    namespace="av::d3d11",
    prefix="d3d11",
    fill="fill_device_funcs",
    includes=["core/trace/function_scope.hpp", "icd/d3d/device_block.hpp", "icd/d3d/umd_binding.hpp", "icd/d3d/umd_root.hpp", "um/d3d/d3d11_ddi.hpp",
              "um/d3d/d3d11_device.hpp", "um/d3d/d3d11_dxgi_thunks.hpp", "um/d3d/device_call.hpp"],
    trace_calls=True,
    preamble=["/* The ABI boundary C requires: the runtime calls each slot with the device",
              " * handle it was given and nothing else, so each thunk is one line into the",
              " * device the handle's block names. */"],
)


if __name__ == "__main__":
    raise SystemExit(TABLE.main())
