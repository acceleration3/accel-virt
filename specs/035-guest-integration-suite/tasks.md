---
description: "Task list for feature 035: in-guest integration suite"
---

# Tasks: In-guest integration suite

Tests come first in every task and must fail before the code they cover exists.

## Phase 1: Building for the guest's user mode

- [X] T001 Kit: the `um` header closure in tools/fetch_windows_kit.sh, the overlay root, VERSION subset line, CMake check; tests/tools overlay test
- [X] T002 Move `TextBuffer` to src/core/support; `append_hex`; tests
- [X] T003 cmake/windows_user_{toolchain,flags}.cmake, src/um CMake project with `.def` import libraries; `rvktest` external project; tools/check_guest_image.py and `guest_image` test (console subsystem, imports only gdi32/kernel32/ntdll)

**Checkpoint**: commit.

## Phase 2: The suite's logic

- [X] T004 [US2] Tests then `PciIdentity`, `Kmt` in src/guest/kmt/; `MockKmt` in tests/harness/guest/; gates scan src/guest
- [X] T005 [US2] Tests then `GuestReport` in src/guest/suite/
- [X] T006 [US2] Tests then `AdapterSearch`, `GuestSession` in src/guest/suite/
- [X] T007 [US2] Tests then `ClientDescriptor`, `ScenarioSteps`, the four scenarios, `GuestSuite` in src/guest/suite/

**Checkpoint**: commit.

## Phase 3: The guest side

- [X] T008 [US2] `GdiKmt`, `GuestProgram`, entry point in src/um/
- [X] T009 [US3] src/km/rvkgpu.inf; tests/tools package check

**Checkpoint**: commit.

## Phase 4: The runner

- [X] T010 [US1] Tests then tools/guest/guest_agent.py (guest agent and QMP clients); tests/tools/test_guest_agent.py
- [X] T011 [US1] Tests then tools/guest/run_guest_suite.py with tools/guest/provision.ps1; fake QEMU, fake qemu-img, fake agent in tests/tools

**Checkpoint**: commit.

## Phase 5: Polish

- [X] T012 docs/testing.md (L5 as built, preparing an image), README, tools/gate.sh guest configuration; `tools/gate.sh`; merge; push

## Phase 6: A debuggable test cycle on a real guest

- [X] T013 Tests then `GuestAgent.pull`, `Qmp.drain`/`take_events`/`seen`, an abandonable `wait_for_agent` in tools/guest/guest_agent.py
- [X] T014 Tests then provision.ps1 `prepare` and `collect` stages; runner `--prepare-image`, diagnostics collected after every run, bugchecks named from `hv-crash`, the kernel dump pulled after the reboot or from a rescue boot without the device, `--hold`, `--gdb`, `--vnc`, `--trace`, `--on-panic`; fake QEMU panics and boot loops
- [X] T015 Tests then tools/guest/guest_shell.py
- [X] T016 Prepare the host's Windows image; run the suite live; docs/testing.md (preparing an image, debugging a run); `tools/gate.sh`; merge; push

**Checkpoint**: commit.
