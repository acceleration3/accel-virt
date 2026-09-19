# Quickstart: validating 053

## Build and run everything

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j"$(nproc)"
ctest --test-dir build -j"$(nproc)"
tools/gate.sh
```

`tools/gate.sh` runs the four Linux configurations. In an environment with no
prepared Windows image it says the guest is skipped, which is not a pass: SC-001
and SC-003 stay open until it runs (research.md §10).

## The wire, on its own

```bash
ctest --test-dir build -R 'shared_object'
```

`test_shared_object_record` and `test_shared_object_request` are the hostile
tables: zero length, every truncation, a wrong magic, an unknown version, an
out-of-range kind, a reserved field set, a size that is not whole pages, and a
byte-flip sweep. Their fuzz targets are

```bash
cmake -S . -B build-fuzz -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DAV_FUZZ=ON
cmake --build build-fuzz -j"$(nproc)"
ctest --test-dir build-fuzz -L fuzz -R 'shared_object'
```

## The miniport's identity

```bash
ctest --test-dir build -R 'shared_object_table|allocation_table|device_table|ddi_scenarios'
```

`test_shared_object_table` mints, opens from a second device, closes in every
order, and refuses an unknown id, a foreign LUID, a mismatched kind and a
double close. `test_ddi_scenarios` drives the same through the registered DDI
table with the fake dxgkrnl: create shareable, open on a second device, close,
destroy.

## The device's identity

```bash
ctest --test-dir build -R 'host_shared_object|context_shared_objects|shared_object_command|transport_command'
```

The one that answers US3 scenario 1 is
`TEST(host_shared_objects, two_contexts_holding_one_object_share_one_descriptor)`,
and the one that answers US1 scenario 3 is
`TEST(host_shared_objects, an_object_outlives_the_context_that_published_it)`.

## End to end, in one process

```bash
ctest --test-dir build -R 'fullstack_shared'
```

`test_fullstack_shared_objects.cpp` runs two rvk contexts on one device through
the real miniport: one publishes, the other opens, both name one host resource,
and the publisher's context is then destroyed with the object still live. On a
machine whose host GPU cannot export by fd the test names the condition and
returns, as `test_fullstack_external_objects` already does.

## The three ICDs

```bash
ctest --test-dir build -R 'external_objects|shared_handles|gl_external|cl_external|cl_gl_sharing'
```

Each checks the refusals its API documents for a handle from nowhere, a named
handle, and a handle type it does not answer - and that the extension is not
reported at all on a scripted host without the fd equivalent.

## In the guest (not available here)

```bash
AV_GUEST_IMAGE=/path/to/windows.qcow2 tools/gate.sh
```

The interop matrix is `rvktest.exe share`, which runs each pair in one process
and across two.
