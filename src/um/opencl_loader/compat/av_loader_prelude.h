/* What the kit's user-mode header closure does not give the Khronos OpenCL
 * loader, forced in ahead of its own sources.
 *
 * NTSTATUS: the loader's D3DKMT discovery path spells its function pointers
 * with it (loader/windows/adapter.h), and it arrives in the SDK through headers
 * this closure does not take. A macro rather than a typedef, so the loader's
 * own `#ifndef NTSTATUS` guard in icd_windows_dxgk.c still sees it defined and
 * does not declare it a second time. */
#ifndef AV_OPENCL_LOADER_PRELUDE_H
#define AV_OPENCL_LOADER_PRELUDE_H

/* The loader's own guarded block in icd_windows_dxgk.c declares all four
 * together and is skipped once NTSTATUS is defined, so all four are here. */
#ifndef NTSTATUS
#define NTSTATUS LONG
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#define NT_SUCCESS(status) (((NTSTATUS)(status)) >= 0)
#endif

#endif  /* AV_OPENCL_LOADER_PRELUDE_H */
