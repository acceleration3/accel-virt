# Feature Specification: In-guest integration suite

**Feature Branch**: `035-guest-integration-suite`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 035 · **Status**: `implement` · **Depends on**: 034

Driver Verifier, D3DKMT-level tests mirroring the loopback suite, and the
application suite. Driven from Linux over the QEMU guest agent with a snapshot
before every install.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### Design rules — binding on this feature

Constitution Principle III and [`CLAUDE.md`](../../CLAUDE.md). Restated here because it is checked at review, not assumed:

- **Every unit of behaviour is a class that owns state.** No static member functions. No stateless class used as a namespace. No free function outside the two ABI boundaries (`DxgkDdi*` entry points in `src/core/ddi/`, and `extern "C"` entry points), each one line with no logic.
- If functions thread the same handle, buffer, bounds or platform through their parameters, **that parameter is a member**. Make it a class and inject its dependencies at construction.
- Name the classes this feature adds before writing them, and say which kind each is: an **object class** (one instance per real thing) or a **system class** (a long-lived service owning a collection).
- Dependencies injected at construction. No global, no singleton, no file-scope state.
- Data members private, trailing underscore. Classes `PascalCase`, methods and locals `snake_case`. No `Manager`, `Helper`, `Utils`, `Handler`, `Impl`, `process()`, `handle()`.
- One class per header, named for it. A header declares; it does not define. One responsibility per class, statable without an "and".
- Errors are `av::Status` values. No exceptions, no RTTI, no C++ standard library, no `new`/`delete` (placement new only in `ObjectStorage`). Cleanup is RAII via `ScopeExit`, never repeated per branch.
- Fallible construction is two-phase: trivial constructor, `initialize(...)` returning `Status`.
- Every public method declares its IRQL ceiling via `platform_.require_irql_at_most(...)`.

### Classes this feature introduces

<!-- Fill this in during /speckit-plan, before writing code. One row per class; if a row has no state, it is not a class. -->

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `TextBuffer` (moved to core/support, extended) | object | destination, capacity, length | Builds one line of text in a fixed buffer, now with hexadecimal |
| `Kmt` | interface | - | The D3DKMT calls the in-guest suite makes, in core's types: `GdiKmt` in the guest, `MockKmt` in tests |
| `PciIdentity` | object | vendor, device, subsystem vendor, subsystem | The PCI identity an adapter reports |
| `AdapterSearch` | object | kmt, adapters enumerated, the one chosen | Finds the virtio-gpu-rvk adapter among those enumerated |
| `GuestSession` | object | kmt, adapter, device, allocations, paging queue, context, mappings, first teardown failure | Holds the kernel objects one scenario creates, destroying them in reverse |
| `GuestReport` | object | text, passed, failed | Records each step's outcome as a line the host parses |
| `ClientDescriptor` | object | encoded descriptor bytes, page count | Encodes the descriptor of one client allocation the suite asks for |
| `ScenarioSteps` | object | kmt, report, scenario name | Records one scenario's steps with the NTSTATUS behind each |
| `AdapterScenario` | object | kmt, search | The adapter is found and reports WDDM 3.2 |
| `AllocationScenario` | object | kmt, session | Allocations are made from a valid descriptor and refused from a hostile one |
| `GpuMappingScenario` | object | kmt, session | An allocation is made resident and mapped at a GPU virtual address |
| `SubmissionScenario` | object | kmt, session | A command buffer is submitted on a context and its completion awaited |
| `GuestSuite` | system | kmt, report, the scenarios | Runs every scenario in order against one adapter |
| `GdiKmt` (src/um, guest only) | object | the last NTSTATUS, the completion event | Translates `Kmt` calls into gdi32's D3DKMT functions |
| `GuestProgram` (src/um, guest only) | object | kmt, report, suite, console handle | Runs the suite and writes its report to standard output |
| `MockKmt` (harness) | object | scripted handles, failures, hostile answers, journal | A D3DKMT that answers what a test scripts |

### Test requirements

Constitution Principle III applies in full. Concretely, this feature is not done until:

- every branch of every function it adds to `src/core`, `src/device` or `src/proto` is covered, or marked `AV_UNTESTABLE` with a justification;
- each stateful module it adds exports an invariant function that tests call after **every** mutation;
- every function it adds that reads bytes from outside its own address space has a hostile-input table test and a libFuzzer target with a committed corpus;
- the allocation-failure sweep passes: with each allocation site failed in turn, no crash and no leak;
- IRQL annotations are present and the mock's IRQL checker is satisfied;
- `tools/check_symbol_tests.py` reports no untested exported symbol.

### Reference material

Knowledge base:

- [`docs/kb/11-implementation-plan.md`](../../docs/kb/11-implementation-plan.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

<!-- Written by /speckit-specify or by hand. The autogen block above is regenerated by tools/gen_specs.py; everything below is preserved. -->

Users: the driver's developers, who need one command that takes a freshly built
`rvkgpu.sys` into a real Windows guest on the patched QEMU, with Driver Verifier
on, and brings back a verdict and the driver's log - without ever damaging the
image they start from.

### User Story 1 - One command from build to verdict (Priority: P1)

**Independent Test**: `tools/guest/run_guest_suite.py` against a fake QEMU and
a fake guest agent (tests/tools) runs every step, reports the first failing
step by name, removes its overlay and never writes the base image.

**Acceptance Scenarios**:

1. **Given** a prepared Windows image, **When** the suite runs, **Then** it
   boots a copy-on-write overlay of it on the patched QEMU with
   `virtio-gpu-rvk-pci`, the guest agent's channel and the driver's debug port,
   and waits for the agent.
2. **Given** a booted guest, **When** the package is pushed, **Then** the guest
   test-signs it (a self-signed certificate, a catalog, test signing on),
   installs it with Driver Verifier's standard checks on `rvkgpu.sys`,
   reboots as each step asks, and checks the adapter started with no problem
   code.
3. **Given** an installed driver, **When** `rvktest.exe` runs, **Then** its
   report and exit code, the verifier's settings and the debug-port log are
   written to the work directory, the guest is powered off, the overlay is
   removed, and the command exits non-zero on any failure.
4. **Given** a step that times out or a guest that never answers, **Then** QEMU
   is killed, the step is named, and the overlay is removed.

### User Story 2 - D3DKMT tests mirroring the loopback suite (Priority: P1)

**Independent Test**: the scenarios run on Linux against `MockKmt` - scripted
answers, failures at every call, hostile values - with every branch covered;
the cross-built `rvktest.exe` imports only gdi32, kernel32 and ntdll.

**Acceptance Scenarios**:

1. **Given** a guest with the driver, **When** the suite runs, **Then** it finds
   the adapter by PCI identity `1af4:1050` subsystem `1af4:5256` and checks it
   reports WDDM 3.2.
2. **Given** a device on it, **When** an allocation is created from a valid
   client descriptor, **Then** it succeeds; from a descriptor with a bad magic,
   **Then** it is refused.
3. **Given** an allocation and a paging queue, **When** it is made resident and
   mapped, **Then** a GPU virtual address comes back and the paging fence is
   reached.
4. **Given** a context, **When** a command buffer at a mapped address is
   submitted and a CPU notification is signalled behind it, **Then** the
   notification arrives before the timeout.
5. Every step is reported; objects are destroyed in reverse whatever failed.

### User Story 3 - A driver package the guest can install (Priority: P1)

**Independent Test**: tests/tools checks `rvkgpu.inf` names the driver image
the build produces, matches the device's hardware id and asks for MSI.

### Edge Cases

- No adapter matches: the suite reports it and every later scenario fails
  without calling D3DKMT with a zero handle.
- More adapters than the search holds: the extra are ignored and closed.
- The guest agent answers garbage, drops the connection mid-command, or never
  syncs: the runner fails the step, it does not hang.
- A reboot the guest never comes back from: the timeout names the step.

## Requirements *(mandatory)*

- **FR-001**: The base image is only ever opened as the backing file of a
  per-run overlay; the overlay is deleted at the end of every run.
- **FR-002**: The runner's every wait has a timeout and kills QEMU on expiry.
- **FR-003**: `src/guest` follows constitution III and IV like `src/core`: it
  joins the design, symbol, coverage and self-containment gates. `src/um`
  is translation only and, like `src/km`, is built only for the guest.
- **FR-004**: The live suite runs in `tools/gate.sh` when `AV_GUEST_IMAGE` names
  a prepared image; without it the gate says it skipped the guest.
- **FR-005**: Nothing is installed in the guest except the driver package, its
  test certificate and the verifier setting.

## Success Criteria *(mandatory)*

- **SC-001**: `rvktest.exe` builds in the default configuration and passes an
  image check; its logic is 100% branch-covered on Linux.
- **SC-002**: The runner's tests cover every step, timeout and hostile agent
  answer.
- **SC-003**: With a prepared image, one command yields a verdict.

## Assumptions

- A prepared image is a Windows 10 or 11 x64 qcow2 on UEFI without Secure Boot,
  with the QEMU guest agent installed and PowerShell 5 or later. Preparing one
  is documented, not automated: it is a Windows installation.

## Out of Scope

- The application suite (vkd3d-proton, DXVK samples): it needs the Vulkan ICD
  (M4) and presentation (M5).
- HLK / WHLK.
- rvk contexts reached from D3DKMT: the driver's submission path does not use
  them until M3; the scenarios cover the path the driver has.
