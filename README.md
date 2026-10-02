# accel-virt

A paravirtual GPU for Windows 11 guests on QEMU/KVM.

Programs in the guest use Direct3D 11 and 12, Vulkan, OpenGL and OpenCL as they
would on a physical card. Every command crosses to the host and runs on the
host's own GPU stack: its Vulkan driver, its OpenGL through EGL, its OpenCL,
and DXVK and vkd3d-proton (built natively for Linux) for Direct3D. The desktop
runs on it too. DWM composes on the host GPU, and the host scans the result out
without copying it back into the guest.

In the guest, the adapter is named after the host GPU it runs on. Device
Manager, `dxdiag`, DXGI and every application see "AMD Radeon RX 9070 XT", or
whichever GPU the host has, with its vendor and device IDs and its VRAM as the
dedicated video memory.

| Piece | What it is | Source |
|---|---|---|
| `avgpu.sys` | A WDDM 3.2 kernel-mode display miniport. It speaks virtio-gpu plus this project's av protocol. | `src/miniport/` (portable), `src/km/` (Windows shim) |
| `avumd.dll` | The Direct3D 9, 11 and 12 user-mode driver, with DXGI | `src/icd/d3d/`, `src/um/d3d/` |
| `avvk.dll` | The Vulkan installable client driver | `src/icd/vulkan/`, `src/um/vulkan/` |
| `avgl.dll` | The OpenGL installable client driver (the `Drv*` interface) | `src/icd/opengl/`, `src/um/opengl/` |
| `avcl.dll` | The OpenCL installable client driver | `src/icd/opencl/`, `src/um/opencl/` |
| `avumd32.dll`, `avvk32.dll`, `avgl32.dll`, `avcl32.dll` | The same four, built for x86, which 32-bit processes load | the same sources, built with `AV_GUEST_ARCH=x86` |
| `virtio-gpu-av-pci` | A QEMU device, built into a patched QEMU 11.1.1 | `src/device/` |
| `libav_executor.so` | The host library that runs guest commands on the host GPU | `src/executor/` |
| `av-renderer` | An optional separate process for the executor, so that a host driver crash cannot take the VM | `src/device/renderer/`, `src/device/isolation/` |
| the av protocol | Shared by both sides, specified in [`docs/protocol.md`](docs/protocol.md) | `src/proto/` |

Everything is written from the WDK headers and from reverse engineering
Microsoft's shipping binaries (`dxgkrnl.sys`, `dxgmms2.sys`, `cdd.dll`,
`basicrender.sys`, ...) against their public symbols. The findings are in
[`docs/kb/`](docs/kb/) and in comments beside the code they shaped.

**Contents:**
[How it works](#how-it-works) ·
[Status](#status) ·
[Limitations and nuances](#limitations-and-nuances) ·
[Requirements](#requirements) ·
[Building](#building) ·
[Installing](#installing) ·
[Device parameters](#device-parameters) ·
[Resolutions](#resolutions) ·
[Environment variables](#environment-variables) ·
[What the driver package installs](#what-the-driver-package-installs) ·
[Debugging](#debugging) ·
[Testing](#testing) ·
[Working on the code](#working-on-the-code) ·
[Source layout](#source-layout) ·
[Documentation](#documentation)

---

## How it works

```
 Windows 11 guest                                                    Linux host
 ───────────────────────────────────────────────────                ──────────────────────────────────────────────
 application
   ├─ d3d11.dll / D3D12Core.dll ─→ avumd.dll ┐
   ├─ vulkan-1.dll ──────────────→ avvk.dll  │  one av context each:          QEMU  ─ virtio-gpu-av-pci
   ├─ opengl32.dll ──────────────→ avgl.dll  ├─ ring in guest RAM ──────────→        (libav_device.a)
   └─ OpenCL.dll ────────────────→ avcl.dll  ┘  doorbell (BAR 0, ioeventfd)          │ per-context workers
                                                  fence page + MSI-X ←──────────────  │
 DWM, GDI (cdd.dll) ─→ dxgkrnl ─→ avgpu.sys ── virtio-gpu control queue ──→   libav_executor.so
                                  (VidPn, VidMm, VidSch,                        (in QEMU, or in av-renderer)
                                   scanout, cursor, fences)                       ├─ Vulkan      → host Vulkan driver
                                                                                  ├─ OpenGL      → EGL + host GL
                                                                                  ├─ OpenCL      → host OpenCL (e.g. rusticl)
                                                                                  └─ Direct3D    → DXVK (11), vkd3d-proton (12)
                                                                                                    → host Vulkan
```

**One adapter per host GPU.** Each `virtio-gpu-av-pci` device binds one host
GPU by its Vulkan device UUID, which survives reboots and driver updates where
enumeration order does not. The PCI function presents that GPU's vendor, device
and revision as its own. Its subsystem stays `1af4:5256`, which is how the
driver package recognises the device. The adapter's name comes from the GPU's
Vulkan device name. A second host GPU can be added as a second device. With
`display=off` it is a render-only adapter, and frames rendered on it are shown
through the display adapter.

**av contexts.** Each API context a process creates, whether a Vulkan device,
a GL context, a CL context or a D3D device, gets an av context:

- a ring in guest memory that the process writes command batches into;
- a doorbell page in BAR 0, which is a KVM ioeventfd, so a 4-byte store wakes
  the host worker without passing through QEMU's main loop;
- a fence page and a reply slot, with completion reported through MSI-X fence
  queues.

The kernel driver creates and destroys contexts, but steady-state submission
bypasses it. This is WDDM 3.1 user-mode submission, with doorbells and native
fences, both enabled by the driver package.

**Commands are generated, and the guest is untrusted.** The serialization for
Vulkan (386 commands), OpenGL (1375 entry points) and OpenCL (105 commands) is
generated from the Khronos registries by `tools/codegen/`. Direct3D's is
generated from this project's own registries (`tools/codegen/registry/*.toml`).
The output is committed under `src/proto/generated/` and never hand-edited.
The host decodes every command against the registry's bounds and judges it
before a host driver sees it. An object ID of the wrong kind, an address in no
allocation, or a length past the frame faults that context and nothing else.
Host drivers trust their callers; a guest is not a caller to trust.

**Memory.** The host GPU reads guest pages directly
(`VK_EXT_external_memory_host`, or a dma-buf made through `/dev/udmabuf`). A
`hostmem=` window adds a BAR of host memory the guest CPU maps write-back.
Uploads, readbacks and `HOST_VISIBLE` Vulkan memory are placed there, so CPU
reads of GPU results are cached rather than uncached. The host GPU's VRAM is
reported to Windows as dedicated video memory. Nothing is ever placed in that
segment; it is a budget, and what lives there stays on the host
([`docs/host-window.md`](docs/host-window.md)).

**Display.** The host decides the resolutions ([Resolutions](#resolutions)).
The desktop's primary surface is a blob the host scans out directly as a
dma-buf when the display backend has a GL context (`gtk,gl=on` or
`egl-headless`). Otherwise it is transferred. Vsync is a synthetic 60 Hz, and
the cursor is a 64x64 hardware cursor.

**Isolation.** With `renderer=`, the whole host GPU stack runs in
`av-renderer`. A crash there loses the device's contexts, which the guest sees
as a device reset that Windows already recovers from, and QEMU starts a new
renderer ([`docs/renderer.md`](docs/renderer.md)).

## Status

| API | Guest path | Host path | What is offered |
|---|---|---|---|
| Desktop (DWM, GDI) | `avumd.dll` (Direct3D 11) | DXVK | composited on the host GPU, scanned out zero-copy, 60 fps |
| Direct3D 11 | `d3d11.dll` → `avumd.dll` (`OpenAdapter10_2`, the D3D11 DDI at WDDM 3.2) | DXVK's `d3d11`, native | feature levels up to 12_1 |
| Direct3D 12 | `D3D12Core.dll` → `avumd.dll` (`OpenAdapter12`) | vkd3d-proton, native and patched | what vkd3d-proton offers on the host GPU |
| Vulkan | Khronos loader → `avvk.dll` | the host's Vulkan driver | Vulkan 1.3, with the host device's extensions under the ICD's extension policy |
| OpenGL | `opengl32.dll` → `avgl.dll` | EGL + the host's GL | up to 4.6, core and compatibility profiles, WGL pixel formats and sharing |
| OpenCL | Khronos loader → `avcl.dll` | the host's OpenCL | OpenCL 3.0, including SVM and `cl_khr_gl_sharing` |
| Direct3D 9 | DXVK's Windows `d3d9.dll` beside the application → `avvk.dll` (`avvk32.dll` for a 32-bit one) | the host's Vulkan driver | no native D3D9 user-mode driver yet |

Every row works in 32-bit processes too. The package installs an x86 build of
each user-mode driver, and Windows gives it to a WoW64 process through the
adapter's `*Wow` registry values. A 32-bit game on Direct3D 11 runs on the host
GPU as a 64-bit one does.

Tested beyond the probes and sample suites:
- the Windows desktop;
- Minecraft's Vulkan renderer;
- Photoshop CS6's OpenCL path (Blur Gallery, whose GPU check times OpenCL
  copies and needs 12 GB/s);
- the D3D11, D3D12, OpenGL, OpenCL and Vulkan sample suites under `samples/`,
  side by side with the same programs run natively on the host.

## Limitations and nuances

**Guest version and signing:**
- **Windows 11 24H2 (build 26100) or later.** The driver declares WDDM 3.2
  (`0x11007`), and Windows 11 refuses to load a driver that declares an older
  level. So there is no Windows 10 or 23H2 build.
- **Test-signed.** The guest needs test signing on, which means UEFI without
  enrolled Secure Boot keys. `tools/guest/provision.ps1` makes a self-signed
  certificate, trusts it, and signs the package.

**Coverage gaps:**
- **32-bit processes need their own loaders.** A 32-bit Vulkan or OpenCL
  application finds `vulkan-1.dll` and `OpenCL.dll` in `C:\Windows\SysWOW64`,
  and Windows ships neither (see [Installing](#installing)).
- **No native Direct3D 9.** `avumd.dll` exports `OpenAdapter10_2` and
  `OpenAdapter12` only. A D3D8 or D3D9 program runs by putting DXVK's Windows
  `d3d9.dll` beside it, which then runs on the Vulkan ICD. That is the path
  `avd3d9probe.exe` checks.

**Display:**
- **One display per adapter**, at 60 Hz.
- **Resolutions have hard limits.** From 100x100, the smallest source mode
  dxgkrnl accepts, up to 4095 pixels per edge with a pixel clock of at most
  655.35 MHz at 60 Hz, which is what an EDID detailed timing can describe.
  3840x2160 works; 5120x2880 is left out.
- **Absolute pointer only.** Use the USB tablet. Relative mouse mode, which
  games want for mouse-look, loops on GTK and flickers on SDL under a Wayland
  host.

**Driver behaviour:**
- **No preemption.** A batch handed to the host cannot be taken back, so the
  package sets `EnablePreemption=0`. A preemption request is answered once the
  node runs dry, and held work is bounded by a deadline instead.

**Setup pitfalls:**
- **Windows Update will replace the driver.** The device presents the host
  GPU's PCI IDs, so Windows Update offers that vendor's real driver. On this
  device the real driver bugchecks. `provision.ps1` turns driver searches off
  (`NoDriverUpdates`), both at `prepare` and at `install`. Keep them off in any
  guest that has network access.
- **The host must find DXVK and vkd3d-proton** (`AV_D3D_PATH`, below).
  Without them the host offers no Direct3D, and DWM crash-loops with
  `0x889800B2` ("the host answers no Direct3D 11 on this adapter").
- **The distribution's QEMU cannot load the device.** Its compiled-in module
  table has no entry for these types, so QEMU itself is rebuilt from the pinned
  source.
- **Guest RAM must be a shared memfd**
  (`-object memory-backend-memfd,...,share=on`). The host imports guest pages
  as dma-bufs, and the renderer process maps them.
- **Give the host window room.** `hostmem=8192M` is what runs real
  applications. At 256 MiB, context arenas run out.

## Requirements

**Host:**
- Linux, x86-64, with KVM. `/dev/udmabuf` should be available (the `udmabuf`
  module): dma-buf imports of guest memory and zero-copy scanout depend on it,
  and the device's capset reports only what the host proved it can do.
- Per API you want served:
  - a Vulkan loader and driver (Mesa RADV or ANV, NVIDIA, ...);
  - EGL with desktop OpenGL;
  - an OpenCL ICD (Mesa rusticl or a vendor's).

  An API the host lacks is simply not offered.
- clang and clang++ (C++23), clang-cl, lld (`lld-link`), `llvm-lib`,
  `llvm-rc`, `llvm-mt`, CMake 3.24 or later, Ninja, and Python 3.11 or later
  (standard library only).
- For QEMU: its own build dependencies (glib2, pixman), plus:
  - gtk3 and libepoxy for `-display gtk,gl=on`;
  - libslirp for `-nic user`;
  - PipeWire for audio.
- For Direct3D on the host: DXVK's and vkd3d-proton's build dependencies
  (meson, glslang). SDL3 is also needed at run time: DXVK's native build loads
  its window-system backend even when it shows no window.
- OVMF (edk2) UEFI firmware.

**Guest:**
- Windows 11 24H2 (26100) or later, x64, installed on UEFI without enrolled
  Secure Boot keys, with BitLocker off.
- For the automated flow: the QEMU guest agent from the virtio-win guest tools,
  set to start automatically, and PowerShell 5 or later (inbox).
- About 25 GB of disk.

## Building

Everything the build needs is in the repository, or is fetched into
`third_party/` pinned by hash.

```bash
git submodule update --init third_party/dxvk third_party/vkd3d-proton

tools/fetch_windows_kit.sh            # the WDK/SDK header and import-library subset (the licence forbids committing it)
tools/fetch_khronos_registries.py     # vk.xml, gl.xml, cl.xml and the Khronos headers
tools/fetch_qemu.sh                   # QEMU 11.1.1's release tarball

# 1. avgpu.sys, the guest DLLs and programs, and the host unit tests
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build

# 2. QEMU with virtio-gpu-av-pci, the executor library and av-renderer
cmake -S . -B build-qemu -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release -DAV_QEMU=ON
cmake --build build-qemu

# 3. The host's Direct3D: DXVK and vkd3d-proton, with tools/*-patches applied
tools/build_d3d_runtime.sh
```

Optional fetches, for the guest side of the flow:

```bash
tools/fetch_vulkan_runtime.py         # the Windows Vulkan loader and vulkaninfo.exe
tools/fetch_opencl_loader.py          # the Khronos OpenCL loader's source; the build then makes OpenCL.dll
tools/fetch_layers.py                 # the Windows DXVK and vkd3d-proton releases the ICD is held to
tools/fetch_cts.py                    # the Vulkan CTS, for --cts runs
tools/fetch_dxc.sh                    # Microsoft's DXC, for the D3D12 samples
```

What comes out:

| File | Built by | Goes |
|---|---|---|
| `build/driver/avgpu.sys` | `build`: `src/core` compiled a second time with `clang-cl /kernel` | the guest's driver package |
| `build/guest/avumd.dll`, `avvk.dll`, `avgl.dll`, `avcl.dll` | `build`: `src/icd` and `src/um` with `clang-cl` and no C runtime | the guest's driver package |
| `src/km/avgpu.inf`, `src/um/vulkan/avvk.json` | committed | the guest's driver package |
| `build/guest/OpenCL.dll` | `build`, once the loader is fetched | the guest's `C:\Windows\System32` |
| `build/guest32/avumd32.dll`, `avvk32.dll`, `avgl32.dll`, `avcl32.dll` | `build`: the same tree built again for x86 | the guest's driver package |
| `src/um/vulkan/avvk32.json` | committed | the guest's driver package |
| `build/guest32/OpenCL.dll` | `build`, once the loader is fetched | the guest's `C:\Windows\SysWOW64` |
| `build/guest32/*.exe` | `build` | the guest: the probes, samples and suite, as 32-bit programs |
| `build/guest/avtest.exe`, `avinspect.exe`, `av*probe.exe`, `av*samples.exe` | `build` | the guest, for testing and inspection |
| `build-qemu/qemu/qemu-system-x86_64` and its modules | `build-qemu` | the host, run with `QEMU_MODULE_DIR=build-qemu/qemu` |
| `build-qemu/libav_executor.so`, `build-qemu/av-renderer` | `build-qemu` | the host, named by `executor=` and `renderer=` |
| `build-qemu/avrecord` | `build-qemu` | the host: prints and replays wire recordings |
| `build-d3d/lib/` | `tools/build_d3d_runtime.sh` | the host, named by `AV_D3D_PATH` |

A core change that would not build in the guest kernel fails the build. That
covers a hosted header, a C runtime call, `new`, `throw` or `typeid`.

### The driver package

`cmake --build build --target drivers` builds only the Windows drivers: the
kernel driver, and the user-mode drivers for x64 and x86. `cmake --install
build` then installs the package as the guest's `C:\av` holds it:

```
build/av-drivers/
├── provision.ps1          the guest-side installer (sign, install, check, ...)
├── adapters.txt           this host's GPU names, which the sign stage gives each INF model
└── package/
    ├── avgpu.sys  avgpu.inf
    ├── avumd.dll  avvk.dll  avvk.json  avgl.dll  avcl.dll                  x64
    ├── avumd32.dll  avvk32.dll  avvk32.json  avgl32.dll  avcl32.dll        x86
    └── package.sha256     what the sign stage checks the package against
```

`--prefix DIR` installs it anywhere else. A build directory configured before
this existed has `/usr/local` cached as its prefix: configure it once with
`-DCMAKE_INSTALL_PREFIX=$PWD/build/av-drivers`. `cmake --install` installs
what is already built, so build `drivers` first. `adapters.txt` comes from `tools/host/list_gpus.py --adapter-names`
on the host doing the install. It is left out, with a warning, when
`vulkaninfo` fails, and the INF's models then take its static name. Copy the
tree to the guest's `C:\av` and run route 2 below from `provision.ps1`.

## Installing

There are three routes. The first is how the project tests itself. The second
is for a Windows VM you already have. The third updates a driver that is
already installed.

### 1. The runner (recommended)

`tools/guest/run_guest_suite.py` drives everything through the QEMU guest
agent. It boots a copy-on-write overlay of an image, pushes the package, signs
it, installs it, reboots, checks the adapter started on `avgpu`, and runs the
suite.

Prepare the image once. This is the one run that writes the image:

```bash
tools/guest/run_guest_suite.py --image /path/to/windows.qcow2 --prepare-image
```

`provision.ps1 -Stage prepare` then sets the image up:
- the test-signing certificate and test signing (one reboot);
- kernel memory dumps with automatic reboot, and verbose `setupapi.dev.log`;
- nothing that interrupts a run: no fast startup, hibernation, sleep,
  automatic updates, automatic maintenance, driver searches or boot recovery;
- a Defender exclusion for `C:\av`.

It is idempotent.

A test run:

```bash
AV_GUEST_IMAGE=/path/to/windows.qcow2 tools/guest/run_guest_suite.py --quick
```

A VM to use:

```bash
tools/guest/run_guest_suite.py --image /path/to/windows.qcow2 --work build-guest-render \
    --executor build-qemu/libav_executor.so --all-gpus --hostmem 8192 \
    --quick --keep --no-collect --display gtk --net --audio
```

`--display gtk` means `gtk,gl=on`. `--all-gpus` adds one device per host GPU,
the second render-only. `--net` is user-mode NAT on an e1000e, and `--audio` is
Intel HD Audio to PipeWire, both on Windows' inbox drivers. `--keep` leaves the
guest running until you press Enter.

> **The runner treats its overlay as scratch.** It deletes
> `<work>/overlay.qcow2` when it starts, and again when its QEMU exits,
> `--keep` or not. If you install software in a runner's guest that you want
> to keep, copy the overlay first (`cp --reflink=always`, with `vars.fd` beside
> it). From then on, relaunch QEMU yourself from its saved command line
> (`tr '\0' '\n' < /proc/<pid>/cmdline`) with the runner's environment
> (`QEMU_MODULE_DIR`, `AV_D3D_PATH`).

### 2. By hand, on an existing Windows 11 VM

**On the host,** give the VM the device. A complete command line:

```bash
QEMU_MODULE_DIR=build-qemu/qemu AV_D3D_PATH=$PWD/build-d3d/lib \
build-qemu/qemu/qemu-system-x86_64 \
  -machine q35,accel=kvm,memory-backend=ram \
  -object memory-backend-memfd,id=ram,size=8192M,share=on -m 8192M \
  -cpu host,hv-relaxed,hv-vapic,hv-spinlocks=0x1fff,hv-vpindex,hv-runtime,hv-synic,hv-stimer,hv-time,hv-frequencies,hv-tlbflush,hv-ipi,hv-crash \
  -smp 8,sockets=1,cores=8,threads=1 \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/edk2/x64/OVMF_CODE.4m.fd \
  -drive if=pflash,format=raw,file=vars.fd \
  -drive if=none,id=disk,format=qcow2,file=windows.qcow2 -device ide-hd,drive=disk,bus=ide.0 \
  -vga none \
  -device virtio-gpu-av-pci,id=gpu,host-gpu=auto,executor=build-qemu/libav_executor.so,max_outputs=1,vectors=11,hostmem=8192M \
  -display gtk,gl=on \
  -device qemu-xhci -device usb-tablet \
  -chardev file,id=debugcon,path=avgpu.log -device isa-debugcon,iobase=0x402,chardev=debugcon \
  -qmp unix:qmp.sock,server=on,wait=off
```

`vars.fd` is the VM's own writable copy of `OVMF_VARS.4m.fd`. For an existing
VM, use the variables file it already boots with.
`tools/host/list_gpus.py --qemu` prints one `-device` argument per host GPU,
bound by UUID, and `--libvirt` prints the same as a libvirt domain fragment.
Keep `-vga none`: another display adapter would take the desktop. The
`isa-debugcon` line gives you the driver's log.

**In the guest,** as Administrator:

1. Copy `build/av-drivers` from `cmake --install build` ([The driver
   package](#the-driver-package)) to `C:\av`, which puts everything below in
   place with its `package.sha256` and `adapters.txt`. By hand, copy the
   package into `C:\av\package`:
   - `avgpu.sys`, `avgpu.inf`, `avvk.dll`, `avvk.json`, `avgl.dll`,
     `avcl.dll`, `avumd.dll`;
   - the 32-bit drivers from `build/guest32`: `avvk32.dll`, `avgl32.dll`,
     `avcl32.dll` and `avumd32.dll`, with `src/um/vulkan/avvk32.json`. The
     INF names them all, and `pnputil` refuses a package missing a file it
     names;
   - `tools/guest/provision.ps1` into `C:\av`.
2. Sign the package:

   ```powershell
   powershell -ExecutionPolicy Bypass -File C:\av\provision.ps1 -Stage sign
   ```

   This names every present `virtio-gpu-av-pci` in the INF, makes and trusts
   the test certificate, catalogs and signs the package, and turns test signing
   on. Exit code 3010 means reboot, then continue.
3. Install:

   ```powershell
   powershell -ExecutionPolicy Bypass -File C:\av\provision.ps1 -Stage install -NoVerifier
   ```

   This turns driver updates off, enables user-mode submission and native
   fences, and runs `pnputil /add-driver ... /install`. Without `-NoVerifier`
   it also puts Driver Verifier's standard checks on `avgpu.sys`, which is
   right for testing and slow for use. Reboot.
4. Put the loaders in place:
   - the Vulkan loader: either run LunarG's Vulkan Runtime installer, or copy
     `third_party/vulkan-runtime/vulkan-1.dll` to `C:\Windows\System32`;
   - the OpenCL loader: copy `build/guest/OpenCL.dll` to `C:\Windows\System32`.
   - for 32-bit applications, the same again: LunarG's Vulkan Runtime installer
     also puts a 32-bit `vulkan-1.dll` in `C:\Windows\SysWOW64`, and
     `build/guest32/OpenCL.dll` goes there too.

   Windows ships none of them.
5. Check:

   ```powershell
   C:\av\provision.ps1 -Stage check
   ```

   This confirms every avgpu adapter started with no problem code.
   `-Stage adapters` shows what WMI and DXGI call it, and `-Stage modes` lists
   the display modes.

`prepare` is optional here. It additionally turns off everything that
interrupts an automated run, which you may not want on a VM you use.

### 3. Updating an installed driver

- **As an update would arrive, with no reboot.** Put the new files in
  `C:\av\package`, run `-Stage sign` (it signs `avgpu.sys` and the catalog),
  then `-Stage upgrade -Minutes 1`. That raises `DriverVer`'s build number by
  1, installs over the running driver, and checks the adapter now runs the new
  version. The count starts from the INF in `C:\av\package`, which each
  upgrade rewrites. Copying the repository's INF over it resets the count, and
  an upgrade to a version already installed keeps the old files without saying
  so: compare a file's hash in the driver store after installing.
- **A user-mode driver only.** Replace the DLL in the driver store directory
  below. The next process to load it gets the new one, with no reboot and no
  signature check, since user-mode DLLs are verified only at install.
- **Quickest, for a kernel-only change.** Sign as above, then replace
  `avgpu.sys` in the driver store and reboot. The driver store copy is
  `C:\Windows\System32\DriverStore\FileRepository\avgpu.inf_amd64_*\`; rename
  the old file, don't delete it. Windows loads the store's copy at the next
  start.

If the new driver bugchecks at boot, boot the same disk without the device
(drop `virtio-gpu-av-pci` and add `-vga std`) and put the old file back.

## Device parameters

`-device virtio-gpu-av-pci,<property>=<value>,...` takes these properties.

| Property | Default | Meaning |
|---|---|---|
| `host-gpu` | `auto` | The host GPU this adapter runs on: `auto` (the first Vulkan device) or a Vulkan device UUID from `tools/host/list_gpus.py`. A malformed or absent GPU fails to start, with the reason. |
| `executor` | none | The executor library, usually `build-qemu/libav_executor.so`. Without one the device is a plain virtio-gpu with no av capset: 2D only, no acceleration. `libav_echo_executor.so` is the test double the runner defaults to. |
| `renderer` | none | The path to `av-renderer`. The executor then runs in that process instead of QEMU, and a host driver crash costs the guest a device reset instead of the VM. |
| `modes` | empty | The display's resolutions: `WxH` between commas, the first preferred, e.g. `2560x1440,1920x1080,1000x600`. Empty means the host monitors' own modes. It can be changed while the guest runs (see [Resolutions](#resolutions)). |
| `display` | `on` | `off` makes a render-only adapter with no outputs, for a second host GPU. |
| `record` | none | A file to write a wire recording to: everything the executor received. |
| `record-limit` | `0` (256 MiB) | The recording's ceiling in bytes. |
| `record-kinds` | all but `blob_bytes` | Comma-separated: `setup`, `batch`, `reply`, `fence`, `fault`, `blob`, `blob_bytes`, `all`. |
| `record-wrap` | `off` | At the ceiling, wrap back to the file header instead of stopping. |

Stock virtio-gpu properties that matter here:

| Property | Use | Why |
|---|---|---|
| `hostmem` | `8192M` | The host window's size. `0` (virtio-gpu's default) means no window: every host-visible chunk is then guest RAM the driver allocates and the host imports (`docs/host-window.md`). |
| `max_outputs` | `1` | Display heads. The driver drives one. |
| `vectors` | `11` | MSI-X vectors: 3 for the device's own sources (control, cursor, configuration) and 8 the contexts' fence queues share. This matches the INF's `MessageNumberLimit`. |
| `xres`, `yres` | `1280`, `800` | The scanout size. The driver's standard list is clipped to it when the host offers no modes at all. |

The runner's own options (`run_guest_suite.py --help`) cover the same ground
for its VMs:
- `--host-gpu` and `--all-gpus`;
- `--executor` and `--renderer`;
- `--hostmem`, `--outputs`, `--memory` and `--cpus`;
- `--display` and `--vnc`;
- `--net` and `--audio`;
- `--record`, `--perfetto`, `--trace`, `--gdb` and `--hold`;
- `--drivers32`, where the 32-bit drivers are (`build/guest32` by default).

## Resolutions

The host decides which resolutions the guest's display offers, and any size
works, not only the standard ones. The rules:

- **`modes=` set:** exactly those, in that order, with the first preferred.
- **`modes=` empty (the default):** the modes of the monitors connected to the
  host, read from `/sys/class/drm/<card>-<connector>/modes`. Interlaced modes
  and those under 640x480 are left out, and the largest monitor's best mode is
  preferred.
- **No host monitor and no list:** the standard list, clipped to
  `xres`x`yres`.

On a running VM, over QMP:

```bash
tools/vm_resolution.py                        # what the display offers now
tools/vm_resolution.py 1920x1080              # exactly 1920x1080: Windows switches to it
tools/vm_resolution.py 1000x600 1920x1080     # both, 1000x600 preferred
tools/vm_resolution.py host                   # the host monitors' modes again
tools/vm_resolution.py --qmp path/to/qmp.sock --device gpu ...
```

A change raises a display event. The driver clears it, asks for the new list,
and replugs the monitor, so Windows reads it again. A list without the mode
Windows is showing makes it switch to the preferred mode, so offering one size
sets the resolution. Offered modes appear in Settings → Display like a real
monitor's.

What the driver does with the list:
- **Size limits.** It keeps what Windows can use: 100x100 and up, and what an
  EDID detailed timing can describe (up to 4095 per edge, 655.35 MHz). Anything
  else is left out, and the driver log says how many were taken.
- **Pixel format.** Source modes are A8R8G8B8, because `cdd.dll` lists only
  R8G8B8, A8R8G8B8, R5G6B5 and P8 modes. Any other format leaves Windows a
  single made-up 800x600 and a greyed-out resolution setting.
- **Saved configurations.** A configuration Windows saved for a mode the host
  no longer offers is refused before it is committed, so Windows falls back to
  the preferred mode rather than failing at boot.

## Environment variables

These are read by QEMU, the executor library and `av-renderer`.

| Variable | Effect |
|---|---|
| `QEMU_MODULE_DIR` | Where QEMU finds its modules: `build-qemu/qemu`. Required. |
| `AV_D3D_PATH` | The directory holding DXVK's and vkd3d-proton's native libraries (`build-d3d/lib`). Unset, the loader's own search is used, and without them the host serves no Direct3D. |
| `DXVK_LOG_PATH` | DXVK's own log directory (DXVK's variable; the runner sets it to the work directory). |
| `AV_PERFETTO=1` | Record every host function scope as a timeline event (`/dev/shm/av-events-*`) for `tools/perfetto_trace.py`. Also lets any process of the user ptrace QEMU for sampling. |
| `AV_TRACE_SCOPES=1` | Bind the host trace tables, so live thread state can be read, without timeline events. |
| `AV_TRACE_DEPTH=<n>` | How deep the timeline records (0 is everything; the defaults are 2 for the device and 6 for the executor). |
| `AV_TRACE_ONLY=<names>` | Record only these comma-separated `Class::method` scopes. |
| `AV_PTRACE=1` | Allow any process of the user to attach to QEMU, under Yama's `ptrace_scope` 1. |
| `AV_VK_SUBMIT_TIMING=1` | Log, once a second, the GPU's time spent in and between Vulkan submissions. |

`VKD3D_VULKAN_DEVICE` is set by the executor itself, per device, to pin
vkd3d-proton to the adapter's GPU. Don't set it yourself.

## What the driver package installs

`avgpu.inf` matches `PCI\VEN_1AF4&DEV_1050&SUBSYS_52561AF4`: the device on a
host GPU with no PCI function, such as llvmpipe. The `sign` stage adds one
model line for each present `PCI\VEN_xxxx&DEV_yyyy&SUBSYS_52561AF4`, named
after the host GPU, because the IDs differ from host to host. It installs into
the driver store and writes these values:

| Value | Where | Why |
|---|---|---|
| `UserModeDriverName` | adapter software key | Four `%13%` paths to `avumd.dll`, one per Direct3D version. dxgkrnl refuses the adapter without this value. A bare file name is looked for in System32, never in the driver store, and every process, DWM included, would fall back to WARP. |
| `UserModeDriverNameWow`, `VulkanDriverNameWow`, `OpenGLDriverNameWow`, `OpenGLVersionWow`, `OpenGLFlagsWow`, `OpenCLDriverNameWow` | adapter software key | The same six values for a 32-bit process, naming the `*32` drivers. dxgkrnl reads the Direct3D and OpenGL ones for a WoW64 caller, and the Vulkan and OpenCL loaders read theirs when `IsWow64Process` says so. They are `%13%` paths too: the driver store is exempt from WoW64's System32 redirection. |
| `VulkanDriverName` | adapter software key | `%13%\avvk.json`, how the Vulkan loader finds the ICD through this adapter |
| `OpenGLDriverName`, `OpenGLVersion`, `OpenGLFlags` | adapter software key | `avgl.dll`. Flags `1` makes opengl32 ask the ICD for pixel formats. |
| `OpenCLDriverName` | adapter software key | `avcl.dll`, as `REG_SZ`. The loader's fast path ignores a multi-string. |
| `Features\4`, `Features\37` | adapter key, and the global `GraphicsDrivers` key at install | Enable user-mode submission and native fences, which Windows ships staged off |
| `Scheduler\EnablePreemption = 0` | `GraphicsDrivers` | See [Limitations and nuances](#limitations-and-nuances). |
| `MSISupported`, `MessageNumberLimit = 11` | device hardware key | MSI-X, one vector per interrupt source |

At every start, the driver writes the host GPU's name to the device's
description.

## Debugging

**Driver log.** `avgpu.sys` writes `avgpu: ...` lines to I/O port 0x402.
With `-device isa-debugcon,iobase=0x402,chardev=...` they land in a host file
(the runner's `<work>/avgpu.log`), across reboots and up to the moment of a
bugcheck.

**Guest user-mode logs.** Each driver appends to
`C:\ProgramData\av\<module>-<pid>.log`, where the module is `avvk`, `avgl`,
`avcl` or `avumd`. The logs name each refused or failed call, shader and
kernel build errors, and the host's reason for a fault. An unhandled exception
writes `C:\ProgramData\av\crash-<module>-<pid>.txt`.

**Live state, without stopping anything.** Every function opens with
`AV_SCOPE("Class::method")`. Each binary keeps a table of which thread is in
which function, since when, and which lock it holds or waits for, and a reader
maps that table from outside ([`docs/trace.md`](docs/trace.md)):

| Binary | Where its table is | Reader |
|---|---|---|
| `avgpu.sys` | non-paged memory (the physical address is logged at `DriverEntry`) | `avinspect.exe kernel ...`, or QMP `pmemsave` on a wedged guest |
| the guest DLLs, `avtest.exe` | the section `Global\av-trace-<module>-<pid>` | `avinspect.exe` |
| QEMU's device, the executor | `/dev/shm/av-trace-{device,executor}-<pid>` and a state socket beside it | `tools/mcp/av_inspect.py` |

`tools/mcp/av_inspect.py` is an MCP server over all of it: guest and host
threads, driver and device state, crash reports and the driver log. Add it with
`claude mcp add av-inspect -- python3 tools/mcp/av_inspect.py`.

**Timelines.**
- `tools/perfetto_trace.py <qemu pid>` writes the host's scopes for
  ui.perfetto.dev (QEMU started with `AV_PERFETTO=1`, or the runner's
  `--perfetto`). With `--guest` it merges the guest's own scopes onto the same
  clock.
- `tools/frame_breakdown.py` cuts the result into frames and shows where each
  one's time went.
- `tools/profile_sample.sh` does all of the above for one sample.
- `tools/frame_pacing.py run` records one WPR trace of the guest - scheduling,
  CPU samples with stacks, DPCs and ISRs, dxgkrnl, DWM and the flip manager -
  under a Windows.Graphics.Capture workload, and accounts for every DWM frame:
  the capture rule, and where the render thread's time went in the slow ones
  ([`docs/frame-pacing.md`](docs/frame-pacing.md)).
- `tools/guest_cpu_profile.py <process>` names a guest process's hottest
  functions, the kernel's included.

**Wire recordings.** Record what crossed the wire, then print it or replay it
into an executor library, on this GPU or another one:

```bash
-device virtio-gpu-av-pci,...,record=/tmp/run.av
build-qemu/avrecord print /tmp/run.av
build-qemu/avrecord replay /tmp/run.av --executor build-qemu/libav_executor.so
```

The replay names every answer that differs. Records are flushed as they are
taken, so a QEMU that dies inside a host driver leaves the batch that killed it.

**Bugchecks.** QEMU with `hv-crash` reports the bugcheck code and parameters
through the Hyper-V crash MSRs, and the runner names it without a dump. The
runner collects minidumps, live kernel reports and, with `--memory-dump`,
`MEMORY.DMP`, with the matching `avgpu.pdb` in `<work>/symbols/`. When the
guest stops at a bugcheck (`-action panic=pause`),
`tools/guest/read_bugcheck.py` reads it out through QEMU's gdbstub.

**Talking to the guest.** `tools/guest/guest_shell.py --work <dir>` runs
PowerShell and programs and moves files over the guest agent:

```bash
tools/guest/guest_shell.py --work build-guest-render ps 'Get-PnpDevice -Class Display'
tools/guest/guest_shell.py --work build-guest-render push file 'C:\av\file'
tools/guest/guest_shell.py --work build-guest-render pull 'C:\ProgramData\av\avvk-1234.log' .
```

## Testing

A display miniport can be unit tested thoroughly on Linux, without booting
Windows, if it is built for that from the first commit. That is the point of
splitting `src/core`, which is portable, from `src/km`, which only translates.
[`docs/testing.md`](docs/testing.md) has the details.

```
 L5  in-guest integration      real Windows, real dxgkrnl        run_guest_suite.py
 L4  loopback protocol         guest core ↔ device core, one process
 L3  fuzzing                   libFuzzer + ASan/UBSan on every parser
 L2  DDI conformance harness   a fake dxgkrnl drives the real DDI table
 L1  unit tests                every function, every branch, invariants, garbage
```

```bash
ctest --test-dir build -LE tools       # the fast loop: unit tests under ASan+UBSan, design and symbol rules
ctest --test-dir build                 # plus the gate scripts' own tests
tools/gate.sh                          # the full verdict: all configurations, and the guest when AV_GUEST_IMAGE is set
```

| Configuration | Directory | What it checks |
|---|---|---|
| default | `build/` | unit tests, `design_rules`, `symbol_tests`, `scope_marks`, `generated_sources`, `protocol_ids`, `extension_policy`, `driver_image`, `self_contained` |
| coverage | `build-coverage/` (`-DAV_COVERAGE=ON`) | 100% of branches in `src/core`, `src/device`, `src/proto`, `src/guest` and `src/icd`; a line marked `AV_UNTESTABLE` is left out and named in the report |
| fuzz | `build-fuzz/` (`-DAV_FUZZ=ON`) | every libFuzzer target, 60 s each (`AV_FUZZ_SECONDS`) |
| qemu | `build-qemu/` (`-DAV_QEMU=ON`) | the device in real QEMU under qtest: the PCI function, realize failures, the virtio-gpu suites |
| guest | `build-guest*/` | the package installed under Driver Verifier; `avtest.exe`'s scenarios; the Vulkan, OpenGL, OpenCL and D3D probes and the Vulkan CTS subset once their runtimes are fetched |

The 32-bit build of every guest program is in `build/guest32`, for testing the
drivers a WoW64 process loads. One part of it is 64-bit only by nature:
`avtest.exe av`'s raw-wire Vulkan scenarios hold the host's dispatchable
handles (64-bit values) in Vulkan's pointer-sized handle types, which a 32-bit
process cannot. The Vulkan ICD itself maps those handles through its own
objects, so `avvk32.dll` is not affected.

Side-by-side reports run the same program on the host natively and in the
guest, and write the frames, their difference and the timings to HTML:
- `tools/vulkan_report.py`
- `tools/opengl_report.py`
- `tools/opencl_report.py`
- `tools/d3d11_report.py`
- `tools/d3d12_report.py`
- `tools/clgl_samples.py`

## Working on the code

Read these before writing code:
- [`CLAUDE.md`](CLAUDE.md): the C++ rules;
- [`docs/constitution.md`](docs/constitution.md): the principles;
- [`docs/testing.md`](docs/testing.md).

The short version:

- **Freestanding C++23.** There is no standard library, no exceptions and no
  RTTI. The guest side is kernel mode, and the device side decodes hostile
  input. Memory comes from `Platform::allocate` with an explicit pool, and
  objects live in `ObjectStorage<T>` or `ObjectPool<T>`.
- **Errors are values.** Every fallible call returns `[[nodiscard]]
  av::Status`, and a malformed frame or bad handle is a status, never an
  assert.
- **Everything is an object with state,** and dependencies are injected at
  construction. There are no static member functions, no stateless classes and
  no free-function dumping grounds. The only process-lifetime state is at the
  C entry points that have nowhere to put a `this`.
- **IRQL is part of the signature.** Each public method declares its ceiling
  with `platform_.require_irql_at_most(...)`. `MockPlatform` enforces the
  ceiling, and also fails any allocation, block or mutex at `Irql::dispatch`,
  so a violation fails a unit test instead of bugchecking a VM.
- **Every function opens with `AV_SCOPE`.** `tools/scope_marks.py --apply`
  writes the marks.
- **Never hand-edit `src/proto/generated/`.** Fix the generator
  (`tools/codegen/`) and regenerate. A protocol ID change is a wire change and
  updates `docs/protocol.md` in the same commit.

`tools/check_design.py` and `tools/check_symbol_tests.py` enforce the rules,
and `tools/gate.sh` runs every gate.

## Source layout

[`docs/architecture.md`](docs/architecture.md) is the code map: which binary
each directory builds, what each subsystem directory holds, which directories
may include which, and where new code goes.

```
src/core/                     the foundation every binary links: support/, trace/, log/, platform/
src/miniport/                 avgpu.sys's logic, portable: driver/, ddi/, adapter/, av_context/, memory/,
                              scheduling/, display/, vidpn/, present/, pci/, virtio/
src/km/                       Windows kernel shim, translation only: KernelPlatform, DriverEntry, avgpu.inf
src/proto/                    what crosses a boundary: wire/, client/, context/, submission/, resources/,
                              dma/, pages/, virtio_gpu/, escape/ (UM<->KM), renderer/, d3d/, vulkan/, generated/
src/device/                   the QEMU device: abi/, host/, module/, gpu/, context/, threading/, fence/,
                              memory/, control/, display/, present/, execution/, isolation/, renderer/,
                              recording/, replay/, feasibility/, shim/ (POSIX), qemu/ (C glue)
src/executor/                 libav_executor.so: sessions at the root; vulkan/, opengl/, opencl/, d3d/;
                              shim/ (the host runtimes it loads)
src/guest/                    kmt/ (D3DKMT, AdapterSearch), transport/, suite/ (avtest), probe/, inspect/
src/icd/                      the user-mode drivers' logic, portable, tested on Linux over MockKmt:
                              common/ (shared by 2+ drivers), vulkan/, opengl/, opencl/, d3d/
src/um/                       Windows user-mode shims: common/, vulkan/, opengl/, opencl/, d3d/, kmt/,
                              avtest/, inspect/, probe/, trace/, imports/, opencl_loader/, sample/
samples/                      d3d11, d3d12, opengl, opencl, vulkan, clgl: the same programs for host and guest
tests/unit/                   L1, mirroring src/;  scenarios/: the driver image through its DDI table
tests/harness/                MockPlatform, fake dxgkrnl/VidMm/VidSch/VidPn/virtio, MockHost, MockKmt, the rigs
tests/fuzz/  tests/corpus/    L3 targets and their seed corpora
tests/loopback/  tests/qemu/  L4 rigs; the device under qtest
tests/conformance/            the executor ABI's conformance suite, for any executor library
tools/                        gates, fetches, generators, reports, profilers
  codegen/                      the command generator and its profiles and registries
  guest/                        run_guest_suite.py, provision.ps1, guest_shell.py, read_bugcheck.py
    probes/                       wgc_probe.exe and etw_schema.exe, the guest's side of frame_pacing.py
  etw/                          ETL reading, the guest's event schemas, scheduling, DWM frames, symbols
  host/                         list_gpus.py, device captures, host-memory probes
  mcp/                          av_inspect.py
  re/                           symbol and binary fetching for reverse engineering
  dxvk-patches/  vkd3d-proton-patches/   what the host's Direct3D is patched with
cmake/                        the Windows kernel and user-mode cross toolchains (clang-cl, lld-link; user mode x64 or x86)
third_party/                  fetched and pinned: windows-kit, khronos, qemu, dxvk, vkd3d-proton, loaders, CTS
```

## Documentation

| | |
|---|---|
| [`docs/architecture.md`](docs/architecture.md) | The code map: binaries, layers, every source directory, where new code goes |
| [`docs/protocol.md`](docs/protocol.md) | The av wire protocol, normative: the capset, context setup, rings, batches, fence pages, the executor ABI, recordings |
| [`docs/ddi-sequences.md`](docs/ddi-sequences.md) | What each DDI does and answers, and the sequences dxgkrnl drives them in |
| [`docs/d3d-desktop.md`](docs/d3d-desktop.md) | The desktop on Direct3D: the design, from guest evidence, dxgkrnl's code and prior art |
| [`docs/host-window.md`](docs/host-window.md) | Host-visible memory: the hostmem window, and dedicated video memory |
| [`docs/renderer.md`](docs/renderer.md) | The renderer process: where the line is, what crosses it, recovery |
| [`docs/trace.md`](docs/trace.md) | Thread and state tracking, crash reports, the trace table format |
| [`docs/frame-pacing.md`](docs/frame-pacing.md) | Why a guest frame came late: the frame-pacing recording and report, and the WGC capture rule |
| [`docs/testing.md`](docs/testing.md) | The five test layers, and preparing and debugging a guest image |
| [`docs/kb/`](docs/kb/) | The WDDM 3.2 knowledge base: the architecture, the kernel-mode contract, memory, scheduling, display, user mode, the 3.x changes, Microsoft's GPU-PV and WARP drivers as case studies |
| [`docs/kb/12-re-cookbook.md`](docs/kb/12-re-cookbook.md) | How to keep reverse engineering: fetching binaries and PDBs, Ghidra, symbols |
| [`tools/codegen/README.md`](tools/codegen/README.md) | The generator: what it guarantees and what it does not |
