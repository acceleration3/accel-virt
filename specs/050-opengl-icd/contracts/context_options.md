# Contract: a host OpenGL context made with the application's options

Three hops, each judging what the one before could have lied about.

## 1. ICD to KMD: `RvkContextRecord`'s reserved tail

The private data of `D3DKMTCreateContextVirtual` stays 32 bytes (magic `RVKT`,
sub-API, ring order, ring, fence, reply, as 043 defined it). Its last eight
bytes, reserved and required zero until now, carry the options:

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 24 | u8 | major | 1..4 for OpenGL; 0 for any other sub-API |
| 25 | u8 | minor | at most the highest minor of `major` (1.5, 2.1, 3.3, 4.6) |
| 26 | u8 | profile | 0 none, 1 core, 2 compatibility (050 sends only 0 and 1) |
| 27 | u8 | flags | bit 0 debug, bit 1 forward compatible, bit 2 robust access, bit 3 lose context on reset; bits 4-7 zero |
| 28 | u32 | share_context | the D3DKMT handle of another rvk OpenGL context of this process and device, or 0 |

All eight zero is the old meaning (the host's default context, no sharing), so
every existing caller is unchanged. The KMD refuses (`STATUS_INVALID_PARAMETER`)
a non-zero option for a sub-API other than OpenGL, any rule above broken
(major 0 with a non-zero minor, profile or flag included), and a share context
that is not a live rvk OpenGL context of the calling process on the same device.

## 2. KMD to device: `ContextSetup` options

`ContextSetup` (32 bytes, every byte used) grows to 48 under the next protocol
minor version: the options above with `share_context` replaced by the share
context's device context id, then four reserved bytes. A setup of the previous
minor version is 32 bytes and means default options; a 48-byte setup with an
older minor, or a 32-byte one with the new minor, is malformed. The device refuses the
create (`ERR_INVALID_PARAMETER`) for options it cannot pass on: a share id that
is not a live OpenGL context on the same host GPU, or non-default options with a
version 3 executor.

## 3. Device to executor: ABI version 4

```c
#define RVK_EXECUTOR_ABI_VERSION_OPTIONS 4u
typedef struct RvkContextOptions {
    uint8_t major, minor, profile, flags;
    uint32_t reserved;
} RvkContextOptions;
RvkExecutor *(*create_with_options)(void *opaque, uint32_t sub_api, const uint8_t *gpu_uuid,
                                    const RvkContextOptions *options, RvkExecutor *share);
```

`options` null means the defaults (`create`'s behaviour); `share` null means no
sharing. An OpenGL executor makes its EGL context with the options and the
share executor's context; a version the driver refuses answers null, which the
device reports to the guest as the create's failure and the ICD as
`ERROR_INVALID_VERSION_ARB` or `ERROR_INVALID_PROFILE_ARB`. Other sub-APIs
refuse non-default options. `RVK_EXECUTOR_ABI_VERSION` becomes 4; a table of
version 4 without `create_with_options` is refused at accept.

## 4. The blob arena import (045's `IMPORT_BLOB`, OpenGL)

`IMPORT_BLOB` on an OpenGL context: `OpenGlBackend::adopt` takes the host
pointer as the region its bulk-data commands read and write, answers a
non-zero handle, and refuses a second import while one is held.
`import_kinds` reports `RVK_BLOB_HOST_POINTER` for OpenGL.
