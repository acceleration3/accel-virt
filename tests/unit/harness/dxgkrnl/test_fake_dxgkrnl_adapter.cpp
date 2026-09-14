/* The harness is trusted by every adapter test, so it is tested against the
 * core classes that consume it. */
#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/pci/mapped_register_window.hpp"
#include "core/pci/pci_base_addresses.hpp"
#include "core/pci/resource_list.hpp"
#include "core/virtio/virtio_capability_walk.hpp"
#include "core/virtio/virtio_pci_layout.hpp"
#include "harness/device/fake_virtio_gpu_function.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

using av::DxgkDeviceInfoRecord;
using av::DxgkrnlInterface;
using av::FakeDxgkrnlAdapter;
using av::FakeVirtioGpuFunction;
using av::MappedRegisterWindow;
using av::MockPlatform;
using av::PciBaseAddresses;
using av::ResourceList;
using av::Status;
using av::TestRegistry;
using av::Violation;
using av::VirtioCapabilityWalk;
using av::VirtioPciLayout;

TEST(fake_dxgkrnl_adapter, presents_a_function_core_can_find_and_map)
{ MockPlatform platform; FakeVirtioGpuFunction function;
  FakeDxgkrnlAdapter dxgkrnl(platform, function);
  DxgkrnlInterface interface(platform);
  CHECK_STATUS(interface.initialize(&dxgkrnl.record()), Status::ok);

  DxgkDeviceInfoRecord info;
  CHECK_STATUS(interface.get_device_information(info), Status::ok);
  ResourceList resources(platform);
  CHECK_STATUS(resources.initialize(info.TranslatedResourceList, ResourceList::max_bytes), Status::ok);
  CHECK_EQ(resources.message_interrupts(), 3u);

  std::uint8_t config[256];
  CHECK_STATUS(interface.read_config_space(config, 256u), Status::ok);
  VirtioPciLayout layout;
  CHECK_STATUS(VirtioCapabilityWalk(config, 256u).walk(layout), Status::ok);
  std::uint64_t address = 0u, length = 0u;
  CHECK_STATUS(PciBaseAddresses(config, 256u).address(layout.common().bar(), address), Status::ok);
  CHECK_STATUS(resources.find_memory(address, 0x4000u, length), Status::ok);

  void *mapped = nullptr;
  CHECK_STATUS(interface.map_memory(address, static_cast<std::uint32_t>(length), mapped), Status::ok);
  CHECK_EQ(dxgkrnl.live_mappings(), 1u);
  MappedRegisterWindow common(platform, mapped, layout.common().length());
  CHECK_EQ(common.read_u16(0x12u), 2u);   /* num_queues */
  CHECK_STATUS(interface.unmap_memory(mapped), Status::ok);
  CHECK_EQ(dxgkrnl.live_mappings(), 0u);
  CHECK_EQ(common.read_u16(0x12u), 0xFFFFu);
  CHECK(platform.claim(Violation::unmapped_register));
  CHECK(interface.queue_dpc());
  CHECK_EQ(dxgkrnl.dpcs_queued(), 1u); }

TEST(fake_dxgkrnl_adapter, knobs_refuse_each_callback)
{ MockPlatform platform; FakeVirtioGpuFunction function;
  FakeDxgkrnlAdapter dxgkrnl(platform, function);
  DxgkrnlInterface interface(platform);
  CHECK_STATUS(interface.initialize(&dxgkrnl.record()), Status::ok);
  DxgkDeviceInfoRecord info;
  dxgkrnl.refuse_device_information(0xC0000017u);
  CHECK_STATUS(interface.get_device_information(info), Status::no_memory);
  std::uint8_t config[256];
  dxgkrnl.short_config_read();
  CHECK_STATUS(interface.read_config_space(config, 256u), Status::malformed);
  dxgkrnl.refuse_config_read(0xC00000BBu);
  CHECK_STATUS(interface.read_config_space(config, 256u), Status::not_supported);
  void *mapped = nullptr;
  dxgkrnl.refuse_map(2u, 0xC000009Au);
  CHECK_STATUS(interface.map_memory(FakeVirtioGpuFunction::msix_bar_address, 0x1000u, mapped), Status::ok);
  void *refused = &info;
  CHECK_STATUS(interface.map_memory(FakeVirtioGpuFunction::virtio_bar_address, 0x4000u, refused),
               Status::no_space);
  CHECK(refused == nullptr);
  CHECK_EQ(dxgkrnl.maps(), 1u);
  CHECK_STATUS(interface.unmap_memory(mapped), Status::ok); }

TEST(fake_dxgkrnl_adapter, a_mapping_left_behind_fails_the_test)
{ { MockPlatform platform; FakeVirtioGpuFunction function;
    FakeDxgkrnlAdapter dxgkrnl(platform, function);
    void *mapped = nullptr;
    CHECK_EQ(dxgkrnl.map_memory(static_cast<std::int64_t>(FakeVirtioGpuFunction::virtio_bar_address), 0x4000u,
                                0u, 0u, 0, &mapped), 0); }
  CHECK(TestRegistry::instance().take_current_failure()); }

/* dxgmms2.sys bugchecks 0x119/0xD on DMA_FAULTED from this adapter
 * (VidSchDdiNotifyInterruptWorker @140031b80), so a driver that reports one
 * fails the test and nothing is delivered; a completion still is. */
TEST(fake_dxgkrnl_adapter, a_dma_faulted_interrupt_fails_the_test_and_is_not_delivered)
{ MockPlatform platform; FakeVirtioGpuFunction function;
  FakeDxgkrnlAdapter dxgkrnl(platform, function);
  platform.set_irql(av::Irql::isr);
  av::DxgkargcbNotifyInterruptDataRecord faulted = {};
  faulted.InterruptType = 4;
  faulted.FenceId = 0x2Eu;
  dxgkrnl.notify_interrupt(faulted);
  CHECK(TestRegistry::instance().take_current_failure());
  CHECK_EQ(dxgkrnl.interrupts(), 0u);
  av::DxgkargcbNotifyInterruptDataRecord completed = {};
  completed.InterruptType = 1;
  completed.FenceId = 0x2Eu;
  dxgkrnl.notify_interrupt(completed);
  platform.set_irql(av::Irql::passive);
  CHECK_EQ(dxgkrnl.interrupts(), 1u);
  CHECK_EQ(dxgkrnl.interrupt(0u).FenceId, 0x2Eu); }

TEST(fake_virtio_gpu_function, knobs_shape_the_resource_list)
{ MockPlatform platform; FakeVirtioGpuFunction function;
  function.set_message_interrupts(0u);
  function.set_virtio_bar_length(0x1000u);
  ResourceList resources(platform);
  CHECK_STATUS(resources.initialize(function.resources().bytes(), function.resources().size()), Status::ok);
  CHECK_EQ(resources.message_interrupts(), 0u);
  std::uint64_t length = 0u;
  CHECK_STATUS(resources.find_memory(FakeVirtioGpuFunction::virtio_bar_address, 0x4000u, length), Status::malformed);
  function.remove_virtio_bar_resource();
  CHECK_STATUS(resources.initialize(function.resources().bytes(), function.resources().size()), Status::ok);
  CHECK_STATUS(resources.find_memory(FakeVirtioGpuFunction::virtio_bar_address, 1u, length), Status::not_found); }
