---
description: "Tasks: the desktop on the host GPU (058)"
---

# Tasks: the desktop on the host GPU

**Input**: plan.md, spec.md, research.md, data-model.md,
contracts/{scanout_blob,display_host_ops,executor_scanout_export,guest_scanout_gate}.md

**Tests**: written first (constitution IV); `invariants_hold()` after every
mutation; a hostile table and a fuzz target for the two wire records, which are
96 and 48 bytes of guest-authored offsets, strides and rectangles pointing into
memory the host is about to hand a GPU. Commit at every **Checkpoint** once
`ctest --test-dir build -LE tools -j4` passes.

**What is deliberately not here.** A guest-side flip path, a new DDI, the
executor's image export, host-visible blobs as a scanout, and a device-driven
vertical blank. plan.md's "What this feature does not build" says why for each,
and Phase 7 carries them as tasks with what would unblock them.

## Phase 1: Foundational - the two requests on the wire

Nothing decodes either today: `SET_SCANOUT_BLOB` is refused before its bytes
are read (`src/device/control/blob_commands.cpp:29`) and `RESOURCE_FLUSH` has
never reached the core at all.

- [X] T001 `GpuRect` (object) in `src/proto/virtio_gpu/gpu_rect.{hpp,cpp}`: virtio-gpu's four `le32` - `x`, `y`, `width`, `height` - decoded from a `WireReader`, and the one question asked of it, `inside(bounds_width, bounds_height)`, with `x + width` and `y + height` computed in 64 bits. A zero side is not inside anything. Tests, with the two records below, in `tests/unit/proto/virtio_gpu/test_scanout_requests.cpp`: the exact fit, one past on each edge, a zero side on each axis, and `x = UINT32_MAX` with `width = 1`. **As built it has no `invariants_hold`**: a pure decoder has no invariant of its own that its own methods can break, which is why no other virtio_gpu record here has one either, and an invariant that cannot return false is not one (constitution IV.2)
- [X] T002 `SetScanoutBlobRequest` (object) in `src/proto/virtio_gpu/set_scanout_blob_request.{hpp,cpp}`: the 96 bytes of contracts/scanout_blob.md, refusing a short request, a non-zero `padding`, and any non-zero stride or offset of planes 1 to 3; nothing else, because everything else needs the blob. A refused decode leaves every field 0, which `decoded()` states and the fuzz target asserts on every refusal. Tests: truncation at every field boundary, each structural refusal named, the maximum of every scalar carried through unchanged
- [X] T003 `FlushRequest` (object) in `src/proto/virtio_gpu/flush_request.{hpp,cpp}`: the 48 bytes of `virtio_gpu_resource_flush`, same rules, same test file
- [X] T004 `tests/fuzz/fuzz_scanout_request.cpp` with a committed corpus in `tests/corpus/fuzz_scanout_request/`: both records from attacker-chosen bytes, asserting a returned status, no read past the buffer, and that a refused decode answers zero for every field. Seeds: a valid 1280x800 scanout, a valid flush, a 95-byte scanout, a scanout with three planes set, and a flush whose rect wraps

**Checkpoint**: commit. `ctest -R scanout_requests` and a 60-second `fuzz_scanout_request` smoke run pass.

## Phase 2: Foundational - the four display operations

contracts/display_host_ops.md. Every layer of the host boundary at once,
because a `Host` virtual with no implementation does not compile the mock.

- [X] T005 `RvkScanoutFrame` and the four function pointers in `src/device/abi/rvk_device.h`, with the comment on `display_takes_dmabuf` recording why it cannot be asked at realize (`system/vl.c`: `qemu_init_displays` runs after `qmp_x_exit_preconfig`)
- [X] T006 The four virtuals on `Host` (`src/device/host/host.hpp`) and their forwarding in `src/device/abi/table_host.{hpp,cpp}`. Tests `tests/unit/device/abi/test_table_host.cpp`: each forwarded with its arguments intact, and a table whose member is null refused at `rvk_device_create` as every other required member already is
- [X] T007 `MockHost` (`tests/harness/host/mock_host.{hpp,cpp}`) answers the four: a settable head count, a settable per-head answer, a settable `show_scanout` refusal, and a record of every call with every field of its frame. The three contract checks of contracts/display_host_ops.md are recorded violations
- [X] T008 `MockHostOps` (`tests/harness/host/mock_host_ops.{hpp,cpp}`) gains the same four at the C-struct level
- [X] T009 The glue: `rvk_display_takes_dmabuf`, `rvk_show_scanout`, `rvk_update_scanout`, `rvk_hide_scanout` in `src/device/qemu/virtio-gpu-rvk-host.c`, a `QemuDmaBuf *` per head in `VirtIOGPURvk` (`src/device/qemu/virtio-gpu-rvk.h`), and the includes `ui/console.h` and `ui/dmabuf.h` the file does not have today. Each is the translation in contracts/display_host_ops.md and nothing else; the resize-before-scanout and free-after-set orders carry the one-line reason each

**Checkpoint**: commit. `tools/build_qemu.sh` links and the qtest suite still passes unchanged.

## Phase 3: US3 (P1) - it degrades instead of failing

First, because it is the behaviour that already exists and it is what keeps
feature 038's scenarios green (SC-004).

- [X] T010 [US3] `CapsetInfo::feature_dmabuf_scanout = 8u` in `src/proto/context/capset_info.hpp`, carried by the `features_` field already on the wire - no encoding change, no new capability, no new BAR (feature 060's configuration-space budget). Tests `tests/unit/proto/context/test_capset_info.cpp`: the bit round-trips, `has_feature` and `withdraw` treat it as the other three are treated, and a capset with every bit set still encodes in 128 bytes
- [X] T011 [US3] `DeviceCapset::resolve_display()` (`src/device/capset/device_capset.{hpp,cpp}`): asks the host once whether head 0 can show a dma-buf, sets or clears the bit, memoizes the answer, and logs it once with the answer. `ContextCommands` takes a non-const `DeviceCapset &` and calls it before answering `GET_CAPSET`. Tests `tests/unit/device/capset/test_device_capset.cpp`: unasked before the first capset read, asked exactly once across many reads, the bit absent when the host says no, present when it says yes, and never set on a device that was never described
- [X] T012 [US3] `SET_SCANOUT_BLOB` answers `ERR_UNSPEC` when the display cannot take a dma-buf, taking no pin and opening no descriptor. **As built the log is `DisplayHeads`'**, not `ScanoutCommands`': there are two host refusals, not one - no display for this head, and no descriptor over this blob's memory - and each is said once per device, because a guest that keeps asking must not turn the log into the reason the run is slow. Tests `tests/unit/device/display/test_display_heads.cpp` and `tests/unit/device/control/test_scanout_commands.cpp`
- [X] T013 [US3] `tests/unit/device/module/test_virtio_gpu_rvk.cpp`: on a device whose host has no GL display, every scanout command is refused, `RESOURCE_UNREF` of the blob still succeeds, and the capset the guest reads has no scanout bit - the whole fallback as one scenario

**Checkpoint**: commit. The fallback is complete and nothing else has changed behaviour.

## Phase 4: US1 (P1) - the head shows the blob

- [X] T014 [US1] `ScanoutFormat` (object) in `src/device/display/scanout_format.{hpp,cpp}`: the eight rows of data-model.md, `scannable()`, `fourcc()`, `pixel_bytes()`. Tests `tests/unit/device/display/test_scanout_format.cpp`: every row asserted against the rule (a virtio-gpu name is bytes in memory, a DRM fourcc is the same bytes as a little-endian word), id 0, an id between two rows, and `UINT32_MAX`
- [X] T015 [US1] `ScanoutFramebuffer` (object) in `src/device/display/scanout_framebuffer.{hpp,cpp}`: `adopt(request, blob_size)` with every term of data-model.md's check 64-bit and overflow-checked, and `invariants_hold()`. Tests `tests/unit/device/display/test_scanout_framebuffer.cpp`: the framebuffer that is exactly the blob, one byte too big, a stride one short of a row, a stride above `INT32_MAX`, `height` such that `(height - 1) * stride` overflows, `offset = UINT32_MAX`, a rect one past each edge, and every refusal leaving the framebuffer empty
- [X] T016 [US1] `ScanoutBinding` (object) in `src/device/display/scanout_binding.{hpp,cpp}`: head index, blob id, framebuffer, `GuestDmaBuf`, shown flag; `show` makes the new descriptor and takes the new pin before the host is told and lets the old ones go only after, `hide` releases both. Tests `tests/unit/device/display/test_scanout_binding.cpp`: bind, rebind with one `show_scanout` and no `hide_scanout` between, a rebind the host refused leaving the old one shown and the new descriptor closed, unbind, unbind twice, and `invariants_hold()` after each
- [X] T017 [US1] `DisplayHeads` (system) in `src/device/display/display_heads.{hpp,cpp}`: 16 bindings, the three-state display answer, `show`, `hide`, `flush`, `release_all`, `invariants_hold()`. Tests `tests/unit/device/display/test_display_heads.cpp`: two heads showing two blobs, two heads showing one blob (two descriptors, two pins), a head the host does not have, `release_all` from every state, and the four invariants each made to fail by a deliberate mutation through a probe
- [X] T018 [US1] `ScanoutCommands` (object) in `src/device/control/scanout_commands.{hpp,cpp}`, wired into `VirtioGpuRvk::control()` beside `ContextCommands` and `BlobCommands`; `VirtioGpuRvk` gains the `DisplayHeads` member and releases it before the blob table on reset. Tests `tests/unit/device/control/test_scanout_commands.cpp`: all sixteen refusals of contracts/scanout_blob.md named one test each, the disable, the bind, the rebind, and that a refusal changes nothing
- [X] T019 [US1] `RESOURCE_FLUSH` naming a blob: answered by the core, every head showing the blob updated, refused when no head shows it or the rect is outside. A 2D resource is still the base class's, asserted. Tests in the same file
- [X] T020 [US1] `SET_SCANOUT` naming a blob refused with `ERR_INVALID_RESOURCE_ID`; `RESOURCE_UNREF` of a shown blob refused with `ERR_INVALID_PARAMETER` and the blob not released, then released once the head is hidden. Tests in the same file and in `tests/unit/device/control/test_blob_commands.cpp`, whose existing `set_scanout_blob` refusal assertion is replaced rather than deleted
- [X] T021 [US1] The allocation-failure sweep: with `MockHost`'s dma-buf refusal, event and mutex failures and `show_scanout` refusal each failed in turn, no head is left half-shown, no pin is left taken and no descriptor is left open. `tests/unit/device/display/test_display_heads.cpp`
- [X] T022 [US1] `tests/unit/device/module/test_virtio_gpu_rvk.cpp`: the whole sequence a guest performs - create a guest blob, bind it to head 0, flush it twenty times, rebind head 0 to a second blob, unref the first, hide, unref the second - asserting on the mock what the host was told at every step, and that the count of bytes the device copied out of guest memory is zero throughout (SC-001's device-side form)

**Checkpoint**: commit. Coverage of `src/device/display` and the new `src/proto/virtio_gpu` files is 100%.

## Phase 5: US2 (P2) - which head shows what, observably

- [X] T023 [US2] `DisplayHeads::report(StateWriter&)` writing one line per head - the blob, the geometry, the fourcc and the count of frames shown - and `VirtioGpuRvk` reporting it. Tests `tests/unit/device/display/test_display_heads.cpp`: a head showing nothing, a head showing a blob, sixteen heads, and that the report takes no lock it would have to wait for (docs/trace.md)
- [X] T024 [US2] FR-006's logging, **as built**: the display's answer once (`DeviceCapset::resolve_display`), and each of the two host refusals once per device. A line per scanout-source change is deliberately not written - a fullscreen application rebinds its head every frame (US2), and sixty lines a second is not a record of a decision. What each head is showing, and how many frames it has shown, is T023's state dump instead
- [X] T025 [US2] A qtest scenario in `tests/qemu/test_virtio_gpu_scanout.py`: over the real patched QEMU, which a qtest runs with no display backend at all, a guest blob created, `SET_SCANOUT_BLOB` refused with `ERR_UNSPEC`, the capset read back without the bit, and every structural and framebuffer refusal of contracts/scanout_blob.md answered as the contract says. This is the configuration the runner uses, so it is the one that must never regress

**Checkpoint**: commit.

## Phase 6: Polish and the record

- [X] T026 `docs/protocol.md`: the `SET_SCANOUT_BLOB | ERR_INVALID_PARAMETER` row replaced by the table of contracts/scanout_blob.md, a section on what a scanned-out blob is, and the capset's feature bits listed with the new one
- [X] T027 `docs/kb/06-display.md` gains the paragraph research §4 establishes - that `qemu_console_has_gl` is set by the display backend and not by the device, and that `-display none` therefore has no dmabuf scanout at all - and `docs/testing.md` gains the runner flag T029 needs
- [X] T028 `tools/gate.sh`'s qemu configuration runs `tests/qemu/test_virtio_gpu_scanout.py`; `tools/check_symbol_tests.py` and `tools/check_design.py` pass on every new file; the untestable baseline is unchanged (this feature adds no `AV_UNTESTABLE`)

## Phase 7: What needs the real guest, merged 056, or 041

Each of these is blocked on something outside this branch. Each says what
unblocks it and what line proves it.

- [X] T029 **Needs the runner.** `tools/guest/run_guest_suite.py` gains `--display egl-headless` (today it is `-vga none -display none`, line 410) and the display scenario runs under it. Proves research §4's claim that `screendump` still works when the console is a GL console: `screen0.ppm` is 1280x800, one colour, no placeholder grey. Until this runs, nothing in this feature has been exercised with a GL display

  *Done, and it disproved the claim it was written to prove.* The flag is there and defaults to `none`. With `egl-headless` the device answers "the display shows dma-bufs" and the guest scans its primary out as a blob - and **`screendump` answers "no surface" for every such console**, ours and stock virtio-gpu-gl's alike, because `qemu_console_surface` answers nothing once `scanout.kind` is `SCANOUT_DMABUF` (research §4, corrected). So the display scenarios that read a PPM stay on the default `-display none`, where the guest keeps the 2D path, and the flag runs a scanout scenario that reads the device's own account instead: a head showing a blob, no refusal in the device's log, no carry failure in the driver's. `tools/gate.sh` runs that boot beside the other two
- [X] T030 **Needs the host.** `ls /dev/udmabuf`, `qemu-system-x86_64 -display help | grep egl`, and `ldd` on QEMU's `ui-opengl` module for `libgbm`. `egl_scanout_dmabuf` is compiled out without `CONFIG_GBM` and the whole path is then a silent no-op; the device would show a scanout QEMU drops

  *Measured on this machine: yes to all four.* `/dev/udmabuf` exists (group `kvm`), `egl-headless` is an available backend, `build-qemu/qemu/ui-opengl.so` links `libgbm.so.1` and `libdrm.so.2`, and `CONFIG_GBM` is defined in this build's `config-host.h` - so `egl_scanout_dmabuf` is compiled in rather than silently absent
- [ ] T031 **Needs 041.** The primary as a host-visible blob: `qemu_memfd_alloc`'s seals in `rvk_create_host_memory` become `F_SEAL_SHRINK` (udmabuf takes no other kind, docs/testing.md), `HostMemoryBlock` answers a descriptor, and contracts/scanout_blob.md's "a host-visible blob is refused" row becomes a success. Rung 2 of research §3 - the first rung on which the desktop is not in guest RAM
- [X] T032 **Needs 041 and 056.** The guest half of contracts/guest_scanout_gate.md: `ScanoutResource` backed by a blob instead of a 2D resource when the capset bit is set, `DisplayScanout::refresh` sending `SET_SCANOUT_BLOB` and no `TRANSFER_TO_HOST_2D`, decided once at `StartDevice` and logged once

  *Built, and it needed neither.* The middle row of the gate's table - "present, primary is guest-backed" - is the primary the guest already has: its pages are guest RAM, which is exactly what a `GUEST` blob is made over and what udmabuf takes. 041 decides whether those pages are the hostmem BAR instead (rung 2) and 056 decides whether anything but WARP ever writes them; neither is needed to stop copying the frame.

  *As built:* `ScanoutBlobSource` is a second class rather than a second mode of `ScanoutResource` - one responsibility each, and the 2D path is untouched, which is what keeps 038's scenarios green. It makes the blob at the first refresh that carries an address rather than at the mode commit, because a blob is made *over* the primary's pages and the commit knows no primary. `DisplayScanout` chooses between the two at `offer(capset)`, from `Adapter::remember_device`, beside where the host-visible segment reads its own bit. Measured in a guest: "a head shows a guest blob; no frame is copied any more" 
- [ ] T033 **Needs 056.** Executor ABI version 6 and `VulkanBackend::export_image`, exactly as contracts/executor_scanout_export.md specifies, plus the create-time flag the guest sets on an image it may scan out. Rung 3 - the desktop composed in the host GPU's own memory and never leaving it

  *Half of it arrived from feature 053.* The ABI is version 6 already (`RVK_EXECUTOR_ABI_VERSION_SHARING`), and `export_object` does hand a host object out as a dma-buf fd - that is how a shared object reaches another executor. What rung 3 still needs on top: the *image* shape (`vkGetImageDrmFormatModifierPropertiesEXT` and `vkGetImageSubresourceLayout` to fill `qemu_dmabuf_new`'s modifier, strides and offsets, or a linear scanout image), a create-time flag so the guest's image is allocated exportable, a sync fd for `allow_fences` (053 exports semaphores as `OPAQUE_FD`, not `SYNC_FD`), and 056, without which nothing produces such an image
- [ ] T034 **Needs 056.** SC-002: DWM composes on rvkgpu after install, after a TDR and after a DWM restart, checked by the in-guest runner through DWM's adapter selection as 056's US2 already describes
- [ ] T035 **Needs 056 and T029.** SC-001 measured live: one minute of desktop animation, the device's counter of bytes transferred out of guest memory still zero, and a host-side capture of the dmabuf matching a reference
- [ ] T036 **Needs T035.** FR-005: the device's vertical blank, in the shape contracts/guest_scanout_gate.md gives it. Blocked on a desktop that is actually flipping, because until then there is no pacing to improve and no way to tell a correct vblank from a wrong one
