# Contract: the transport NOP

Command id `0x00000001` (API 0, the transport's own; `docs/protocol.md`,
"Command stream"). Body: empty. A body is refused as malformed framing.

- The device's batch executor answers it itself and never calls the executor
  library, whichever library is loaded.
- With the reply flag, the answer is an empty reply (`RVKY` header, size 0) for
  the batch's fence; without it, nothing is written.
- A NOP on a faulted context is not run: the context runs nothing once faulted.
- It is the round trip a guest times as a no-op's latency (SC-002).

# Contract: `rvktest.exe rvk`

Runs `RvkSuite` against the first rvkgpu adapter: transport (NOP and latency),
Vulkan, OpenCL, OpenGL, isolation, in that order; one report line per step as the
suite writes today; exit code the failure count. A sub-API the capset does not
offer reports its scenario `skipped` by name, not failed (spec edge case).
