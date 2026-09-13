# Quickstart: building the transport, driving it over L4, running it in a guest

Read this as a run guide, not a design. The design is [`plan.md`](./plan.md),
[`research.md`](./research.md) and
[`contracts/client_transport.md`](./contracts/client_transport.md).

**What exists today.** The guest-side rvk state machine is complete and is
exercised only in the L4 loopback rig: `GuestContext`, `RingSubmitter`,
`FenceObserver` and `ContextRequestSequence` live in `src/core/rvk/`
(`src/core/rvk/guest_context.hpp`), and `LoopbackRig` drives a full
setup → submit → doorbell → execute → fence → complete round trip in one Linux
process (`tests/loopback/test_loopback_context.cpp:118`). The device runs all
three sub-APIs (042) and the wire is generated here (063).

**What does not exist yet.** Every file this feature adds is absent. Confirmed
by `ls`:

```bash
ls src/proto/client_transport.hpp        # the interface the generated client is handed — absent
ls src/guest/transport/                  # KmtTransport, TransportRing, ReplyReader, TransportFence — absent
ls tests/fuzz/fuzz_reply_reader.cpp      # the reply/fence-page fuzz target — absent
```

Also absent: `src/proto/context/rvk_context_record.*`, the miniport half
(`RvkContextBinding`, `ContextBlobPublisher`, `SubmissionReader`,
`FenceInterruptRoute` in `src/core/rvk/` and `src/core/adapter/`), the emitter's
writer-lending change under `tools/codegen/rvk_codegen/emit/`, and the new
guest scenario in `src/guest/suite/`. The class list is `plan.md`, "Classes this
feature introduces". Every command below that names one of these says so where
it names it.

## 1. Prerequisites

The Khronos registries and the Windows kit, once per machine:

```bash
tools/fetch_khronos_registries.py     # vk.xml, gl.xml, cl.xml + headers, by hash
tools/fetch_windows_kit.sh            # the configure refuses without it
```

The registries matter only for the emitter change (step 4) and the
generated-tree gate; the transport core and its unit tests build without them.
The Windows kit is required to configure at all — the transport reads
`third_party/windows-kit/include/wdk-shared/d3dkmddi.h` for the DDI structs it
threads (`DXGKARG_SUBMITCOMMANDVIRTUAL`, `research.md` §2).

L4 (steps 2–4) needs no guest. L5 (step 5) needs the libvirt `win11` **session**
domain shut off, because a run opens the domain's qcow2 and the domain holds its
lock while it is up:

```bash
virsh --connect qemu:///session list --all        # win11 must read "shut off"
virsh --connect qemu:///session domblklist win11   # the qcow2 the run wants
```

`virsh shutdown` is not enough while fast startup is on; shut down from the guest
agent or turn fast startup off first with `powercfg /h off`
(`docs/testing.md`, "Preparing an image", step 1).

## 2. Build

The default configuration builds the transport core and every unit test on
Linux, under ASan+UBSan, the way `src/core` builds:

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build
ctest --test-dir build -E tools_tests -j"$(nproc)"     # the fast loop, ~5 s
```

`-E tools_tests` is the fast loop; `tools_tests` alone is ~9 min and makes a full
run look hung (memory: fast-test-loop). The transport core is portable core with
no STL, no exceptions, no heap — it compiles where `GuestContext` compiles
(`research.md` §6), so no new configuration flag is needed to build it.

New targets this feature adds to the default build, none of which exist yet:

- the transport core's unit tests, one `tests/unit/test_*.cpp` per class in
  `src/guest/transport/` and `src/proto/`, picked up by the unit-test glob
  (`CMakeLists.txt:259`) with no CMake edit;
- the L4 transport round trip, a `tests/loopback/test_*.cpp`, picked up by the
  same glob;
- `design_rules` and `symbol_tests` cover the new classes automatically.

The generated Vulkan and OpenCL clients are **not** built by this configuration.
`rvktest`'s no-CRT toolchain has no `<cstdint>`/`<cstring>` and no API header
(`research.md` §6), and the ICDs that host the generated client are 045/052.
043's build proves the transport, not the generated client (step 4 is honest
about where that compile does and does not happen).

## 3. Drive it over L4

The loopback rig is the round trip SC-001 measures. Today a test drives it
without any transport class, straight against `GuestContext`
(`tests/loopback/test_loopback_context.cpp:118`,
`loopback_context.sets_up_runs_batches_with_a_reply_and_tears_down`):

1. `LoopbackRig rig;` builds the guest's `MockPlatform`, the device core over it,
   and the `LoopbackDriver` between them (`loopback_rig.hpp:52`).
2. `rig.read_capset()` reads the device's capset; `rig.establish(0u)` sets a
   context up — allocate ring/fence/reply, `create_context`, three `guest_blob`s,
   `setup` — and drives every request to the device
   (`loopback_rig.cpp:41`).
3. The test writes a batch into the mapped ring with `GuestRing::write_batch`,
   submits an `RVKS` descriptor with `rig.submit(index, fence, begin, end)`
   (`loopback_rig.cpp:57`), and rings the doorbell.
4. `rig.driver().serve_until(context, line, fence)` serves each fence interrupt
   as the DPC would until the fence completes (`loopback_driver.hpp:29`).
5. The test decodes the answer out of the context's reply block with
   `ReplyHeader` and compares the bytes
   (`test_loopback_context.cpp:139`).

The transport slots in **above** `GuestContext`, as the object the generated
client holds. The rig gains a second `ClientTransport` implementation beside
`KmtTransport` — the contract names it "the loopback rig's" transport
(`contracts/client_transport.md` heading) — over the same `GuestContext` the rig
already owns. A new `tests/loopback/test_*.cpp` then drives the round trip
through the transport's own members rather than the rig's helpers:

- `writer = t.request_writer()` hands out the next free span of the mapped ring
  (contract A1); the test encodes one command into it.
- `t.send_async(id, writer)` frames a batch on the ring and advances the tail
  (contract C); `t.send_sync(id, writer, reply)` closes the batch, submits the
  `RVKS`, and waits on the context's fence, which the rig's served interrupt
  completes.
- the test reads the answer through the `WireReader` the transport pointed at the
  reply slot, and asserts `TransportResult::Ok`.

A pass is what the existing round trip asserts: `completed()` reaches the
submitted fence, `!faulted()`, the executor ran the batch, and the reply's
`fence_id`, `size` and bytes match what was sent
(`test_loopback_context.cpp:133-141`). SC-001's other cases — a batch, a command
stream, an answer too large for the reply slot, a faulted context, a hostile
mapping and a full ring — are the rig's existing hostile-device and backpressure
tests (`test_loopback_context.cpp:143`, `test_loopback_hostile`) re-expressed
through the transport, each returning not-`Ok` where the contract says so
(contract B3, C).

```bash
ctest --test-dir build -R test_loopback -j"$(nproc)"   # runs the L4 suite
```

The exact ctest name of the new file is whatever `tasks.md` names it; the glob
registers it by basename, no CMake edit (`CMakeLists.txt:259`). `plan.md` and the
contract do not name the loopback transport class or the test file — see
"What the design does not pin down".

## 4. The generated client over the transport

The emitter change (`research.md` §4, option A) makes the generated client ask
the transport for a writer and hand it back to `send_*`, so the fixed-capacity
`wire_*` types are enough and the overflow check lands in one place (contract B1).
The change is in `tools/codegen/rvk_codegen/emit/` (the client dispatch is
`client_dispatch.py`); after it, regenerate and check as 063 documents:

```bash
python3 -m rvk_codegen --xml third_party/khronos/vk.xml \
    --profile tools/codegen/profiles/vulkan.toml \
    --ids tools/codegen/profiles/vulkan_ids.toml \
    --header third_party/khronos/include/vulkan/vulkan_core.h \
    --out src/proto/generated/vulkan
ctest --test-dir build -R generated_sources        # re-runs every generator and diffs
```

`generated_sources` fails naming the file that differs and skips — saying why —
when the registries are not fetched (063 quickstart, "Checking").

A tiny program that encodes `vkCreateInstance` onto a transport looks like this,
and it is the shape a 045 ICD test will take, not a target 043 builds:

```cpp
KmtTransport t;                          // or the loopback rig's transport in a test
t.initialize(/* adapter LUID, sub-API vulkan */);
auto writer = t.request_writer();        // WireWriter over one ring slot (contract A1)
av::vkproto::encode_vkCreateInstance(writer, &create_info, nullptr, &instance);
WireReader reply;
if (t.send_sync(id_of_vkCreateInstance, writer, reply) != av::TransportResult::Ok)
    return /* dropped frame or too-large answer — a faulted context (contract A3, B3) */;
```

**Be honest about what does not build in 043.** There is no CMake target here
that compiles the generated Vulkan or OpenCL client against the transport: the
generated sources are excluded from the no-CRT build (`research.md` §6) and the
ICDs that give them a CRT home are 045 and 052. 043's deliverable is that the
emitter change *lets* them compile and link — the C++ the emitter now writes
matches the `ClientTransport` members (contract A) — proven by the emitter's own
tests and `generated_sources`, not by a linked binary. The guest round trip in
step 5 uses a hand-written frame, not the generated client (`research.md` §6, §7).

OpenGL is left compiling against the contract, not linking: it additionally needs
`gl_helpers.h`, nine length helpers, `rvk_glGetError` and `mint_sync_token`, all
050's to write (`research.md` §4, "What stays other features' to write"). It also
needs `ClientBlobArena` (`t.blobs()`, contract A5), which 043 leaves as a
follow-task — one `KmtBlobArena` added when the OpenGL write path first calls it,
per constitution VIII (`plan.md`, after the class table).

## 5. In a real guest

SC-004 adds a transport round trip to the in-guest suite. The suite is
`src/guest/`: `GuestSuite::run` runs each scenario in order against the one
adapter the search finds (`src/guest/suite/guest_suite.cpp:26`), and
`GuestReport::finish` prints `rvktest: N passed, M failed`
(`src/guest/suite/guest_report.cpp:58-64`). The new scenario is a
`*_scenario.{cpp,hpp}` under `src/guest/suite/`, constructed and run beside the
others in `GuestSuite` (`guest_suite.hpp:44`), modelled on `SubmissionScenario`,
which chains D3DKMT steps through `ScenarioSteps::check`
(`src/guest/suite/submission_scenario.cpp:23`). It creates a context per sub-API
through the transport, submits a hand-written frame, waits, and completes.

The runner drives it unchanged. `run_guest_suite.py` runs `rvktest.exe`, reads
the summary line with `SUMMARY = re.compile(rb"^rvktest: (\d+) passed, (\d+)
failed$")` (`tools/guest/run_guest_suite.py:47`), and fails the step if the
failed count or exit code is non-zero (`run_guest_suite.py:550-560`):

```bash
AV_GUEST_IMAGE=<the qcow2 from domblklist> tools/guest/run_guest_suite.py \
    --image <that qcow2> --work build-guest
```

The pass line to read is the runner's `report ok: N passed`
(`run_guest_suite.py:560`), which prints only when `rvktest.exe` reported zero
failures; the full per-step verdict is `build-guest/report.txt` and
`build-guest/verdict.txt` (`docs/testing.md`, "Debugging a run").

The same path over a *live host executor* is feature 044, not 043: until a
guest batch reaches the host executor, a submission completes from the driver's
own fence signalling, not from real host work (`docs/testing.md`, L5 `tdr`
bullet; memory: 040 guest-power state).

## 6. The gates

`tools/gate.sh` runs every configuration in turn and stops at the first failure
(`tools/gate.sh:16-22`):

| Configuration | Directory | Roughly | What it adds for 043 |
|---|---|---|---|
| default | `build/` | `-E tools_tests` ~5 s; `tools_tests` alone ~9 min | the transport core's unit tests under ASan+UBSan, the L4 transport round trip, `design_rules`, `symbol_tests` |
| coverage | `build-coverage/` | a full rebuild plus the run | 100% branch coverage of the transport core (SC-002) |
| fuzz | `build-fuzz/` | 60 s per target, 20-way parallel — minutes | `fuzz_reply_reader` over the reply-slot and fence-page readers (SC-002) |
| qemu | `build-qemu/` | first build compiles QEMU; then minutes | `qemu_smoke`, `qemu_virtio_gpu` — the device the miniport half drives |

```bash
tools/gate.sh                                  # all four Linux configurations, then the guest
```

**100% branch coverage of the transport core (SC-002).** The coverage gate is a
floor, not the goal: every branch of every function 043 adds to `src/core`,
`src/proto` and the transport is covered or marked `AV_UNTESTABLE`
(spec, "Test requirements"). A test must fail if a comparison is flipped or a
line deleted (`CLAUDE.md`, "Tests").

**The fuzz target (SC-002).** `tests/fuzz/fuzz_reply_reader.cpp` drives the reply
and fence-page readers over hostile bytes — a size past the slot, a completed
fence that goes backwards or exceeds what was submitted, a required length beyond
capacity, a fault flag that clears (spec, "Edge Cases"). The glob registers it as
`fuzz_reply_reader_smoke` with no CMake edit (`CMakeLists.txt:384`), and configure
is a `FATAL_ERROR` in every configuration until a non-empty seed corpus exists at
`tests/corpus/fuzz_reply_reader/` (`cmake/check_fuzz_corpora.cmake`, as with the
existing `tests/corpus/fuzz_submission/`). The required outcome is a fault, never
a crash: the reader returns a status and never reads outside its mapping
(constitution V; spec FR-005).

**L5 is a merge gate.** `gate.sh:40-48` runs the guest when `AV_GUEST_IMAGE`
names a prepared image and says it skipped otherwise; from feature 037 on a
skipped guest is not a pass (`docs/testing.md`, "The gate"; memory: 037/038
state).

## 7. Troubleshooting

These are the failures this design produces, and what each one means.

| Symptom | What it means |
|---|---|
| `D3DKMTCreateContextVirtual` returns `STATUS_INVALID_PARAMETER` and no context is created | the private-data record was rejected: bad magic, an out-of-range ring order, or two of the ring/fence/reply blob ids equal. The record carries the RVKC magic `0x434B5652` and a ring order in 12–24 (`src/proto/context/context_setup.hpp:44-45`), and RVKC needs three *distinct* blob ids with no offset (`research.md` §1). Until the record is accepted the context table refuses any private data at all (`src/core/scheduling/context_table.cpp:21`). Check the record's magic and its three ids before blaming the ring. |
| `send_sync` returns not-`Ok` on a command that looks well-formed | the writer overflowed the ring slot: a field that did not fit latched the writer's status, and `send_*` refuses a writer whose `status()` is not `ok` — a dropped frame that faults the context, never a truncated command (contract B1, A2). This is the check Vulkan and OpenCL never emitted (`research.md` §4). A frame that genuinely does not fit one slot is a fault by design; split the work, do not grow the slot. |
| a `send_sync` wait never wakes and the thread sleeps forever | the fence-page interrupt is not reaching the context. The driver completes the submission fence only when the device's fence page says the batch finished, read in the DPC through `FenceObserver` off the context's fence-queue MSI-X vector (`research.md` §5). If `FenceInterruptRoute` did not route that vector to the context, the monitored-fence wait (`D3DKMTWaitForSynchronizationObjectFromCpu`) has nothing to wake it — a real sleep with no waker (contract C, SC-003). In the L4 rig the analogue is forgetting to serve the interrupt: `serve_until` must run on the right `line` (`tests/harness/loopback/loopback_driver.hpp:29`). |
| a sync reply reads empty although the command succeeded | the reply slot was never bound, or the wrong blob id was named as the reply resource. `ReplyReader` refuses a slot no device wrote (`plan.md`, class table) and `send_sync` returns not-`Ok` rather than a reader over nothing (contract B3). The reply resource is the third of the three blob ids in the setup record; a context that sends a reply-flagged command with reply resource 0 gets no slot to write into (`src/proto/context/context_setup.hpp:29`). |
| a context answers not-`Ok` on every send after one failure | the page faulted and the link is disconnected — `ContextLoss` seen from user mode (contract C; spec US3). A faulted fence page becomes `DXGK_INTERRUPT_DMA_FAULTED` for the owning context; recovery is destroy and a fresh setup (`research.md` §5). This is the intended terminal state, not a transient error to retry in place. |

## What the design does not pin down

Turned to notes rather than commands, because `spec.md`, `plan.md` and the
contract do not say enough:

1. **The loopback transport class and its test file.** The contract names "the
   loopback rig's" transport as the second `ClientTransport` implementation
   (contract heading) but gives it no class name, and `plan.md`'s project
   structure says only "the rig gains a `Kmt`-driven round trip" with no test
   filename. The `-R test_loopback` command in step 3 runs whatever `tasks.md`
   names; the exact ctest id is not derivable yet.
2. **The guest scenario's name and step count.** SC-004 says a scenario "creates
   a context per sub-API through the transport, submits, waits and completes",
   and `plan.md` says it lives in `src/guest/suite/`, but neither names the
   scenario or how many steps it reports — so the expected `N passed` count in
   step 5 cannot be stated in advance.
3. **How the guest scenario reaches the transport's D3DKMT calls.** `KmtTransport`
   needs `D3DKMTLock2` and `D3DKMTWaitForSynchronizationObjectFromCpu`; the
   in-guest program reaches D3DKMT only through `GdiKmt` over the `Kmt` contract
   (`docs/testing.md`, L5 "As built"). Whether those two calls are already on
   `Kmt`/`GdiKmt` or the scenario adds them is not settled in the plan, and the
   generated client explicitly is not the vehicle (`research.md` §6).
4. **MockKmt scripting for US3.** The spec's fault, TDR and device-lost scenarios
   say `MockKmt` "scripts a faulted fence page, a device-lost status ... and a
   doorbell disconnect", but the harness additions to `tests/harness/guest/` that
   provide those hooks are not named in the plan, so the L1 fault tests cannot be
   spelled as commands here.
</content>
</invoke>
