/* The root's Direct3D 12 half, over a host driver that records what it is
 * asked: a create adopts the runtime's block into the root's device table and
 * hands the device on with the kernel adapter the connection holds; a refused
 * create gives the device back; the adapter-level destroy reaches the host
 * driver with the device, so it knows which runtime's device it tears down;
 * every table but DXGI's is the host driver's to fill; and a caps question
 * reaches it naming the D3D12 adapter it was asked on. */
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/fake_d3d_runtime.hpp"
#include "harness/icd/recording_host_driver.hpp"
#include "harness/icd/umd_rig.hpp"
#include "icd/d3d/ddi/d3d12ddiarg_create_device_record.hpp"
#include "icd/d3d/hresult.hpp"
#include "icd/d3d/runtime_kind.hpp"
#include "icd/d3d/umd_host_driver.hpp"
#include "icd/d3d/umd_root.hpp"

#include <cstdint>

using av::AdapterScenario;
using av::AdapterSearch;
using av::AvgpuAdapters;
using av::FakeD3dRuntime;
using av::PciIdentity;
using av::Status;
using av::RecordingHostDriver;
using av::UmdRig;
using av::d3d::RuntimeKind;
using av::d3d::hr_fail;
using av::d3d::hr_invalid_arg;
using av::d3d::hr_not_impl;
using av::d3d::hr_ok;

namespace {

constexpr std::uint32_t table_device = 0u;
constexpr std::uint32_t table_list = 1u;
constexpr std::uint32_t table_dxgi = 3u;
constexpr std::uint32_t table_extended = 27u;
/* D3D12DDICAPS_TYPE_0081_3DPIPELINESUPPORT1: D3D12Core's first question. */
constexpr std::uint32_t caps_pipeline_level = 1074u;

}  // namespace

TEST(umd_root_d3d12, a_create_adopts_the_block_and_hands_the_device_to_the_host_driver)
{ RecordingHostDriver host;
  UmdRig rig(&host);
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d12);
  CHECK_EQ(runtime.open(), hr_ok);
  CHECK_EQ(runtime.create_device(), hr_ok);
  CHECK_EQ(host.builds12(), 1u);
  CHECK_EQ(host.builds11(), 0u);
  CHECK(host.built_block() == runtime.device_block());
  CHECK(host.kmt_adapter() != 0u);
  CHECK(rig.root().device_of(runtime.device_block()) != nullptr);
  CHECK(rig.root().invariants_hold());
  /* D3D12's destroy is the adapter's slot: the host driver is told whose
   * device it is, then the root forgets it. */
  runtime.destroy_device();
  CHECK_EQ(host.tear_downs(), 1u);
  CHECK(host.torn_block() == runtime.device_block());
  CHECK(host.torn_runtime() == RuntimeKind::d3d12);
  CHECK(rig.root().device_of(runtime.device_block()) == nullptr);
  CHECK_EQ(runtime.close_adapter(), hr_ok);
  CHECK(rig.root().invariants_hold()); }

/* A guest has one avgpu adapter per host GPU, and an application choosing
 * among them opens more than one. A device made on the second reaches the
 * second's kernel adapter: the runtime makes that device's allocations there,
 * and a device that reached the first's would share them on the wrong host
 * GPU, under keys the first's kernel adapter already gave out. */
TEST(umd_root_d3d12, a_device_is_built_on_the_kernel_adapter_its_runtime_opened)
{ constexpr std::uint64_t second_luid = 0x0000000300005678ull;
  const PciIdentity avgpu(AvgpuAdapters::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                           AvgpuAdapters::virtio_vendor, AvgpuAdapters::av_subsystem);
  RecordingHostDriver host;
  UmdRig rig(&host);
  rig.kmt().add_adapter(avgpu, AdapterScenario::wddm_3_2);
  rig.kmt().set_adapter_luid(1u, second_luid);
  FakeD3dRuntime first(rig.root(), RuntimeKind::d3d12);
  FakeD3dRuntime second(rig.root(), RuntimeKind::d3d12);
  first.answer_luid(UmdRig::adapter_luid);
  second.answer_luid(second_luid);
  CHECK_EQ(first.open(), hr_ok);
  CHECK_EQ(second.open(), hr_ok);
  std::uint64_t luid = 0u;
  CHECK_EQ(second.create_device(), hr_ok);
  CHECK_STATUS(rig.kmt().adapter_luid(host.kmt_adapter(), luid), Status::ok);
  CHECK_EQ(luid, second_luid);
  CHECK_EQ(first.create_device(), hr_ok);
  CHECK_STATUS(rig.kmt().adapter_luid(host.kmt_adapter(), luid), Status::ok);
  CHECK_EQ(luid, UmdRig::adapter_luid);
  CHECK(rig.root().invariants_hold());
  first.destroy_device();
  second.destroy_device();
  CHECK_EQ(first.close_adapter(), hr_ok);
  CHECK_EQ(second.close_adapter(), hr_ok);
  CHECK(rig.root().invariants_hold()); }

TEST(umd_root_d3d12, a_create_the_host_driver_refuses_leaves_no_device_and_no_tear_down)
{ RecordingHostDriver host;
  host.answer(hr_fail, hr_ok, hr_ok);
  UmdRig rig(&host);
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d12);
  CHECK_EQ(runtime.open(), hr_ok);
  CHECK_EQ(runtime.create_device(), hr_fail);
  CHECK_EQ(host.builds12(), 1u);
  CHECK(rig.root().device_of(runtime.device_block()) == nullptr);
  /* The adapter closes: nothing the refused create made is left on it. */
  CHECK_EQ(runtime.close_adapter(), hr_ok);
  runtime.destroy_device();
  CHECK_EQ(host.tear_downs(), 0u);
  CHECK(rig.root().invariants_hold()); }

TEST(umd_root_d3d12, the_device_tables_are_the_host_drivers_and_dxgis_is_the_roots)
{ RecordingHostDriver host;
  UmdRig rig(&host);
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d12);
  CHECK_EQ(runtime.open(), hr_ok);
  CHECK_EQ(runtime.fill_ddi_table(table_device, 1000u), hr_ok);
  CHECK_EQ(host.table_type(), table_device);
  CHECK_EQ(host.table_bytes(), 1000u);
  CHECK_EQ(runtime.fill_ddi_table(table_extended), hr_ok);
  CHECK_EQ(host.table_type(), table_extended);
  /* DXGI's never reaches the host driver. */
  CHECK_EQ(runtime.fill_ddi_table(table_dxgi), hr_ok);
  CHECK_EQ(host.table_type(), table_extended);
  CHECK_EQ(runtime.filled_slots(), 22u);
  /* A refusal is the host driver's answer, logged with its type. */
  host.answer(hr_ok, hr_ok, hr_not_impl);
  CHECK_EQ(runtime.fill_ddi_table(table_list), hr_not_impl);
  CHECK_EQ(rig.log().holding("ddi table refused, type 1"), 1u);
  CHECK_EQ(runtime.close_adapter(), hr_ok); }

TEST(umd_root_d3d12, a_caps_question_reaches_the_host_driver_naming_the_d3d12_adapter)
{ RecordingHostDriver host;
  host.answer(hr_ok, hr_invalid_arg, hr_ok);
  UmdRig rig(&host);
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d12);
  CHECK_EQ(runtime.open(), hr_ok);
  std::uint32_t levels[2] = {14u, 0u};
  CHECK_EQ(runtime.caps(caps_pipeline_level, levels, sizeof levels), hr_invalid_arg);
  CHECK(host.caps_runtime() == RuntimeKind::d3d12);
  CHECK_EQ(host.caps_type(), caps_pipeline_level);
  CHECK(host.kmt_adapter() != 0u);
  CHECK_EQ(runtime.close_adapter(), hr_ok); }

/* D3D12Core's create record, as the root reads it: the runtime's block is
 * where the device lives, and a record naming no block, or an adapter this
 * root never opened, is refused before the host driver is asked. */
TEST(umd_root_d3d12, a_create_record_without_a_block_or_on_no_adapter_is_refused_unasked)
{ RecordingHostDriver host;
  av::d3d::UmdHostDriver *const builder = &host;
  UmdRig rig(builder);
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d12);
  CHECK_EQ(runtime.open(), hr_ok);
  alignas(16) static std::uint8_t block[4096];
  av::d3d::D3d12ddiargCreateDeviceRecord create = {};
  CHECK_EQ(rig.root().create_device12(runtime.adapter(), nullptr), hr_invalid_arg);
  CHECK_EQ(rig.root().create_device12(runtime.adapter(), &create), hr_invalid_arg);
  create.hDrvDevice = block;
  int stranger = 0;
  CHECK_EQ(rig.root().create_device12(&stranger, &create), hr_invalid_arg);
  CHECK_EQ(host.builds12(), 0u);
  /* The same record on the open adapter reaches the host driver's build12. */
  CHECK_EQ(rig.root().create_device12(runtime.adapter(), &create), hr_ok);
  CHECK_EQ(host.builds12(), 1u);
  CHECK(host.built_block() == block);
  CHECK(rig.root().device_of(block) != nullptr);
  rig.root().destroy_device(block);
  CHECK(rig.root().device_of(block) == nullptr);
  CHECK_EQ(runtime.close_adapter(), hr_ok);
  CHECK(rig.root().invariants_hold()); }
