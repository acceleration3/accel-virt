# Contract: what the Khronos loader and `rvkcl.dll` promise each other

Source: the pinned loader, `OpenCL-ICD-Loader-2026.05.29` (research §1), and
`third_party/khronos/include/CL/cl_icd.h`.

## Registration

In each rvkgpu adapter's software key, written by the driver package:

| Value | Type | INF flags | Data |
|---|---|---|---|
| `OpenCLDriverName` | `REG_SZ` | `0x00000000` | `%13%\rvkcl.dll` |

`REG_SZ`, not `REG_MULTI_SZ`: the loader's D3DKMT path asks
`D3DKMTQueryAdapterInfo` for a `REG_SZ` and returns nothing for another type,
while its SetupAPI path takes either. `OpenCLDriverNameWow` is feature 061's.
Nothing is written under `HKLM\SOFTWARE\Khronos\OpenCL\Vendors`.

## Exports

```
LIBRARY rvkcl.dll
EXPORTS
clGetExtensionFunctionAddress
clIcdGetPlatformIDsKHR
```

The pinned loader resolves only `clGetExtensionFunctionAddress` by name and
asks it for `clIcdGetPlatformIDsKHR`; older loaders looked the second up
directly. `--only-exports` on the image gate holds the surface to these two.

## The sequence a loader performs, and what must answer

| # | The loader does | This ICD must |
|--:|---|---|
| 1 | `LoadLibrary` the registered path | load with no CRT, DllMain doing nothing |
| 2 | `GetProcAddress("clGetExtensionFunctionAddress")` | answer `clIcdGetPlatformIDsKHR` for that name, null for names it does not serve |
| 3 | `clIcdGetPlatformIDs(0, NULL, &count)` | answer `CL_SUCCESS` and a count, opening no adapter |
| 4 | `clIcdGetPlatformIDs(count, platforms, NULL)` | fill the platforms |
| 5 | check `platform->dispatch` | have written the dispatch pointer as the first word |
| 6 | `clGetPlatformInfo(CL_PLATFORM_EXTENSIONS, ...)`, twice | answer size then data |
| 7 | `clGetPlatformInfo(CL_PLATFORM_ICD_SUFFIX_KHR, ...)`, twice | answer `RVK`; **a failure here drops the platform** |

Steps 3 to 7 happen in every process that calls `clGetPlatformIDs`, including
processes with no interest in this adapter, so none of them may open a D3DKMT
adapter, a device or a context.

## Object layout

Every object handed to the application - `cl_platform_id`, `cl_device_id`,
`cl_context`, `cl_command_queue`, `cl_mem`, `cl_program`, `cl_kernel`,
`cl_event`, `cl_sampler`, and every sub-buffer, sub-device, cloned kernel, user
event and member of `clCreateKernelsInProgram`'s out-array - is a struct whose
**first member is `cl_icd_dispatch *`**, pointing at this ICD's one table. The
loader dereferences that word and jumps; a null or foreign value is a crash
inside the loader with no diagnostic.

## The dispatch table

`cl_icd_dispatch` is positional: 149 active slots at
`CL_TARGET_OPENCL_VERSION 300`, no version field, no count, no tag. A
version-gated slot degrades to `void *` rather than disappearing, so offsets are
invariant.

The ICD zeroes the table, writes its own entries, then calls
`av::clproto::fill_unimplemented(table)` last, which writes only null slots and
reports each refusal once through `av::icd::stub_called`. ICD version 1 only:
the pinned headers carry no `clIcdGetFunctionAddressForPlatformKHR`,
`clIcdSetPlatformDispatchDataKHR` or `CL_ICD2_TAG_KHR`.

## What is reported

| Query | Answer |
|---|---|
| `CL_PLATFORM_ICD_SUFFIX_KHR` | `RVK` |
| `CL_PLATFORM_PROFILE` | `FULL_PROFILE` |
| `CL_PLATFORM_VERSION` | the host's, capped at `OpenCL 3.0` |
| `CL_PLATFORM_EXTENSIONS` | this ICD's policy rows, intersected with the host |
| `CL_DEVICE_VENDOR_ID`, `CL_DEVICE_LUID_KHR` | the rvkgpu adapter's |
| `CL_DEVICE_SVM_CAPABILITIES` | `CL_DEVICE_SVM_COARSE_GRAIN_BUFFER`, or 0 |
| every other `CL_DEVICE_*` | the host device's answer, unchanged |
| `cl_khr_gl_sharing` | not reported until 053 |

One device per rvkgpu adapter, on one platform.
