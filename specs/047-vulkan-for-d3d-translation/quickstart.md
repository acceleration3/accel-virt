# Quickstart: proving 047

## The Linux layers

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build
ctest --test-dir build -LE tools -j6          # unit tests, ASan+UBSan
ctest --test-dir build -R 'generated_sources|protocol_ids|extension_policy'
```

`generated_sources` re-runs the generator and diffs its whole output, so a
profile change that was not regenerated fails there. `extension_policy` fails a
reported extension whose test does not exist.

The extension set as an application sees it:

```bash
ctest --test-dir build -R 'test_icd_extensions|test_descriptor_templates|test_placed_mapping|test_external_objects'
```

What the pinned layers require, against what the ICD reports:

```bash
python3 -B tools/extract_required_extensions.py --check     # are the pins fetched
ctest --test-dir build -R 'tools_test_extract_required_extensions'
ctest --test-dir build -R 'test_layer_requirements'         # every required name is reported
```

The fetchers, which need a network and are not part of any gate:

```bash
python3 -B tools/fetch_dxvk.py            # third_party/dxvk/
python3 -B tools/fetch_vkd3d_proton.py    # third_party/vkd3d-proton/
```

## The host GPU

```bash
ctest --test-dir build -R 'test_fullstack_vulkan_icd|test_fullstack_templates'
```

A descriptor template update and a placed mapping over the real host driver,
and an exported handle imported by a second device in the same process.

## The guest

```bash
AV_GUEST_IMAGE=~/.local/share/libvirt/images/win11.qcow2 \
  python3 -B tools/guest/run_guest_suite.py --image "$AV_GUEST_IMAGE" \
  --work build-guest-d3d --d3d --executor build-qemu/librvk_executor.so
```

which pushes the pinned DXVK and vkd3d-proton DLLs beside the three probes and
runs each in the console session (feature 046): `rvkd3d9probe.exe`,
`rvkd3d11probe.exe` and `rvkd3d12probe.exe`. Each prints one `ok d3dN.<step>`
line per step and leaves a BMP; the runner pulls them into `<work>/images/`.

Expected, on a host GPU meeting both layers' requirements:

```
guest: d3d11 ok: 6 passed, feature level 11_1
guest: d3d9 ok: 5 passed
guest: d3d12 ok: 6 passed, feature level 12_1
guest: d3d.images ok: 3 in .../images
```

A step that fails names the extension or feature the layer said was missing,
which is the whole point of the run: DXVK and vkd3d-proton report the first
thing they cannot find, and the runner repeats it verbatim.

## The full verdict

```bash
AV_GATE_JOBS=6 AV_GUEST_IMAGE=~/.local/share/libvirt/images/win11.qcow2 tools/gate.sh
```
