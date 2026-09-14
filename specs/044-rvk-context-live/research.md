# Research: rvk contexts in a real guest

Two questions decided this plan, both answered from the tree: what stops the
guest from encoding a real stream, and what the host executor does with one. The
answers change the spec's scope in four places, each recorded below with the
reason, and add a security task the spec did not know it needed.

## 1. Encoding a real stream in the guest

`rvktest.exe` is freestanding (no C runtime; `/X /Zl /NODEFAULTLIB`, imports only
gdi32, kernel32, ntdll) and excludes every generated source
(`src/um/CMakeLists.txt:16-20`). The generated Vulkan and OpenCL clients do not
build there for six reasons:

1. `third_party/khronos/include` is not on rvktest's include path.
2. `<cstdint>` and `<cstring>` exist nowhere on that path; `/X` drops system
   includes and the kit ships neither.
3. The kit's `km/crt/cstddef` declares no `std::size_t` without `_GLOBAL_USING`.
4. `vulkan_core.h` includes `vk_video/*.h`, which `tools/fetch_khronos_registries.py`
   never fetches; on Linux they resolve silently from `/usr/include` (a newer
   version than the pinned headers).
5. `handle_to_wire` and `host_image_copy_bytes` are defined in
   `vk_handle_default.cpp` and `vk_host_image_default.cpp`, which are excluded.
6. `strlen` is not among ntdll's imports.

### Decision: the emitters write freestanding C++

The generated code includes `<stdint.h>`/`<stddef.h>`, names unqualified
`uint32_t`/`size_t`, and calls `__builtin_memcpy`/`__builtin_strlen`; `vk_video`
headers are fetched from the already-pinned tarball; the four sources are built
into rvktest; `strlen` is imported from ntdll. This is the constitution's
freestanding subset applied to generated code, and the Vulkan and OpenCL ICDs
(045, 052) are freestanding user-mode DLLs built the same way, so they need it too.

**Rejected:** shim `<cstdint>`/`<cstring>` headers local to rvktest (a fake slice
of `std` that bends the no-standard-library rule); leaving the clients to the
ICDs (the guest would exercise no generated encoder until 045).

## 2. What the host executor does with a real stream

`librvk_executor.so` (042) decodes and runs all three APIs on the bound GPU.
Vulkan and OpenCL contexts create nothing up front; an OpenGL rvk context gets an
EGL context automatically, surfaceless, made current before each command. One
reply per batch is kept; the guest transport's reply slot holds 4064 bytes of
answer and a command body at most 1024 bytes.

### Finding: three guest-to-host memory-safety holes

- **OpenCL handles are cast from the wire unchecked** (`cl_dispatch_server.cpp`,
  e.g. :631-633, :1006-1011); a bogus `cl_mem` from a guest is dereferenced by the
  host ICD inside QEMU. Vulkan validates every handle through
  `handle_to_wire`/`wire_to_handle` and a registry; OpenCL's registry is only
  cleared.
- **`clEnqueueReadBuffer` with `size > ptr_capacity`** writes past the decode arena.
- **`glReadPixels`'s `pixels_bytes` is not checked** against width × height ×
  format, so a mismatch overflows the host buffer.

Constitution V makes these blocking: a guest-to-host escape is the failure this
project exists to prevent, and 044 is the first feature that puts real streams in
front of the executor from a guest. They are fixed here, with the OpenCL and
OpenGL executor fuzz targets 042 T027 listed but never built.

## 3. What the spec asks that cannot be proven yet

- **US1.2's "a buffer in host-visible memory, write a pattern, read it in the
  guest".** `vkMapMemory`/`vkUnmapMemory` are off the wire by design
  (`tools/codegen/profiles/vulkan.toml:246-255`), no command reads `VkDeviceMemory`
  back, blob creation refuses a context object (`blob_commands.cpp:64`), and 041
  could not place an allocation in the host-visible segment. Mapping device
  memory into a guest process is the Vulkan ICD's memory model (045, with 041's
  placement). **Decision:** the Vulkan scenario proves instance → physical device
  → device → buffer → memory → bind on the host GPU; the runner records which GPU
  the device is bound to. The pattern round trip moves to 045,
  whose spec already owns memory types and mapping (FR-003).
- **US1.3's "buffers in host-visible memory".** OpenCL import is not adopted
  (042 T041/T042). **Decision:** vector add over `CL_MEM_COPY_HOST_PTR` buffers
  with the result read back inline; host-visible buffers move to 052.
- **US1.4 (OpenGL clear and read-back).** The generated OpenGL client does not
  link until 050's helpers exist (043 research §4), and hand-writing GL frames
  would duplicate the generator in a test. **Decision:** moved to 050, which
  depends on this feature and is where the OpenGL client first links; 050's spec
  carries the scenario. What 044 proves for OpenGL is that an OpenGL rvk context
  sets up on the host GPU (the executor opens its EGL context) and tears down.
- **US3.1's "host-visible mapping path used (zero-copy or copy fallback)".** No
  mapping path exists to report (see US1.2). The report records the executor
  library, the host GPU and the no-op latency.

## 4. A no-op, and the echo collision

The 043 transport scenario sends `0x10001` with a u64. That is the echo
executor's echo command and also the real `vkDestroyInstance`; under
`librvk_executor.so` it faults its context. And there is no command whose
round-trip time is a no-op's (SC-002).

### Decision: a transport-level no-op the device answers

`docs/protocol.md` reserves API 0 for transport commands and defines none. 044
defines `0x00000001 NOP`: the device's batch executor answers it itself - an
empty reply when asked - without calling the executor library. The transport
scenario sends NOP and times it, so it passes under either executor and measures
exactly the transport, not a backend.

## 5. The runner and the gate

`--executor` defaults to the echo library and is not recorded anywhere. The
real library builds in `build-qemu/librvk_executor.so` (unsanitized; the one QEMU
should load).

### Decision

The runner records the executor it loaded (path and whether it is the real one)
and the capset's GPU in `verdict.txt`, and gains `--rvk` to run the rvk scenarios
(`rvktest.exe rvk`), which need the real library. The gate's guest stage runs the
existing suite with the echo library as today, then the rvk scenarios with the
real library when `build-qemu/librvk_executor.so` exists - two boots, because the
TDR scenario (040) needs the echo library's fault injection and the rvk
scenarios need a real GPU.

## 6. Fault isolation

A malformed stream must fault its own context without handing a host ICD a bad
handle. **Decision:** the isolation scenario sends `vkCreateInstance` with a
truncated payload on one context: the Vulkan decoder - handle-checked and
fuzzed since 042 - answers `RVK_UNDECODABLE`, the device faults that context with
code 6, and the driver reports `DMA_FAULTED` (043). A second context completes a
NOP and a Vulkan call before and after; the first context's next send fails and
the adapter stays started. US2.3 (TDR on one device) is 040's `tdr` scenario
with the echo library's stall, unchanged.
