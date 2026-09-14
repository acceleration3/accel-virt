# Data model: rvk contexts in a real guest

## Guest scenarios (object classes, `src/guest/suite/`)

Each takes `(Kmt &, GuestReport &, const AdapterSearch &)`, owns a
`GuestConnection` and the `KmtTransport`s it opens for one run, reports through
`ScenarioSteps`, and tears down in reverse whatever failed (FR-003).

| Scenario | Sub-API | Steps | Judged by |
|---|---|---|---|
| `TransportScenario` | Vulkan | adapter, connection, initialize, nop, latency | NOP answered with an empty reply; latency reported in microseconds, never judged |
| `VulkanScenario` | Vulkan | adapter, connection, initialize, instance, physical_device, device, buffer, memory, bind, destroy | every call `VK_SUCCESS`; at least one physical device, its count reported; a no-op after the unanswered destroys |
| `OpenClScenario` | OpenCL | adapter, connection, initialize, platform, device, context, queue, buffers, program, kernel, run, read, sum, release | `c[i] == a[i] + b[i]` for every i, the matches reported; every release `CL_SUCCESS` |
| `OpenGlScenario` | OpenGL | initialize, nop, teardown | the host opened the context (setup established) |
| `IsolationScenario` | Vulkan ×3, one device | adapter, connection, initialize, healthy_before, fault, faulted_send, healthy_after, relink | the faulted context's next send fails; the healthy one's succeed; a context made afterwards answers a no-op |

Every rvk scenario opens its context through `ScenarioTransport` (the
connection and one transport, with the adapter, connection and initialize
steps), and `KmtTransport::nop` is the no-op round trip they judge.

Which host GPU ran the work is the runner's to record (the device's `host-gpu`
and the capset's identity in `verdict.txt`); the guest has no channel for the
capset's UUID today, and adding one for a test alone would be speculative.

## Device

- `TransportCommand` (object): constructed per batch with the reply writer;
  `run(const CommandHeader &, body)` answers NOP, and refuses any other API-0 id as
  `RVK_UNKNOWN` (skipped, as an unknown id is today).
- `ClHandleRegistry` (system, executor): `made(kind, pointer) -> wire`,
  `known(kind, wire) -> pointer or null`, `forget(kind, wire)`; bounded like
  Vulkan's registry, refusing past its capacity as device loss.
