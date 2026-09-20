# Data model: the desktop on the host GPU (058)

## A pixel format the device can scan out

`ScanoutFormat` holds one `VIRTIO_GPU_FORMAT_*` id and answers two things about
it: how many bytes a pixel is, and which DRM fourcc QEMU's `qemu_dmabuf_new`
must be given for it. The table is in core rather than in the glue because
choosing one is a decision, and Principle II leaves the glue none.

| virtio-gpu format | id | DRM fourcc | bytes |
|---|--:|---|--:|
| `B8G8R8A8_UNORM` | 1 | `DRM_FORMAT_ARGB8888` | 4 |
| `B8G8R8X8_UNORM` | 2 | `DRM_FORMAT_XRGB8888` | 4 |
| `A8R8G8B8_UNORM` | 3 | `DRM_FORMAT_BGRA8888` | 4 |
| `X8R8G8B8_UNORM` | 4 | `DRM_FORMAT_BGRX8888` | 4 |
| `R8G8B8A8_UNORM` | 67 | `DRM_FORMAT_ABGR8888` | 4 |
| `X8B8G8R8_UNORM` | 68 | `DRM_FORMAT_RGBX8888` | 4 |
| `A8B8G8R8_UNORM` | 121 | `DRM_FORMAT_RGBA8888` | 4 |
| `R8G8B8X8_UNORM` | 134 | `DRM_FORMAT_XBGR8888` | 4 |

The mapping is mechanical, and the test states the rule rather than the answers:
a virtio-gpu format's name is its **bytes in memory** (`B8G8R8A8` is B, then G,
then R, then A - that is what `virtio_gpu_get_pixman_format` means by
`PIXMAN_BE_b8g8r8a8`), and a DRM fourcc names the same bytes as a
little-endian 32-bit word read the other way round, so `B8G8R8A8` is
`DRM_FORMAT_ARGB8888`. The device computes it rather than asking QEMU because
QEMU's own `drm_format_pixman_map` (`ui/qemu-pixman.c`) holds five rows and
covers only four of these eight, so `qemu_pixman_to_drm_format` would answer 0
for half the formats the guest may legitimately ask for. Anything this table
does not list - id 0 included - is not a format this device scans out, and
`scannable()` is false for it.

- Invariant: `scannable()` and a non-zero `fourcc()` agree.
- Invariant: `pixel_bytes()` is 0 when it is not scannable, so a size computed
  from it cannot come out plausible by accident.

## One framebuffer inside a blob

`ScanoutFramebuffer` is what `SET_SCANOUT_BLOB` describes: a `ScanoutFormat`, a
`width` and `height` in pixels, a `stride` in bytes, an `offset` into the blob,
and the `GpuRect` of it that is the head's picture.

It is adopted from a decoded `SetScanoutBlobRequest` against one number the
device owns - the blob's size - and every term of the check is 64-bit and
overflow-checked, because all of them are the guest's:

```
pixel_bytes  = format.pixel_bytes()                     != 0
row_bytes    = width * pixel_bytes                      no overflow
stride                                                  >= row_bytes, <= INT32_MAX
last_row     = offset + (height - 1) * stride           no overflow
end          = last_row + row_bytes                     no overflow, <= blob size
rect                                                    inside width x height
```

`height - 1` rather than `height` because the last row needs only its own
pixels, which is how QEMU sizes it too (`virtio_gpu_scanout_blob_to_fb`) and
what lets a blob be exactly a framebuffer.

- Invariant: adopted or empty - a framebuffer that refused its request holds
  nothing and answers zero for every term.
- Invariant: an adopted framebuffer's `end` is within the size it was adopted
  against, and it remembers that size so the invariant can be re-checked
  without the request.
- Invariant: only plane 0 is ever set; a request naming a second plane is
  refused rather than truncated (research §4: QEMU can show one plane
  usefully, and a second would be silently dropped).

## What one head shows

`ScanoutBinding` is one display head. It holds the head's index, the blob id it
shows, its `ScanoutFramebuffer`, and one `GuestDmaBuf` over the **whole** blob -
not over the framebuffer, because `GuestDmaBuf::make` takes whole pages and a
framebuffer inside a blob is not page-aligned in general, while
`qemu_dmabuf_new` takes the offset per plane.

Its states are `empty` and `shown`, and a rebind goes straight from `shown` to
`shown`: the new descriptor is made and the new pin taken *before* the host is
told, and the old descriptor and pin are let go only once the host has taken
the new one. That order is what US2 scenario 2 means by "without a blank
frame", and it means a refused rebind leaves the head showing exactly what it
showed. For the moment between, the binding holds two descriptors and two
pins, and nothing else may observe it.

- Invariant: `shown` implies a made dma-buf, an adopted framebuffer and a
  non-zero blob id; `empty` implies none of the three.
- Invariant: the pin count this binding took is 1 while shown and 0 otherwise,
  so a leaked pin - which would make the blob unreleasable forever - is a
  failing invariant rather than a support case.

## The heads

`DisplayHeads` owns `max_heads` bindings (16, `VIRTIO_GPU_MAX_SCANOUTS`), the
`Host` it shows them through, the `BlobTable` it pins from, and the display's
answer once it has been asked for.

That answer is a third state, not a bool: `unasked`, `takes_dmabuf`,
`refuses_dmabuf`. It has to be, because it cannot be asked at realize - QEMU
registers its display backends after every device is realized
(`system/vl.c:3862-3864`) - and because asking twice on a host that answers
differently would let the capset and the scanout disagree.

- Invariant: no two bindings hold the same dma-buf descriptor.
- Invariant: every shown binding's blob is still in the table.
- Invariant: nothing is shown while the display's answer is not `takes_dmabuf`.
- Invariant: a head index a binding reports is its own index in the array.

`report(StateWriter&)` writes one line per head - `head/0 blob=7 shown=true
1280x800 stride=5120 offset=0 fourcc=XR24 frames=931` - which is US2's
"scanout-source trace" and FR-006's record of what changed when.

## The two wire records

`GpuRect` is virtio-gpu's four `le32` - `x`, `y`, `width`, `height` - and the
one question asked of it: does it lie inside a `w x h` framebuffer, with
`x + width` and `y + height` computed in 64 bits. Zero width or height is a
refusal, not an empty update: an update of nothing is a guest mistake worth
naming.

`SetScanoutBlobRequest` decodes the 96 bytes of `virtio_gpu_set_scanout_blob`:
the 24-byte header, a `GpuRect`, `scanout_id`, `resource_id`, `width`,
`height`, `format`, `padding`, `strides[4]`, `offsets[4]`. It validates
nothing beyond framing and the two structural rules that are the record's own -
the padding word is 0, and planes 1 to 3 have a zero stride and a zero offset -
because everything else needs the blob, which the record does not know.

`FlushRequest` decodes the 48 bytes of `virtio_gpu_resource_flush`: the header,
a `GpuRect`, `resource_id`, and a `padding` that must be 0.

- Invariant of both: a record that did not decode answers zero for every field,
  so a caller that ignored the status reads nothing plausible.

## The capset bit

`CapsetInfo::feature_dmabuf_scanout = 8u`, the fourth bit of the `features_`
field that already crosses the wire, beside `feature_host_visible_memory` (1),
`feature_gpu_exported_memory` (2) and `feature_guest_memory_import` (4). No
change to the capset's 128-byte encoding, no new virtio capability and no new
BAR - which matters, because feature 060 measured the configuration-space
budget at 192 bytes and found the express capability's 60 would not fit beside
the two shared-memory capabilities already there.

`DeviceCapset::resolve_display()` sets it, once, the first time the guest asks
for the capset.
