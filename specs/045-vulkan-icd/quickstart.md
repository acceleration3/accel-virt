# Quickstart: proving the Vulkan ICD

## On Linux, in seconds

```bash
cmake --build build
ctest --test-dir build -E tools_tests -j8          # ICD classes over MockKmt, KMD and device units
ctest --test-dir build -R test_fullstack_vulkan_icd --output-on-failure
```

`test_fullstack_vulkan_icd` loads nothing: it calls the ICD's `EntryTable`
directly, as the loader would, over the real driver, device core and host
executor on this machine's GPU (`WidestHostGpu`). Expected: an instance, the one
physical device with a valid LUID, a device, a compute dispatch whose output is
read back through `vkMapMemory` of host-visible memory, the offscreen draw read
back the same way, and a forced context fault answering `VK_ERROR_DEVICE_LOST`
with a recreated device working.

```bash
ctest --test-dir build -R "check_spirv|guest_image" --output-on-failure
```

The committed SPIR-V matches its GLSL, and `rvkvk.dll` imports only gdi32,
kernel32 and ntdll and exports exactly its two entry points.

## The first live experiment (before the ICD is built)

```bash
tools/guest/run_guest_suite.py --image "$AV_GUEST_IMAGE" --quick --rvk \
    --executor build-qemu/librvk_executor.so
```

With `rvktest.exe rvk` carrying the `host_visible` import scenario: a 64 MiB
`host_visible_memory` chunk bound, imported on the host GPU, a pattern the host
writes read in the guest view and one the guest writes read by the host.

## In the guest

```bash
tools/fetch_vulkan_runtime.py      # once: vulkan-1.dll and vulkaninfo.exe, pinned
AV_GUEST_IMAGE=... tools/guest/run_guest_suite.py --vulkan --executor build-qemu/librvk_executor.so
```

The runner installs the package (with `rvkvk.dll`, `rvkvk.json` and the
`VulkanDriverName` value), checks that no Khronos global key exists, runs
`vulkaninfo --summary` (the rvk device with the adapter's LUID, SC-001), then
`rvkvkprobe.exe` (SC-002, SC-003), and pulls the ICD's log into `diag/`.

`AV_GUEST_IMAGE=... tools/gate.sh` runs this as a third guest boot.
