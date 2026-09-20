# Quickstart: wire recordings (065)

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j32
```

`build/rvkrecord` is the tool; `build/librvk_executor.so` is the library a
replay drives.

## The unit and loopback proof (no guest, no GPU)

```bash
ctest --test-dir build -R 'recording|wire_recorder|replay' --output-on-failure
```

- `test_recording_format` - the file and record headers encode, decode, and
  refuse every hostile case in the table (truncation at every field boundary,
  a length past the file, an unknown kind, a non-zero reserved field, a
  version no reader accepts).
- `test_wire_recorder` - the device core over `MockHost` records a context's
  setup, three batches, a reply and a fault; the bytes `MockHost` kept read
  back as those records in submission order. With recording off, nothing is
  written and no payload is copied.
- `test_recording_reader` - a recording cut at every offset yields every whole
  record before the cut and names the damage offset.
- `test_recording_printer` - a recorded Vulkan batch prints with
  `vkCreateInstance`, a transport batch prints its decoded `import_blob`, and
  each filter keeps only what it names.
- `test_recording_replay` - a recording replays into `MockExecutorOps` and
  reports the one answer that was changed, by fence and command.
- `test_loopback_recording` - a real guest-core-to-device-core run is
  recorded and replayed, and the replies match.

## The fuzz proof

```bash
cmake -S . -B build-fuzz -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DAV_FUZZ=ON
cmake --build build-fuzz -j32
ctest --test-dir build-fuzz -R 'fuzz_recording_(reader|printer)_smoke' --output-on-failure
```

`fuzz_recording_reader` frames the records; `fuzz_recording_printer` walks the
guest's own commands inside a batch record, which the reader never does.

## Making a recording by hand

With the device in QEMU:

```bash
QEMU_MODULE_DIR=build-qemu/qemu build-qemu/qemu/qemu-system-x86_64 \
    -device virtio-gpu-rvk-pci,host-gpu=auto,executor=build-qemu/librvk_executor.so,\
record=/tmp/run.rvk,record-limit=67108864 ...
```

or, in a guest run,

```bash
python3 -B tools/guest/run_guest_suite.py --image "$AV_GUEST_IMAGE" \
    --work build-guest --record        # writes build-guest/recording.rvk
```

## What the recording cost (SC-002)

Two boots, one each way, into the same work directory; each keeps its warm
no-op round trip in `build-guest-real/recording-cost.txt` and the second says
the difference.

```bash
python3 -B tools/guest/run_guest_suite.py --image "$AV_GUEST_IMAGE" \
    --work build-guest-real --executor build-qemu/librvk_executor.so --rvk
python3 -B tools/guest/run_guest_suite.py --image "$AV_GUEST_IMAGE" \
    --work build-guest-real --executor build-qemu/librvk_executor.so --rvk --record
```

The second run prints `recording cost ok: a no-op round trip took N us with
recording off and M us with it on`. The number kept is the median of the 31
warm round trips, not the first cold one, which swings by milliseconds on a
busy host.

## Reading it

```bash
build/rvkrecord print build-guest/recording.rvk
build/rvkrecord print build-guest/recording.rvk --context 3 --command vkCreate
build/rvkrecord print build-guest/recording.rvk --fence 10:20 --bytes 64
```

## Replaying it

```bash
build/rvkrecord replay build-guest/recording.rvk --executor build/librvk_executor.so
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation \
    build/rvkrecord replay build-guest/recording.rvk --executor build/librvk_executor.so
build/rvkrecord replay build-guest/recording.rvk --executor build/librvk_executor.so \
    --gpu <32 hex from tools/host/list_gpus.py>
```

Exit status 0 means every answer matched. A difference is printed with the
fence and the command that produced it.
