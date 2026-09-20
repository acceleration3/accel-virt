# Contract: blob scanout on the wire (058)

What the device core answers for the three virtio-gpu control commands that
choose what a display head shows. This replaces the row in `docs/protocol.md`
that reads `SET_SCANOUT_BLOB | ERR_INVALID_PARAMETER`, and that row is updated
in the same change.

Every request is guest-authored. Nothing below is read before the request's
length has been checked, and every sum is computed in 64 bits and checked for
overflow before it is compared (constitution V).

## `SET_SCANOUT_BLOB` (0x010D), 96 bytes

| Offset | Type | Field |
|--:|---|---|
| 0 | 24 bytes | the virtio-gpu control header |
| 24 | rect | `r`: `x`, `y`, `width`, `height`, each `le32` |
| 40 | u32 | `scanout_id` |
| 44 | u32 | `resource_id` |
| 48 | u32 | `width` |
| 52 | u32 | `height` |
| 56 | u32 | `format`, a `VIRTIO_GPU_FORMAT_*` |
| 60 | u32 | `padding` |
| 64 | 4 x u32 | `strides` |
| 80 | 4 x u32 | `offsets` |

### Disabling a head

`resource_id` 0 hides head `scanout_id`: the dma-buf is released, the blob
unpinned, the host told to stop showing it, and the answer is
`VIRTIO_GPU_RESP_OK_NODATA`. A head that shows nothing is hidden again with no
error - the guest may disable twice, and does, on a mode change.

### Showing a blob

Everything else binds head `scanout_id` to blob `resource_id`. A head that was
already showing something is not hidden first: the new descriptor is made and
the new pin taken, the host is told, and only then are the old ones let go, so
the head never goes dark between two pictures (US2 scenario 2) and a refused
rebind leaves it showing exactly what it showed. The answer is
`VIRTIO_GPU_RESP_OK_NODATA`.

### Every refusal, and why

| Condition | Answer |
|---|---|
| the request is shorter than 96 bytes | `ERR_INVALID_PARAMETER` |
| `padding` is not 0 | `ERR_INVALID_PARAMETER` |
| any of `strides[1..3]` or `offsets[1..3]` is not 0 | `ERR_INVALID_PARAMETER` |
| `scanout_id` is at or beyond 16 heads | `ERR_INVALID_PARAMETER` |
| the host has no head `scanout_id` | `ERR_UNSPEC` |
| the display cannot show a dma-buf | `ERR_UNSPEC` |
| `resource_id` names no blob | `ERR_INVALID_RESOURCE_ID` |
| `format` is not one of the eight in data-model.md | `ERR_INVALID_PARAMETER` |
| `width` or `height` is 0 | `ERR_INVALID_PARAMETER` |
| `strides[0]` is less than `width x 4` | `ERR_INVALID_PARAMETER` |
| `strides[0]` is above `INT32_MAX` | `ERR_INVALID_PARAMETER` |
| `offsets[0] + (height - 1) x strides[0] + width x 4` overflows | `ERR_INVALID_PARAMETER` |
| that sum is beyond the blob's size | `ERR_INVALID_PARAMETER` |
| `r` is not inside `width x height`, or has a zero side | `ERR_INVALID_PARAMETER` |
| the blob's pages cannot become a dma-buf | `ERR_UNSPEC` |
| the host-visible blob it names has none to make | `ERR_UNSPEC` |

A refusal changes nothing: the head goes on showing what it showed, no pin is
taken and no descriptor is opened. The rule the answers follow is one line -
`ERR_INVALID_PARAMETER` is the guest being wrong, anything else is the host
declining and is `ERR_UNSPEC` - so a guest can tell the two apart without a
table.

The three `ERR_UNSPEC` rows are all the same thing from the guest's side: this
host will not show this memory. A host-visible blob is one of them today
because `Host::create_host_memory` is a memfd with no seals and udmabuf takes
only a sealed one (research §3); rung 2 makes that row a success, and nothing
about the answer's shape changes when it does.

## `RESOURCE_FLUSH` (0x0104), 48 bytes

| Offset | Type | Field |
|--:|---|---|
| 0 | 24 bytes | header |
| 24 | rect | `r` |
| 40 | u32 | `resource_id` |
| 44 | u32 | `padding` |

The core answers this only when `resource_id` names a blob; a 2D resource is
the base class's, as it is today. For a blob:

| Condition | Answer |
|---|---|
| the request is shorter than 48 bytes, or `padding` is not 0 | `ERR_INVALID_PARAMETER` |
| no head shows this blob | `ERR_INVALID_PARAMETER` |
| `r` is not inside the head's framebuffer, or has a zero side | `ERR_INVALID_PARAMETER` |
| otherwise | `OK_NODATA`, and every head showing the blob is updated |

A blob on two heads is updated on both, which is what the guest asked for by
binding it twice.

## `SET_SCANOUT` (0x0103) naming a blob

Not intercepted, and that is a decision rather than an omission. The base class
answers `ERR_INVALID_RESOURCE_ID` for any resource it does not hold, and a blob
is not one of its resources, so the guest already gets the right answer.
Intercepting it would mean a third decoder to say what is already said.

## `RESOURCE_UNREF` of a blob a head shows

`ERR_INVALID_PARAMETER`, and the blob is not released. This falls out of what
is already there: a shown blob is pinned, and `BlobTable::release` answers
`invalid_state` for a pinned blob, which `BlobCommands::unref` already turns
into that response. The guest disables the scanout first.

A device reset releases every head before it releases the blobs, so a reset
does not have to observe the rule.

## What the guest may assume

- A head goes on showing the last blob it was bound to until it is rebound,
  hidden, or the device is reset. A flush does not change what is shown.
- The bytes the head shows are the blob's own pages. Nothing is copied, and a
  write the guest makes to those pages is visible at the next flush without
  any further command.
- `feature_dmabuf_scanout` in the capset is the only promise that any of this
  will work. Without it every `SET_SCANOUT_BLOB` answers `ERR_UNSPEC` and the
  guest is expected to use `SET_SCANOUT` and `TRANSFER_TO_HOST_2D` as it does
  today.
