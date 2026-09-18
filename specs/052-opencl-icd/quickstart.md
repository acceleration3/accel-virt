# Quickstart: proving the OpenCL ICD

## On Linux, in seconds

```bash
cmake --build build -j32
ctest --test-dir build -j32 -LE tools          # ICD classes, the fake loader, device and executor units
ctest --test-dir build -j32 -R test_fullstack_opencl_icd --output-on-failure
python3 tools/check_generated.py               # the ten commands, the new role, the ids
python3 tools/check_extension_policy.py        # every reported extension names a real test
```

`test_fullstack_opencl_icd` drives the ICD through its `cl_icd_dispatch` as the
Khronos loader does, over the real driver, device core and executor on this
machine's GPU: a platform and device are found, a context and queue are made, a
program is built and a kernel run, the output reads back exactly, a mapped
buffer is written through and seen by the kernel, an SVM allocation survives
map, write, unmap and a kernel argument, a callback registered on an event runs
once after the host reaches it, and a forced fault leaves the context lost with
a new one working. The device name the ICD reports is compared with what the
executor answers directly, with no guest in between.

## The three gates that are easy to forget

```bash
ctest --test-dir build -j32 -R "rvkcl_image|rvkclprobe_image"   # exports and imports
ctest --test-dir build -j32 -R tools_test_driver_package        # INF and runner agree
python3 tools/check_protocol_ids.py                             # ids in OpenCL's range, none repeated
```

`rvkcl_image` is `--dll --only-exports` with the two required exports and no
`--allow-import`: this ICD needs gdi32, kernel32 and ntdll and nothing else.

## Coverage and fuzz

```bash
cmake --build build-coverage -j32 && ctest --test-dir build-coverage -j32
cmake --build build-fuzz -j32 && ctest --test-dir build-fuzz -j32 -L fuzz
```

The fuzz targets are the property lists (`cl_context_properties`,
`cl_queue_properties`, `cl_mem_properties`), the info queries
(`clGetDeviceInfo` and friends, where a size and a buffer come from the
application), and the host replies the ICD decodes.

## In the guest

```bash
python3 tools/fetch_opencl_loader.py            # pinned source, cross-built OpenCL.dll
python3 tools/fetch_opencl_loader.py --check
AV_GUEST_IMAGE=... tools/gate.sh                # includes the --opencl boot
```

Expected in `build-guest-opencl/`: `opencl.txt` with
`rvkclprobe: N passed, 0 failed` and an `ok opencl.<step>` line for platform,
device, luid, context, queue, program, kernel, read, map, svm, callback, lost
and recreated; `CL_DEVICE_NAME` naming the host GPU rather than a software
device; and the ICD's log in `diag/` stating which map path the run took and
every refusal it made.

The loader in the guest is the real Khronos one, so this run also proves the
registry path: the ICD is found through `OpenCLDriverName` on the rvkgpu
adapter, with nothing under the global vendors key.
