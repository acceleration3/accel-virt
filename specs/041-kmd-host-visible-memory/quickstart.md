# Quickstart: validating host-visible memory

Everything below runs from the repository root.

## The four Linux configurations

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ && cmake --build build
ctest --test-dir build                      # unit tests, design, symbols, scenarios
tools/gate.sh                               # the above plus coverage, fuzz and qemu
```

What to look for:

- `test_host_visible_segment`, `test_memory_segments`, `test_hostmem_placement`
  and `test_host_visible_blob` - the classes this feature adds;
- `test_ddi_scenarios` - the residency sequence in
  [`contracts/residency_sequences.md`](./contracts/residency_sequences.md),
  driven through the real DDI table for an adapter with the region and for one
  without it;
- `qemu_virtio_gpu` - the requests the driver now emits, against the real
  device: a host blob created, mapped at the offset the driver chose, refused
  where it does not fit, and its contents surviving an unmap.

## The real device, by hand

```bash
cmake -S . -B build-qemu -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DAV_QEMU=ON
cmake --build build-qemu
ctest --test-dir build-qemu -R qemu_virtio_gpu
```

`tests/qemu/test_virtio_gpu_blobs.py` is where the region's rules live; the
tests this feature adds use the same helpers (`host_blob`, `map_blob`) with the
offsets the driver would choose.

## The guest

```bash
python3 -B tools/guest/run_guest_suite.py \
    --image "$AV_GUEST_IMAGE" --work build-guest --hostmem 256
```

The run prints, in order:

```
guest: check ok: 1 monitor(s)
guest: segments ok: 2 reported, the second 256 MiB of host memory at 0x...
guest: hostvisible ok: 4096 bytes written through the guest's view
guest: hostvisible ok: the host reads the same bytes out of the BAR
guest: report ok: N passed
```

The third line is the one that cannot be faked from inside the guest: it is the
host's own view of the BAR, read back through QMP after the guest wrote through
its mapping.

`--hostmem 0` runs the same suite against a device with no region, where the
driver must report one segment and place the same allocation in guest memory -
the fallback FR-007 asks to be observable.

## What failure looks like

| Symptom | What it means |
|---|---|
| `segments ok: 1 reported` with `--hostmem 256` | the capset's host-visible bit is clear: the device has no region, or `CapsetExchange` failed at StartDevice |
| the guest's own read-back passes but the host's does not | the view is guest RAM, not the BAR: the allocation was placed in the wrong segment |
| `ERR_INVALID_PARAMETER` on a map | the driver chose an offset the device refuses - misaligned, outside the region, or overlapping a live mapping |
| a bugcheck in `DxgkDdiBuildPagingBuffer` | a transfer named a segment the driver reported but cannot back |
