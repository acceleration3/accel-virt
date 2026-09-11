# Quickstart: validating feature 002

```bash
cmake --build build && ctest --test-dir build      # unit + gates, includes the kernel platform link
tools/gate.sh                                      # coverage 100%, fuzz smoke traps on unclaimed violations
```

| Check | Expect |
|---|---|
| `build/test_mock_platform` | every violation kind provoked, claimed; each "unclaimed fails" case reported |
| `build/test_spin_lock`, `build/test_mutex` | IRQL raised and restored per guard, lock order enforced, early return releases |
| `build/test_gpu_virtual_address_space` | passes with claims replacing flag checks; the destroy-before-reserve case releases nothing |
| remove the null guard in `~GpuVirtualAddressSpace` | `address_space_lifetime` test fails with `null_release` |
| `llvm-readobj --coff-imports build/driver/rvkgpu.sys` | still only `ntoskrnl.exe` (the kernel platform is linked but not yet instantiated) |
