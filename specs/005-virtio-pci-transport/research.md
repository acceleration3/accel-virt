# Research: virtio-pci transport

## R1. Layouts, checked against virtio 1.2 cs01 text

- 4.1.4 `struct virtio_pci_cap` (16 bytes): `cap_vndr` 0x09, `cap_next`,
  `cap_len`, `cfg_type`, `bar`, `id`, 2 padding, le32 `offset`, le32 `length`.
  `cfg_type`: 1 common, 2 notify, 3 ISR, 4 device, 5 PCI cfg access, 8 shared
  memory, 9 vendor; others reserved. `bar` 0-5, others reserved.
- `struct virtio_pci_cap64` (24 bytes): + u32 `offset_hi`, u32 `length_hi`
  (shared memory uses it).
- 4.1.4.4 notify cap (20 bytes): + le32 `notify_off_multiplier`, 0 or an even
  power of 2; queue notify address = `cap.offset + queue_notify_off *
  notify_off_multiplier`, and `cap.length >= queue_notify_off * multiplier + 2`.
- 4.1.4.1 driver requirements: use the first instance of each type it can
  support; ignore reserved `cfg_type` and reserved `bar`; accept a larger
  `cap_len`; handle unexpectedly large `length`.
- 4.1.4.3 common configuration offsets: 0x00 device_feature_select, 0x04
  device_feature, 0x08 driver_feature_select, 0x0C driver_feature, 0x10
  config_msix_vector, 0x12 num_queues, 0x14 device_status, 0x15
  config_generation, 0x16 queue_select, 0x18 queue_size, 0x1A
  queue_msix_vector, 0x1C queue_enable, 0x1E queue_notify_off, 0x20 queue_desc,
  0x28 queue_driver, 0x30 queue_device, 0x38 queue_notify_data, 0x3A queue_reset.
- `VIRTIO_MSI_NO_VECTOR` 0xFFFF. Maximum queue size 32768, always a power of 2.
- Split virtqueue: desc 16 bytes (le64 addr, le32 len, le16 flags NEXT 1 /
  WRITE 2 / INDIRECT 4, le16 next); avail le16 flags, le16 idx, le16 ring[N];
  used le16 flags, le16 idx, {le32 id, le32 len}[N].

## R2. What QEMU 11.1.1 actually presents

Captured with `tools/host/capture_pci_config.py` (qtest, no guest), committed
as fixtures:

| Fixture | Capabilities (offset: type, BAR, offset, length) |
|---|---|
| `virtio_gpu_pci_config.bin` | 0x98 MSI-X; 0x84 PCI cfg; 0x70 notify BAR4 0x3000/0x1000 mult 4; 0x60 device BAR4 0x2000; 0x50 ISR BAR4 0x1000; 0x40 common BAR4 0x0 |
| `virtio_gpu_gl_pci_hostmem_config.bin` | 0xB0 MSI-X; 0x9C PCI cfg; 0x88 notify BAR2 0x3000 mult 4; 0x78 device BAR2; 0x68 ISR BAR2; 0x58 common BAR2; 0x40 shared memory id 1 BAR4 0x0 length 0x10000000 |

## R3. Kernel primitives

`MmAllocateContiguousMemorySpecifyCache` / `MmFreeContiguousMemorySpecifyCache`
and `MmGetPhysicalAddress` for ring memory; `KeStallExecutionProcessor` for the
bounded reset wait; `READ_REGISTER_*` / `WRITE_REGISTER_*` on the mapped BAR
for register windows. No IOMMU is negotiated (`VIRTIO_F_ACCESS_PLATFORM`), so a
guest physical address is what the device dereferences.
