# Quickstart: rvkumd.dll

## Build and run its tests

```bash
tools/fetch_windows_kit.sh                 # now also d3d10umddi.h, d3d12umddi.h, dxgiddi.h
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j4
ctest --test-dir build -j4 -LE tools -R '^test_(umd|fake_d3d|fullstack_umd)'
```

The UMD's own tests are `tests/unit/icd/d3d/test_*.cpp`; they link
`av_d3d_umd`, and like the two ICDs' tests they may not be linked into any
other binary.

## Drive the driver as a runtime would

```cpp
UmdRig rig;
FakeD3dRuntime runtime(rig.root());
CHECK_STATUS(runtime.open(RuntimeKind::d3d10_11,
                          D3DWDDM3_2_DDI_INTERFACE_VERSION, runtime_build), Status::ok);
std::uint64_t words[4] = {};
std::uint32_t entries = 4u;
CHECK_EQ(runtime.supported_versions(&entries, words), S_OK);
CHECK_EQ(entries, 1u);
CHECK_EQ(words[0], D3DWDDM3_2_DDI_SUPPORTED);
CHECK_EQ(runtime.calc_private_device_size(), sizeof(av::d3d::DeviceCookie));
CHECK_EQ(runtime.create_device(), E_NOTIMPL);      // until feature 056
CHECK_EQ(runtime.close_adapter(), S_OK);
CHECK(rig.root().invariants_hold());
CHECK_EQ(rig.kmt().open_objects(), 0u);
```

## Check the image the guest will load

```bash
ctest --test-dir build -R rvkumd_image
```

That runs `tools/check_guest_image.py` over `build/guest/rvkumd.dll` with
`--dll --only-exports`, so an export the two runtimes do not resolve, or an
import beyond gdi32/kernel32/ntdll, fails the build.

## Where to look

| Question | File |
|---|---|
| Which export does which runtime call, and what does the INF say | `contracts/umd_interface.md` |
| What fills the DXGI table, and which slot is whose | `contracts/dxgi_seam.md` |
| What the KMD refuses from a user-mode process | `contracts/private_data.md` |
| Why any of it is the way it is | `research.md` |
| What each class owns | `data-model.md` |
