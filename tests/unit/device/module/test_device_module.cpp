#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "core/trace/state_reporter.hpp"
#include "core/trace/state_writer.hpp"
#include "device/abi/entry_points.hpp"
#include "device/abi/av_device.h"
#include "device/module/device_instance.hpp"
#include "device/module/device_module.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "device/module/driver_status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_host_ops.hpp"
#include "harness/host/mock_request.hpp"

#include <cstdint>
#include <cstring>

using av::DeviceInstance;
using av::DeviceModule;
using av::GpuRequestBuilder;
using av::MockHost;
using av::MockHostOps;
using av::MockRequest;
using av::Status;

namespace {

constexpr std::uint8_t gpu[16] = {0x7b, 0x7a};

av::ObjectStorage<DeviceModule> module_storage;

class FreshModule {
public:
    FreshModule() { CHECK_STATUS(module_storage.construct(), Status::ok); }
    ~FreshModule() { module_storage.destroy(); }
    FreshModule(const FreshModule &) = delete;
    FreshModule &operator=(const FreshModule &) = delete;
    DeviceModule &module() { return module_storage.object(); }
};

}

TEST(device_module_create, realizes_a_device_on_the_named_gpu)
{ MockHost host; MockHostOps ops(host);
  host.add_gpu(gpu);
  FreshModule fresh;
  DeviceModule &module = fresh.module();
  char error[96] = "untouched";
  DeviceInstance *const instance = module.create(&ops.table(), nullptr, "auto", false, nullptr, nullptr, nullptr, error, sizeof error);
  CHECK(instance != nullptr);
  CHECK(instance->device().realized());
  CHECK_EQ(error[0], '\0');
  CHECK_EQ(module.live(), 1u);
  instance->device().status(av::DriverStatus::driver_ok);
  CHECK_STATUS(instance->device().create_context(3u), Status::ok);
  CHECK_STATUS(module.destroy(instance), Status::ok);
  CHECK_EQ(module.live(), 0u);
  CHECK_EQ(host.live_threads(), 0u);
  CHECK_STATUS(module.destroy(instance), Status::invalid_parameter); }

TEST(device_module_report, writes_every_live_device_under_its_slot)
{ MockHost host; MockHostOps ops(host);
  host.add_gpu(gpu);
  FreshModule fresh;
  DeviceModule &module = fresh.module();
  av::StateReporter &root = module;
  char dump[512];
  av::StateWriter none(dump, sizeof dump);
  root.report(none);
  CHECK(std::strcmp(none.dump(), "module/0 devices=0") == 0);
  char error[96] = {};
  DeviceInstance *const first = module.create(&ops.table(), nullptr, "auto", false, nullptr, nullptr, nullptr, error, sizeof error);
  DeviceInstance *const second = module.create(&ops.table(), nullptr, "auto", false, nullptr, nullptr, nullptr, error, sizeof error);
  CHECK_STATUS(module.destroy(first), Status::ok);
  av::StateWriter one(dump, sizeof dump);
  root.report(one);
  CHECK(std::strcmp(one.dump(), "module/0 devices=1\n"
                                "module/0/device/1 realized=true gpu=0 capsets=0 blobs=0 scanouts=0 "
                                "contexts=0 driver_ok=false\n"
                                "module/0/device/1/host_blobs/0 live=0\n"
                                "module/0/device/1/contexts/0 live=0") == 0);
  CHECK_STATUS(module.destroy(second), Status::ok); }

TEST(device_module_destroy, finds_the_device_named_among_others)
{ MockHost host; MockHostOps ops(host);
  host.add_gpu(gpu);
  FreshModule fresh;
  DeviceModule &module = fresh.module();
  char error[96] = {};
  DeviceInstance *const first = module.create(&ops.table(), nullptr, "auto", false, nullptr, nullptr, nullptr, error, sizeof error);
  DeviceInstance *const second = module.create(&ops.table(), nullptr, "auto", false, nullptr, nullptr, nullptr, error, sizeof error);
  CHECK_STATUS(module.destroy(second), Status::ok);
  CHECK_EQ(module.live(), 1u);
  CHECK_STATUS(module.destroy(first), Status::ok); }

TEST(device_module_create, a_failed_realize_or_missing_table_leaves_no_device)
{ MockHost host; MockHostOps ops(host);
  FreshModule fresh;
  DeviceModule &module = fresh.module();
  char error[96] = {};
  CHECK(module.create(&ops.table(), nullptr, "not-a-uuid", false, nullptr, nullptr, nullptr, error, sizeof error) == nullptr);
  CHECK(std::strstr(error, "is neither auto nor a device UUID") != nullptr);
  CHECK_EQ(module.live(), 0u);
  CHECK(module.create(nullptr, nullptr, "auto", false, nullptr, nullptr, nullptr, error, sizeof error) == nullptr);
  CHECK(std::strcmp(error, "virtio-gpu-av: no host operations") == 0); }

TEST(device_module_create, one_device_per_slot)
{ MockHost host; MockHostOps ops(host);
  host.add_gpu(gpu);
  FreshModule fresh;
  DeviceModule &module = fresh.module();
  char error[96] = {};
  DeviceInstance *instances[DeviceModule::max_devices] = {};
  for (DeviceInstance *&instance : instances) {
      instance = module.create(&ops.table(), nullptr, nullptr, false, nullptr, nullptr, nullptr, error, sizeof error);
      CHECK(instance != nullptr);
  }
  CHECK(module.create(&ops.table(), nullptr, nullptr, false, nullptr, nullptr, nullptr, error, sizeof error) == nullptr);
  CHECK(std::strcmp(error, "virtio-gpu-av: every device slot is in use") == 0);
  for (DeviceInstance *instance : instances) CHECK_STATUS(module.destroy(instance), Status::ok); }

TEST(av_device_abi, create_reset_and_destroy_through_the_c_entry_points)
{ MockHost host; MockHostOps ops(host);
  host.add_gpu(gpu);
  char error[96] = {};
  AvDevice *const device = av_device_create(&ops.table(), nullptr, "auto", 0, nullptr, nullptr, nullptr, error, sizeof error);
  CHECK(device != nullptr);
  auto *const instance = reinterpret_cast<DeviceInstance *>(device);
  av_device_status(device, 0x0Fu);
  CHECK(host.fence_interrupts_connected());
  CHECK_STATUS(instance->device().create_context(1u), Status::ok);
  av_device_reset(device);
  CHECK(!host.fence_interrupts_connected());
  CHECK_EQ(instance->device().contexts(), 0u);
  CHECK_EQ(av::device_module.live(), 1u);
  CHECK_EQ(av_device_destroy(device), 0);
  CHECK_EQ(av_device_destroy(device), 1);
  CHECK(av_device_create(&ops.table(), nullptr, "gpu0", 0, nullptr, nullptr, nullptr, error, sizeof error) == nullptr);
  CHECK_EQ(av::device_module.live(), 0u); }

TEST(av_device_abi, av_device_pci_identity_answers_the_bound_gpus_ids_or_nothing)
{ MockHost host; MockHostOps ops(host);
  host.add_gpu(gpu);
  host.add_gpu(gpu);
  host.place_gpu_on_pci(0u, MockHost::PciBus{0u, 0x03u, 0u, 0u});
  host.add_file("/sys/bus/pci/devices/0000:03:00.0/vendor", "0x1002\n", 7u);
  host.add_file("/sys/bus/pci/devices/0000:03:00.0/device", "0x7550\n", 7u);
  host.add_file("/sys/bus/pci/devices/0000:03:00.0/revision", "0xc0\n", 5u);
  char error[96] = {};
  AvDevice *const device = av_device_create(&ops.table(), nullptr, "auto", 0, nullptr, nullptr, nullptr, error, sizeof error);
  CHECK(device != nullptr);
  AvPciIdentity identity = {};
  CHECK_EQ(av_device_pci_identity(device, &identity), 1);
  CHECK_EQ(identity.vendor_id, 0x1002u);
  CHECK_EQ(identity.device_id, 0x7550u);
  CHECK_EQ(identity.revision, 0xC0u);
  CHECK_EQ(av_device_destroy(device), 0);
  MockHost bare; MockHostOps bare_ops(bare);
  bare.add_gpu(gpu);
  AvDevice *const off_pci = av_device_create(&bare_ops.table(), nullptr, "auto", 0, nullptr, nullptr, nullptr, error, sizeof error);
  AvPciIdentity untouched = {0xAAAAu, 0xBBBBu, 0xCCu};
  CHECK_EQ(av_device_pci_identity(off_pci, &untouched), 0);
  CHECK_EQ(untouched.vendor_id, 0xAAAAu);
  CHECK_EQ(av_device_destroy(off_pci), 0); }

TEST(av_device_abi, av_device_control_answers_blob_requests_and_leaves_the_rest_to_the_base_class_and_av_device_capsets_offers_one)
{ MockHost host; MockHostOps ops(host);
  host.add_gpu(gpu);
  host.set_hostmem_size(0x10000u);
  host.add_guest_ram(0x100000u, 0x1000u);
  av::MockExecutorOps executor(AV_EXECUTOR_SUB_API_VULKAN);
  char error[96] = {};
  AvDevice *const device = av_device_create(&ops.table(), &executor.table(), "auto", 0, nullptr, nullptr, nullptr, error, sizeof error);
  CHECK(device != nullptr);
  CHECK_EQ(av_device_capsets(device), 1u);
  GpuRequestBuilder create(0x010Cu);
  create.u32(1u).u32(1u).u32(0u).u32(1u).u64(0u).u64(0x1000u).u64(0x100000u).u32(0x1000u).u32(0u);
  MockRequest blob(create.bytes(), create.size());
  CHECK_EQ(av_device_control(device, &blob, 0x010Cu), 1);
  CHECK_EQ(blob.response_type(), 0x1100u);
  GpuRequestBuilder display_info(0x0100u);
  MockRequest base(display_info.bytes(), display_info.size());
  CHECK_EQ(av_device_control(device, &base, 0x0100u), 0);
  CHECK_EQ(host.live_guest_mappings(), 1u);
  CHECK_EQ(av_device_destroy(device), 0);
  CHECK_EQ(host.live_guest_mappings(), 0u);
  CHECK(av_device_create(&ops.table(), nullptr, "auto", 1, nullptr, nullptr, nullptr, error, sizeof error) == nullptr);
  CHECK(std::strstr(error, "blob=on is not supported") != nullptr); }
