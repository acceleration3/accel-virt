# Contract: exporting a host image for scanout (executor ABI version 6)

**Specified, not built.** Nothing can call this until feature 056 makes DWM
compose into a host image on this adapter and feature 041 lets the guest name
one. It is written here so that building it later is transcription rather than
design, and so that the reason it is absent is on the record (plan.md, "What
this feature does not build").

## The gap

`src/device/abi/rvk_executor.h` at version 5 is import-only:

- `import_blob(executor, blob_id, handle)` takes an `RvkBlobHandle` - a host
  pointer or a dma-buf - **into** the host API;
- `imported_handle(executor, blob_id)` answers the wire handle that import
  became, so the guest can name it;
- nothing hands a host object **out**.

So a `VkImage` the guest created on an executor - which is what DWM's primary
becomes once there is a D3D UMD - has no route to a dma-buf, and therefore no
route to `qemu_console_gl_scanout_dmabuf`. Rung 3 of research §3 is blocked on
exactly this one call.

## Version 6

```c
#define RVK_EXECUTOR_ABI_VERSION 6u
#define RVK_EXECUTOR_ABI_VERSION_SCANOUT 6u

/* One plane of an exported image. */
typedef struct RvkExportedImage {
    int      fd;         /* a dma-buf; the caller owns it and closes it */
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t offset;
    uint32_t fourcc;     /* DRM */
    uint32_t reserved;   /* 0 */
    uint64_t modifier;   /* DRM; DRM_FORMAT_MOD_LINEAR when the image is linear */
} RvkExportedImage;

    /* Version 6 onwards. A version 5 table ends above.

       The image `wire_handle` names on this executor, exported as a dma-buf
       the host display can import. RVK_EXECUTED with *image filled in, or
       RVK_UNDECODABLE for a handle that names no image, an image the host
       cannot export, or a layout with no DRM modifier. Runs on the context's
       worker.

       The export is a second reference to the image's memory, not a copy: the
       guest goes on rendering into it while it is shown. What it is not is a
       promise of synchronisation - see below. */
    uint32_t (*export_image)(void *opaque, RvkExecutor *executor, uint64_t wire_handle,
                             RvkExportedImage *image);
```

A version 5 library exports nothing and the device reads no further than its
`abi_version` allows, exactly as it already does for the five versions before
this one. A device whose executor is version 5 reports no
`feature_dmabuf_scanout` for host images and keeps rungs 0 and 1.

## What the Vulkan backend has to do

1. The image must have been created for export. `VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT`
   in a `VkExternalMemoryImageCreateInfo` at create time, which means the
   backend has to know at `vkCreateImage` that the image may become a scanout.
   The guest says so, because the guest is the one that knows it is a primary:
   a create flag on the wire, not a guess in the backend.
2. `vkGetMemoryFdKHR` with that handle type on the memory the image is bound
   to. `VulkanBackend::on_vkGetMemoryFdKHR` already exists
   (`src/device/executor/vulkan_backend.cpp`), so this is one more caller.
3. The layout. With `VK_EXT_image_drm_format_modifier`,
   `vkGetImageDrmFormatModifierPropertiesEXT` answers the modifier and
   `vkGetImageSubresourceLayout` the plane's offset and row pitch. Without it
   the image must be `VK_IMAGE_TILING_LINEAR` and the modifier is
   `DRM_FORMAT_MOD_LINEAR`; an optimally-tiled image with no modifier
   extension is `RVK_UNDECODABLE`, never an export EGL would misread.
4. The fourcc from the `VkFormat`, by the same rule `ScanoutFormat` uses.

## What it deliberately does not carry

**Synchronisation.** `qemu_dmabuf_new` takes an `allow_fences` flag and
`qemu_dmabuf_set_fence_fd` an explicit fence, and nothing in this project
produces a sync fd. Until something does, a scanned-out host image is shown
whenever the display asks and may be shown mid-frame. That is the same tearing
a guest gets from an immediate-mode present, it is what stock virtio-gpu's
blob scanout does, and calling it anything better would be a claim constitution
VI forbids. The fence is the next version's, and the guest-visible symptom that
would justify it is tearing that vertical-blank pacing does not remove.

**More than one plane.** One plane, as everywhere else in this feature: QEMU
shows one usefully and a second would be dropped silently.

## Where it plugs in

`DisplayHeads::show` learns a third source of a descriptor beside "the blob's
guest pages" and "the blob's host block": "the image the guest named". The
`ScanoutBinding`'s shape does not change - a descriptor, a geometry, and the
thing it must let go of when the head is unbound - only what fills the first
and the third.
