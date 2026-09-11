---
description: "Task list for feature 035: in-guest integration suite"
---

# Tasks: In-guest integration suite

Tests come first in every task and must fail before the code they cover exists.

## Phase 1: Building for the guest's user mode

- [ ] T001 Kit: the `um` header closure in tools/fetch_windows_kit.sh, the overlay root, VERSION subset line, CMake check; tests/tools overlay test
- [ ] T002 Move `TextBuffer` to src/core/support; `append_hex`; tests
- [ ] T003 cmake/windows_user_toolchain.cmake, src/guest/win32 CMake project with `.def` import libraries; `rvktest` external project; `guest_image` check (console subsystem, imports only gdi32/kernel32/ntdll)

**Checkpoint**: commit.

## Phase 2: The suite's logic

- [ ] T004 [US2] Tests then `KmtAdapter`, `PciIdentity`, `Kmt` in src/guest/kmt/; `MockKmt` in tests/harness/guest/; gates scan src/guest
- [ ] T005 [US2] Tests then `GuestReport` in src/guest/suite/
- [ ] T006 [US2] Tests then `AdapterSearch`, `GuestSession` in src/guest/suite/
- [ ] T007 [US2] Tests then `Scenario`, the four scenarios, `GuestSuite` in src/guest/suite/

**Checkpoint**: commit.

## Phase 3: The guest side

- [ ] T008 [US2] `GdiKmt`, `GuestProgram`, entry point in src/guest/win32/
- [ ] T009 [US3] src/km/rvkgpu.inf; tests/tools package check

**Checkpoint**: commit.

## Phase 4: The runner

- [ ] T010 [US1] Tests then tools/guest/guest_agent.py (QGA client)
- [ ] T011 [US1] Tests then tools/guest/run_guest_suite.py with tools/guest/provision.ps1; fake QEMU, fake qemu-img, fake agent in tests/tools

**Checkpoint**: commit.

## Phase 5: Polish

- [ ] T012 docs/testing.md (L5 as built, preparing an image), README, tools/gate.sh guest configuration; `tools/gate.sh`; merge; push
