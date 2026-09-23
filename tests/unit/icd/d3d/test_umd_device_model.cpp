/* The device object model (contracts/umd_interface.md, "The device object
 * model"): the runtime owns the block, the driver's pool owns the device, and
 * a cookie in that block is the only thing joining them. Everything here is
 * about what happens when the block is not what the driver wrote - which is
 * the normal case for memory another component can reach. */
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/fake_d3d_runtime.hpp"
#include "harness/icd/umd_rig.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_create_device_record.hpp"
#include "icd/d3d/device_cookie.hpp"
#include "icd/d3d/device_table.hpp"
#include "icd/d3d/umd_root.hpp"

#include <cstdint>
#include <cstring>

using av::FakeD3dRuntime;
using av::UmdRig;
using av::d3d::RuntimeKind;
using namespace av::d3d;

namespace av::d3d {

/* The create without the runtime's answer, so a test can hold a live device
 * and drive the entry points that name one. Feature 056's create is this plus
 * a filled device-functions table (umd_root.hpp). */
struct UmdRootProbe {
    static Hresult adopt(UmdRoot &root, void *adapter, D3d10ddiargCreateDeviceRecord &create, UmdDevice *&made)
    {
        UmdAdapter *const open_adapter = root.adapters_.adapter_of(adapter);
        if (open_adapter == nullptr) return hr_invalid_arg;
        return root.adopt_device(*open_adapter, create, made);
    }
    static void release(UmdRoot &root, void *block) { root.forget_device(block); }
};

}  // namespace av::d3d

namespace {

/* One open adapter with a create the driver will accept, so the tests below
 * can be about the block rather than about opening. */
class Fixture {
public:
    Fixture() : rig_(), runtime_(rig_.root(), RuntimeKind::d3d10_11)
    {
        opened_ = runtime_.open() == hr_ok;
        create_.hRTDevice = &rt_device_;
        create_.Interface = FakeD3dRuntime::d3d11_interface;
        create_.Version = FakeD3dRuntime::d3d11_runtime_build;
        create_.pKTCallbacks = &kt_;
        create_.hDrvDevice = block_;
        create_.DXGIBaseDDI.pDXGIBaseCallbacks = &dxgi_callbacks_;
        create_.DXGIBaseDDI.pDXGIDDIBaseFunctions = &dxgi_table_;
        create_.hRTCoreLayer = &core_layer_;
        create_.pUMCallbacks = &um_;
    }
    Fixture(const Fixture &) = delete;
    Fixture &operator=(const Fixture &) = delete;

    UmdRig                        &rig() { return rig_; }
    FakeD3dRuntime                &runtime() { return runtime_; }
    bool                           opened() const { return opened_; }
    D3d10ddiargCreateDeviceRecord &create() { return create_; }
    void                          *block() { return block_; }
    Dxgi161DdiBaseFunctionsRecord &dxgi_table() { return dxgi_table_; }

private:
    UmdRig                        rig_;
    FakeD3dRuntime                runtime_;
    bool                          opened_ = false;
    D3d10ddiargCreateDeviceRecord create_ = {};
    Dxgi161DdiBaseFunctionsRecord dxgi_table_ = {};
    DxgiDdiBaseCallbacksRecord    dxgi_callbacks_ = {};
    alignas(16) unsigned char     block_[64] = {};
    int                           rt_device_ = 0;
    int                           core_layer_ = 0;
    int                           kt_ = 0;
    int                           um_ = 0;
};

}  // namespace

/* The size is the cookie's whatever the runtime asked with, because a magic
 * and a pointer is what the block holds however the device grows. */
TEST(umd_device_model, the_private_device_size_is_the_cookie_for_every_create)
{ UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(runtime.open(), hr_ok);
  CHECK_EQ(runtime.calc_private_device_size(), sizeof(DeviceCookie));
  CHECK_EQ(DeviceTable::private_bytes, sizeof(DeviceCookie) + sizeof(void *));
  CHECK_EQ(runtime.close_adapter(), hr_ok); }

TEST(umd_device_model, a_created_device_records_the_runtimes_handles_and_both_callback_tables)
{ Fixture fixture;
  CHECK(fixture.opened());
  UmdDevice    *device = nullptr;
  const Hresult adopted =
      UmdRootProbe::adopt(fixture.rig().root(), fixture.runtime().adapter(), fixture.create(), device);
  CHECK_EQ(adopted, hr_ok);
  if (device == nullptr) return;
  CHECK(device->runtime_device() == fixture.create().hRTDevice);
  CHECK(device->runtime_core_layer() == fixture.create().hRTCoreLayer);
  /* Pointers to the runtime's tables, not copies of their rows. */
  CHECK(device->kernel_callbacks() == fixture.create().pKTCallbacks);
  CHECK(device->core_layer_callbacks() == fixture.create().pUMCallbacks);
  CHECK_EQ(device->interface_version(), FakeD3dRuntime::d3d11_interface);
  CHECK(device->invariants_hold());
  CHECK(fixture.rig().root().invariants_hold());
  /* The cookie in the runtime's block names it, and nothing else does. */
  CHECK(fixture.rig().root().device_of(fixture.block()) == device);
  UmdRootProbe::release(fixture.rig().root(), fixture.block());
  CHECK(fixture.rig().root().device_of(fixture.block()) == nullptr);
  CHECK(fixture.rig().root().invariants_hold()); }

/* The count the adapter guards its close with is the table's, so the two must
 * not be able to disagree. */
TEST(umd_device_model, the_tables_count_follows_every_device_made_and_destroyed)
{ Fixture fixture;
  DeviceTable table(fixture.rig().platform(), fixture.rig().log());
  CHECK_EQ(table.live_devices(), 0u);
  CHECK(table.invariants_hold());

  UmdDevice *device = nullptr;
  CHECK_EQ(UmdRootProbe::adopt(fixture.rig().root(), fixture.runtime().adapter(), fixture.create(), device), hr_ok);
  if (device == nullptr) return;
  UmdAdapter &adapter = device->adapter();
  CHECK_EQ(adapter.devices(), 1u);
  UmdRootProbe::release(fixture.rig().root(), fixture.block());
  CHECK_EQ(adapter.devices(), 0u);
  /* A table of its own stays empty throughout: the root's is the one counting. */
  CHECK_EQ(table.live_devices(), 0u);
  CHECK(table.invariants_hold()); }

TEST(umd_device_model, a_block_the_driver_did_not_write_names_no_device)
{ Fixture fixture;
  UmdDevice *device = nullptr;
  CHECK_EQ(UmdRootProbe::adopt(fixture.rig().root(), fixture.runtime().adapter(), fixture.create(), device), hr_ok);

  /* Every way a block can be wrong, in the order a hostile or buggy caller
   * produces them. */
  alignas(16) unsigned char foreign[64] = {};
  CHECK(fixture.rig().root().device_of(foreign) == nullptr);
  CHECK(fixture.rig().root().device_of(nullptr) == nullptr);

  alignas(16) unsigned char scribbled[64] = {};
  std::memcpy(scribbled, fixture.block(), sizeof scribbled);
  scribbled[0] ^= 0xFFu;
  CHECK(fixture.rig().root().device_of(scribbled) == nullptr);

  /* The right magic over a pointer the pool never handed out. */
  alignas(16) unsigned char forged[64] = {};
  std::memcpy(forged, fixture.block(), sizeof forged);
  int elsewhere = 0;
  void *const target = &elsewhere;
  std::memcpy(forged + sizeof(std::uint64_t), &target, sizeof target);
  CHECK(fixture.rig().root().device_of(forged) == nullptr);

  UmdRootProbe::release(fixture.rig().root(), fixture.block());
  /* And a block whose device is gone. */
  CHECK(fixture.rig().root().device_of(fixture.block()) == nullptr);
  CHECK(fixture.rig().root().invariants_hold()); }

TEST(umd_device_model, destroying_twice_is_refused_and_logged_rather_than_repeated)
{ Fixture fixture;
  UmdDevice *device = nullptr;
  CHECK_EQ(UmdRootProbe::adopt(fixture.rig().root(), fixture.runtime().adapter(), fixture.create(), device), hr_ok);
  fixture.rig().root().destroy_device(fixture.block());
  CHECK_EQ(fixture.rig().log().holding("device destroy, status"), 0u);
  fixture.rig().root().destroy_device(fixture.block());
  CHECK_EQ(fixture.rig().log().holding("device destroy, status"), 1u);
  fixture.rig().root().destroy_device(nullptr);
  CHECK_EQ(fixture.rig().log().holding("device destroy, status"), 2u);
  CHECK(fixture.rig().root().invariants_hold()); }

/* Until feature 056 fills the device functions, a device that cannot draw is
 * not offered as one - and the object model built for it is given back rather
 * than leaked, because the runtime never calls DestroyDevice for a create that
 * failed. */
TEST(umd_device_model, the_create_answers_not_implemented_and_leaves_no_device_behind)
{ UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(runtime.open(), hr_ok);
  CHECK_EQ(runtime.create_device(), hr_not_impl);
  CHECK(rig.root().device_of(runtime.device_block()) == nullptr);
  CHECK(rig.root().invariants_hold());
  /* Which means the adapter can still be closed: nothing is counted on it. */
  CHECK_EQ(runtime.close_adapter(), hr_ok);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }

TEST(umd_device_model, the_d3d12_create_makes_no_device_yet)
{ UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d12);
  CHECK_EQ(runtime.open(), hr_ok);
  CHECK_EQ(runtime.create_device(), hr_not_impl);
  CHECK(rig.root().invariants_hold());
  /* pfnDestroyDevice on a block no create wrote is ignored, not dereferenced. */
  runtime.destroy_device();
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(runtime.close_adapter(), hr_ok); }

TEST(umd_device_model, an_adapter_with_a_live_device_is_not_closed)
{ Fixture fixture;
  UmdDevice *device = nullptr;
  CHECK_EQ(UmdRootProbe::adopt(fixture.rig().root(), fixture.runtime().adapter(), fixture.create(), device), hr_ok);
  CHECK_EQ(fixture.runtime().close_adapter(), hr_fail);
  CHECK_EQ(fixture.rig().log().holding("adapter closed with live devices 1"), 1u);
  CHECK(fixture.rig().root().invariants_hold());
  UmdRootProbe::release(fixture.rig().root(), fixture.block());
  CHECK_EQ(fixture.runtime().close_adapter(), hr_ok);
  CHECK_EQ(fixture.rig().kmt().open_objects(), 0u); }

TEST(umd_device_model, a_create_with_no_runtime_handles_is_refused_and_makes_nothing)
{ Fixture fixture;
  fixture.create().hRTDevice = nullptr;
  UmdDevice *device = nullptr;
  CHECK_EQ(UmdRootProbe::adopt(fixture.rig().root(), fixture.runtime().adapter(), fixture.create(), device),
           hr_invalid_arg);
  CHECK(device == nullptr);
  CHECK(fixture.rig().root().device_of(fixture.block()) == nullptr);
  CHECK(fixture.rig().root().invariants_hold());

  Fixture without_callbacks;
  without_callbacks.create().pKTCallbacks = nullptr;
  UmdDevice *second = nullptr;
  CHECK_EQ(UmdRootProbe::adopt(without_callbacks.rig().root(), without_callbacks.runtime().adapter(),
                               without_callbacks.create(), second),
           hr_invalid_arg);
  CHECK(without_callbacks.rig().root().invariants_hold()); }

TEST(umd_device_model, a_create_with_no_block_makes_nothing)
{ Fixture fixture;
  fixture.create().hDrvDevice = nullptr;
  UmdDevice *device = nullptr;
  CHECK_EQ(UmdRootProbe::adopt(fixture.rig().root(), fixture.runtime().adapter(), fixture.create(), device),
           hr_invalid_arg);
  CHECK_EQ(fixture.rig().log().holding("no device, status"), 1u);
  CHECK(fixture.rig().root().invariants_hold()); }

TEST(umd_device_model, a_create_with_no_memory_answers_out_of_memory_and_leaks_nothing)
{ Fixture fixture;
  fixture.rig().platform().fail_allocation_at(1u);
  UmdDevice *device = nullptr;
  CHECK_EQ(UmdRootProbe::adopt(fixture.rig().root(), fixture.runtime().adapter(), fixture.create(), device),
           hr_out_of_memory);
  CHECK(device == nullptr);
  CHECK(fixture.rig().root().invariants_hold());
  fixture.rig().platform().fail_allocation_at(0u);
  CHECK_EQ(fixture.runtime().close_adapter(), hr_ok); }

TEST(umd_device_model, a_state_dump_names_the_device)
{ Fixture fixture;
  UmdDevice *device = nullptr;
  CHECK_EQ(UmdRootProbe::adopt(fixture.rig().root(), fixture.runtime().adapter(), fixture.create(), device), hr_ok);
  char bytes[1024] = {};
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.rig().root().report(writer);
  CHECK(std::strstr(writer.dump(), "umd/0 adapters=1 devices=1") != nullptr);
  CHECK(std::strstr(writer.dump(), "removed=0x0000000000000000") != nullptr);
  UmdRootProbe::release(fixture.rig().root(), fixture.block()); }
