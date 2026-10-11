# Vendor libraries

avgpu presents its host GPU's PCI vendor, device and revision as its own
(subsystem `1af4:5256`), so Device Manager, DXGI and WMI all show an AMD or
NVIDIA GPU. Some applications go further: they trust a GPU only after the
vendor's own management library has described it, and pair that description
with a Direct3D adapter and an OpenCL device by where the GPU sits on the PCI
bus. Without that library they find no GPU at all.

DaVinci Resolve is the case that made this necessary. Its `gpudetect.dll`:

1. enumerates DXGI adapters and reads each one's bus and device number through
   SetupAPI (`DEVPKEY_Device_BusNumber`, `DEVPKEY_Device_Address`);
2. asks ADL (`atiadlxx.dll`) or NVAPI (`nvapi64.dll`) for the vendor's GPUs and
   pairs each one with an adapter by bus/device, or by the hardware ID it
   parses out of ADL's `strPNPString`;
3. attaches an OpenCL device to each GPU by `CL_DEVICE_TOPOLOGY_AMD` (AMD) or
   `CL_DEVICE_PCI_BUS_ID_NV`/`CL_DEVICE_PCI_SLOT_ID_NV` (NVIDIA). An Intel GPU
   is paired with an OpenCL platform whose name contains the vendor and that
   offers `cl_khr_d3d10_sharing`.

An adapter that fails any step does not appear in Resolve's GPU list.

## What avgpu ships

The driver package copies four libraries into the system directories, where
an application's `LoadLibrary("atiadlxx.dll")` or `LoadLibrary("nvapi64.dll")`
finds them (`src/km/avgpu.inf`, `[Avgpu_Vendor64]` and `[Avgpu_Vendor32]`):

| Library | Directory | API |
|---|---|---|
| `atiadlxx.dll` | System32 | ADL, 64-bit |
| `atiadlxy.dll` | SysWOW64 | ADL, 32-bit |
| `nvapi64.dll` | System32 | NVAPI, 64-bit |
| `nvapi.dll` | SysWOW64 | NVAPI, 32-bit |

A guest that also runs the vendor's real driver on a real GPU would have its
library replaced; avgpu's guests have no other GPU, so nothing else provides
these names.

Each library describes the avgpu adapters whose PCI vendor is its own
(`0x1002` for ADL, `0x10DE` for NVAPI). It reads every fact from Windows on
each call (`VendorAdapters`, over `D3DKMTQueryAdapterInfo`) and leaves out an
adapter it cannot describe fully. With no adapter of its vendor it refuses to
initialise, as the real library does on a machine without that vendor's
driver, so an application takes the path it takes anywhere else.

## Only what is true

Neither library is a reimplementation. Each answers the identification calls
an application makes to find a GPU, with facts Windows already states, and
nothing more:

| Fact | Source |
|---|---|
| vendor, device, subsystem, revision | `KMTQAITYPE_PHYSICALADAPTERDEVICEIDS` |
| bus, device, function | `KMTQAITYPE_ADAPTERADDRESS` |
| name | `KMTQAITYPE_ADAPTERREGISTRYINFO` (`AdapterString`) |
| driver version | `KMTQAITYPE_KMD_DRIVER_VERSION`, the version the INF states |
| memory | `KMTQAITYPE_GETSEGMENTSIZE` |
| integrated or discrete | avgpu's UMDRIVERPRIVATE features, `UmdAdapterInfo::feature_integrated_gpu` |
| primary display | the adapter the primary display's device context opens (`D3DKMTOpenAdapterFromHdc`) |

Overclocking, display configuration, power, fan control and every other call
the real libraries offer are left out. ADL leaves such functions unexported.
NVAPI's `nvapi_QueryInterface` returns null for an ID it doesn't answer. Both
are what an application already meets on an older driver that predates the
call, and every application handles that.

ADL (`src/icd/vendor/adl_root.hpp`) exports the `ADL_` and `ADL2_` forms of
`Main_Control_Create`/`Destroy`/`Refresh`, `Adapter_NumberOfAdapters_Get`,
`Adapter_AdapterInfo_Get`, `Adapter_Active_Get`, `Adapter_ASICFamilyType_Get`,
`Adapter_Primary_Get` and `Graphics_Versions_Get`, with
`ADL2_Main_Control_GetProcAddress` and `ADL2_Graphics_VersionsX2_Get`
(`src/um/vendor/adl.def`). An ADL2 context is the library's one root, so the
ADL and ADL2 halves share one state.

NVAPI (`src/icd/vendor/nvapi_root.hpp`) answers `Initialize`, `Unload`,
`EnumPhysicalGPUs`, `GPU_GetFullName`, `GPU_GetPCIIdentifiers`,
`GPU_GetBusId`, `GPU_GetBusSlotId`, `GPU_GetGPUType`, `GPU_GetMemoryInfo`,
`GPU_GetAdapterIdFromPhysicalGpu`, `SYS_GetDriverAndBranchVersion`,
`EnumNvidiaDisplayHandle` and `GetPhysicalGPUsFromDisplay`, by the IDs in
NVIDIA's MIT-licensed `nvapi_interface.h` (`src/um/vendor/nvapi_exports.cpp`).

## The OpenCL side

The OpenCL ICD answers the matching device queries from the same facts, not
the host's, since the host's would name a bus the guest has no part of
(`ClDeviceReport`, `ClIcdRoot::describe_adapter`):

- `CL_DEVICE_VENDOR_ID` and `CL_DEVICE_NAME`: the adapter's.
- `CL_DEVICE_PCI_BUS_INFO_KHR`: reported as `cl_khr_pci_bus_info` once the
  adapter is described.
- On an AMD adapter, `CL_DEVICE_TOPOLOGY_AMD` and `CL_DEVICE_BOARD_NAME_AMD`,
  reported as `cl_amd_device_attribute_query`.
- On an NVIDIA adapter, `CL_DEVICE_PCI_BUS_ID_NV`, `CL_DEVICE_PCI_SLOT_ID_NV`
  and `CL_DEVICE_PCI_DOMAIN_ID_NV`, reported as
  `cl_nv_device_attribute_query`.
- `CL_PLATFORM_NAME`: `avgpu OpenCL on <adapter name>`. Asked before any
  device exists, it is found through the adapter's handle alone, without
  opening a device.

The vendor extensions' other queries still go to the host device, which
answers them where its own driver carries the extension.

## Layout

- `src/icd/vendor/`: the portable decisions (`VendorAdapters`, `AdlRoot`,
  `NvapiRoot`), unit tested over `MockKmt`. `ddi/` holds the ADL and NVAPI
  structures they fill, as records.
- `src/um/vendor/`: the exports, the `VendorDriver` that builds a root over
  `GdiKmt` on first use, and `vendor_record_layout.cpp`, which proves the
  records against the vendors' layouts.
