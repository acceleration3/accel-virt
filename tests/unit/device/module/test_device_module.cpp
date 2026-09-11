/* The module's devices through the C ABI the QEMU glue calls, over a C host
 * table that reaches the MockHost. */
#include "core/support/status.hpp"
#include "device/abi/entry_points.hpp"
#include "device/abi/rvk_device.h"
#include "device/module/device_instance.hpp"
#include "device/module/device_module.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_host_ops.hpp"

#include <cstdint>
#include <cstring>

using av::DeviceInstance;
using av::DeviceModule;
using av::MockHost;
using av::MockHostOps;
using av::Status;

namespace {

constexpr std::uint8_t gpu[16] = {0x7b, 0x7a};

}  // namespace

TEST(device_module_create, realizes_a_device_on_the_named_gpu)
{ MockHost host; MockHostOps ops(host);
  host.add_gpu(gpu);
  DeviceModule module;
  char error[96] = "untouched";
  DeviceInstance *const instance = module.create(&ops.table(), "auto", error, sizeof error);
  CHECK(instance != nullptr);
  CHECK(instance->device().realized());
  CHECK_EQ(error[0], '\0');
  CHECK_EQ(module.live(), 1u);
  CHECK_STATUS(instance->device().create_context(3u), Status::ok);
  CHECK_STATUS(module.destroy(instance), Status::ok);
  CHECK_EQ(module.live(), 0u);
  CHECK_EQ(host.live_threads(), 0u);
  CHECK_STATUS(module.destroy(instance), Status::invalid_parameter); }

TEST(device_module_create, a_failed_realize_or_missing_table_leaves_no_device)
{ MockHost host; MockHostOps ops(host);
  DeviceModule module;
  char error[96] = {};
  CHECK(module.create(&ops.table(), "not-a-uuid", error, sizeof error) == nullptr);
  CHECK(std::strstr(error, "is neither auto nor a device UUID") != nullptr);
  CHECK_EQ(module.live(), 0u);
  CHECK(module.create(nullptr, "auto", error, sizeof error) == nullptr);
  CHECK(std::strcmp(error, "virtio-gpu-rvk: no host operations") == 0); }

TEST(device_module_create, one_device_per_slot)
{ MockHost host; MockHostOps ops(host);
  host.add_gpu(gpu);
  DeviceModule module;
  char error[96] = {};
  DeviceInstance *instances[DeviceModule::max_devices] = {};
  for (DeviceInstance *&instance : instances) {
      instance = module.create(&ops.table(), nullptr, error, sizeof error);
      CHECK(instance != nullptr);
  }
  CHECK(module.create(&ops.table(), nullptr, error, sizeof error) == nullptr);
  CHECK(std::strcmp(error, "virtio-gpu-rvk: every device slot is in use") == 0);
  for (DeviceInstance *instance : instances) CHECK_STATUS(module.destroy(instance), Status::ok); }

TEST(rvk_device_abi, create_reset_and_destroy_through_the_c_entry_points)
{ MockHost host; MockHostOps ops(host);
  host.add_gpu(gpu);
  char error[96] = {};
  RvkDevice *const device = rvk_device_create(&ops.table(), "auto", error, sizeof error);
  CHECK(device != nullptr);
  auto *const instance = reinterpret_cast<DeviceInstance *>(device);
  CHECK_STATUS(instance->device().create_context(1u), Status::ok);
  rvk_device_reset(device);
  CHECK_EQ(instance->device().contexts(), 0u);
  CHECK_EQ(av::device_module.live(), 1u);
  CHECK_EQ(rvk_device_destroy(device), 0);
  CHECK_EQ(rvk_device_destroy(device), 1);
  CHECK(rvk_device_create(&ops.table(), "gpu0", error, sizeof error) == nullptr);
  CHECK_EQ(av::device_module.live(), 0u); }
