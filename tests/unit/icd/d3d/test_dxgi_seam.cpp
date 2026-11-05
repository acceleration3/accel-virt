#include "harness/framework/test_registry.hpp"
#include "harness/icd/fake_d3d_runtime.hpp"
#include "harness/icd/umd_rig.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_create_device_record.hpp"
#include "icd/d3d/ddi/dxgi_ddi_args_record.hpp"
#include "icd/d3d/dxgi_base_table.hpp"
#include "icd/d3d/umd_root.hpp"

#include <cstdint>
#include <cstring>

using av::FakeD3dRuntime;
using av::UmdRig;
using av::d3d::RuntimeKind;
using namespace av::d3d;

namespace av::d3d {

struct UmdRootProbe {
    static Hresult adopt(UmdRoot &root, void *adapter, D3d10ddiargCreateDeviceRecord &create, UmdDevice *&made)
    {
        UmdAdapter *const open_adapter = root.adapters_.adapter_of(adapter);
        if (open_adapter == nullptr) return hr_invalid_arg;
        return root.adopt_device(*open_adapter, create, made);
    }
    static void release(UmdRoot &root, void *block) { root.forget_device(block); }
};

}

namespace {
enum Slot : std::uint32_t {
    present = 0u,
    gamma_caps = 1u,
    set_display_mode = 2u,
    resource_priority = 3u,
    resource_residency = 4u,
    rotate_identities = 5u,
    blt = 6u,
    resolve_shared = 7u,
    blt1 = 8u,
    offer_resources1 = 9u,
    reclaim_resources = 10u,
    overlay_caps = 11u,
    overlay_group_caps = 12u,
    reserved1 = 13u,
    present_overlay = 14u,
    reserved2 = 15u,
    present1 = 16u,
    present_duration = 17u,
    trim_residency = 18u,
    overlay_color_space = 19u,
    present_overlay1 = 20u,
    reclaim_resources1 = 21u,
};

constexpr std::uint32_t later_slots[] = {present,   set_display_mode, resource_residency, rotate_identities,
                                         blt,       resolve_shared,   blt1,               reserved1,
                                         present_overlay, reserved2,  present1,           present_overlay1};

class Device {
public:
    Device() : rig_(), runtime_(rig_.root(), RuntimeKind::d3d10_11)
    {
        if (runtime_.open() != hr_ok) return;
        create_.hRTDevice = &rt_device_;
        create_.Interface = FakeD3dRuntime::d3d11_interface;
        create_.Version = FakeD3dRuntime::d3d11_runtime_build;
        create_.pKTCallbacks = &kt_;
        create_.hDrvDevice = block_;
        create_.DXGIBaseDDI.pDXGIBaseCallbacks = &callbacks_;
        create_.DXGIBaseDDI.pDXGIDDIBaseFunctions = &table_;
        create_.hRTCoreLayer = &core_layer_;
        create_.pUMCallbacks = &um_;
    }
    ~Device()
    {
        if (device_ != nullptr) UmdRootProbe::release(rig_.root(), block_);
    }
    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;

    Hresult adopt()
    {
        const Hresult made = UmdRootProbe::adopt(rig_.root(), runtime_.adapter(), create_, device_);
        return made;
    }

    UmdRig                        &rig() { return rig_; }
    D3d10ddiargCreateDeviceRecord &create() { return create_; }
    UmdDevice                     *device() { return device_; }
    void                          *block() { return block_; }
    Dxgi161DdiBaseFunctionsRecord &table() { return table_; }
    DxgiDdiBaseCallbacksRecord    &callbacks() { return callbacks_; }

    template <typename Args>
    Hresult call(std::uint32_t index, Args *args)
    {
        void *const *const slots = reinterpret_cast<void *const *>(&table_);
        if (slots[index] == nullptr) return hr_fail;
        return reinterpret_cast<Hresult (*)(Args *)>(slots[index])(args);
    }

private:
    UmdRig                        rig_;
    FakeD3dRuntime                runtime_;
    D3d10ddiargCreateDeviceRecord create_ = {};
    Dxgi161DdiBaseFunctionsRecord table_ = {};
    DxgiDdiBaseCallbacksRecord    callbacks_ = {};
    UmdDevice                    *device_ = nullptr;
    alignas(16) unsigned char     block_[64] = {};
    int                           rt_device_ = 0;
    int                           core_layer_ = 0;
    int                           kt_ = 0;
    int                           um_ = 0;
};

}

TEST(dxgi_seam, a_published_table_has_no_null_slot)
{ Device device;
  CHECK_EQ(device.adopt(), hr_ok);
  if (device.device() == nullptr) return;
  void *const *const slots = reinterpret_cast<void *const *>(&device.table());
  for (std::uint32_t slot = 0u; slot < DxgiBaseTable::wddm_3_2_slots; slot++) CHECK(slots[slot] != nullptr);
  CHECK(device.device()->dxgi().revision() == DxgiBaseTable::Revision::wddm_3_2);
  CHECK(device.device()->dxgi().invariants_hold()); }

TEST(dxgi_seam, the_callbacks_are_kept_as_the_pointer_the_runtime_gave)
{ Device device;
  CHECK_EQ(device.adopt(), hr_ok);
  if (device.device() == nullptr) return;
  CHECK(device.device()->dxgi().callbacks() == &device.callbacks());
  int moved = 0;
  device.callbacks().pfnPresentCb = &moved;
  CHECK(device.device()->dxgi().callbacks()->pfnPresentCb == &moved); }

TEST(dxgi_seam, a_create_with_no_dxgi_callbacks_or_no_table_publishes_nothing)
{ Device without_callbacks;
  without_callbacks.create().DXGIBaseDDI.pDXGIBaseCallbacks = nullptr;
  CHECK_EQ(without_callbacks.adopt(), hr_invalid_arg);
  CHECK(without_callbacks.device() == nullptr);
  void *const *const slots = reinterpret_cast<void *const *>(&without_callbacks.table());
  CHECK(slots[0] == nullptr);
  CHECK(without_callbacks.rig().root().invariants_hold());

  Device without_table;
  without_table.create().DXGIBaseDDI.pDXGIDDIBaseFunctions = nullptr;
  CHECK_EQ(without_table.adopt(), hr_invalid_arg);
  CHECK(without_table.device() == nullptr);
  CHECK(without_table.rig().root().invariants_hold()); }

TEST(dxgi_seam, an_interface_this_driver_does_not_serve_publishes_nothing)
{ Device device;
  device.create().Interface = FakeD3dRuntime::d3d11_interface - 1u;
  CHECK_EQ(device.adopt(), hr_no_interface);
  void *const *const slots = reinterpret_cast<void *const *>(&device.table());
  for (std::uint32_t slot = 0u; slot < DxgiBaseTable::wddm_3_2_slots; slot++) CHECK(slots[slot] == nullptr);
  CHECK(device.rig().root().invariants_hold()); }

TEST(dxgi_seam, until_later_refuses_a_null_argument_and_otherwise_is_not_implemented)
{ Device device;
  CHECK_EQ(device.adopt(), hr_ok);
  if (device.device() == nullptr) return;
  DxgiDdiArgRecord args = {};
  args.hDevice = device.block();
  CHECK_EQ(device.device()->dxgi().until_later(&args), hr_not_impl);
  CHECK_EQ(device.device()->dxgi().until_later(nullptr), hr_invalid_arg); }

TEST(dxgi_seam, offer_and_reclaim_accept_a_device_and_refuse_a_null_argument)
{ Device device;
  CHECK_EQ(device.adopt(), hr_ok);
  if (device.device() == nullptr) return;
  DxgiDdiArgRecord args = {};
  args.hDevice = device.block();
  CHECK_EQ(device.device()->dxgi().offer_resources(&args), hr_ok);
  CHECK_EQ(device.device()->dxgi().offer_resources(nullptr), hr_invalid_arg);
  DxgiDdiArgReclaimResourcesRecord reclaim = {};
  reclaim.hDevice = device.block();
  CHECK_EQ(device.device()->dxgi().reclaim_resources(&reclaim), hr_ok);
  DxgiDdiArgReclaimResources1Record reclaim1 = {};
  reclaim1.hDevice = device.block();
  CHECK_EQ(device.device()->dxgi().reclaim_resources1(&reclaim1), hr_ok); }

TEST(dxgi_seam, every_slot_a_later_feature_fills_answers_not_implemented)
{ Device device;
  CHECK_EQ(device.adopt(), hr_ok);
  if (device.device() == nullptr) return;
  DxgiDdiArgRecord args = {};
  args.hDevice = device.block();
  for (const std::uint32_t slot : later_slots) {
      CHECK_EQ(device.call(slot, &args), hr_not_impl);
  }
  CHECK(device.rig().root().invariants_hold()); }

TEST(dxgi_seam, the_gamma_slot_answers_no_control_points_without_touching_the_array)
{ Device device;
  CHECK_EQ(device.adopt(), hr_ok);
  struct Caps {
      DxgiGammaControlCapabilitiesRecord header;
      float                              points[8];
  } caps = {};
  for (float &point : caps.points) point = 7.5f;
  caps.header.ScaleAndOffsetSupported = 1;
  caps.header.NumGammaControlPoints = 99u;
  DxgiDdiArgGetGammaControlCapsRecord args = {};
  args.hDevice = device.block();
  args.pGammaCapabilities = &caps;
  CHECK_EQ(device.call(Slot::gamma_caps, &args), hr_ok);
  CHECK_EQ(caps.header.ScaleAndOffsetSupported, 0);
  CHECK_EQ(caps.header.NumGammaControlPoints, 0u);
  for (const float point : caps.points) CHECK(point == 7.5f);

  args.pGammaCapabilities = nullptr;
  CHECK_EQ(device.call(Slot::gamma_caps, &args), hr_invalid_arg); }

TEST(dxgi_seam, the_overlay_slots_answer_no_planes_and_no_groups)
{ Device device;
  CHECK_EQ(device.adopt(), hr_ok);
  DxgiDdiArgGetMultiplaneOverlayCapsRecord planes = {};
  planes.hDevice = device.block();
  planes.MaxPlanes = 4u;
  planes.MaxRGBPlanes = 4u;
  planes.MaxYUVPlanes = 4u;
  planes.MaxStretchFactor = 2.0f;
  planes.MaxShrinkFactor = 2.0f;
  CHECK_EQ(device.call(Slot::overlay_caps, &planes), hr_ok);
  CHECK_EQ(planes.MaxPlanes, 0u);
  CHECK_EQ(planes.MaxRGBPlanes, 0u);
  CHECK_EQ(planes.MaxYUVPlanes, 0u);
  CHECK(planes.MaxStretchFactor == 0.0f);
  CHECK(planes.MaxShrinkFactor == 0.0f);

  DxgiDdiArgGetMultiplaneOverlayGroupCapsRecord groups = {};
  groups.hDevice = device.block();
  groups.MaxPlanes = 3u;
  groups.OverlayCaps = 0xFFu;
  CHECK_EQ(device.call(Slot::overlay_group_caps, &groups), hr_ok);
  CHECK_EQ(groups.MaxPlanes, 0u);
  CHECK_EQ(groups.OverlayCaps, 0u);

  DxgiDdiArgCheckMultiplaneOverlayColorSpaceSupportRecord space = {};
  space.hDevice = device.block();
  space.Supported = 1;
  CHECK_EQ(device.call(Slot::overlay_color_space, &space), hr_ok);
  CHECK_EQ(space.Supported, 0);
  struct Case {
      std::uint32_t format, colour_space;
      std::int32_t  supported;
  };
  const Case cases[] = {{87u, 0u, 1}, {28u, 0u, 1}, {87u, 8u, 0}, {103u, 8u, 1}, {107u, 5u, 1},
                        {103u, 0u, 0}, {103u, 12u, 0}, {2u, 0u, 0}};
  for (const Case &item : cases) {
      space.Format = item.format;
      space.ColorSpace = item.colour_space;
      CHECK_EQ(device.call(Slot::overlay_color_space, &space), hr_ok);
      CHECK_EQ(space.Supported, item.supported);
  } }

TEST(dxgi_seam, the_present_duration_slot_answers_no_custom_duration)
{ Device device;
  CHECK_EQ(device.adopt(), hr_ok);
  DxgiDdiArgCheckPresentDurationSupportRecord args = {};
  args.hDevice = device.block();
  args.DesiredPresentDuration = 166667u;
  args.ClosestSmallerDuration = 1u;
  args.ClosestLargerDuration = 2u;
  CHECK_EQ(device.call(Slot::present_duration, &args), hr_ok);
  CHECK_EQ(args.ClosestSmallerDuration, 0u);
  CHECK_EQ(args.ClosestLargerDuration, 0u);
  CHECK_EQ(args.DesiredPresentDuration, 166667u); }

TEST(dxgi_seam, the_advisory_slots_accept_and_do_nothing)
{ Device device;
  CHECK_EQ(device.adopt(), hr_ok);
  DxgiDdiArgRecord args = {};
  args.hDevice = device.block();
  CHECK_EQ(device.call(Slot::resource_priority, &args), hr_ok);
  CHECK_EQ(device.call(Slot::trim_residency, &args), hr_ok); }

TEST(dxgi_seam, offered_resources_are_reclaimed_with_their_contents)
{ Device device;
  CHECK_EQ(device.adopt(), hr_ok);
  int handles[2] = {};
  DxgiDdiArgRecord offer = {};
  offer.hDevice = device.block();
  CHECK_EQ(device.call(Slot::offer_resources1, &offer), hr_ok);
  std::int32_t discarded[2] = {1, 1};
  DxgiDdiArgReclaimResourcesRecord reclaim = {};
  reclaim.hDevice = device.block();
  reclaim.pResources = handles;
  reclaim.pDiscarded = discarded;
  reclaim.Resources = 2u;
  CHECK_EQ(device.call(Slot::reclaim_resources, &reclaim), hr_ok);
  CHECK_EQ(discarded[0], 0);
  CHECK_EQ(discarded[1], 0);
  reclaim.pDiscarded = nullptr;
  CHECK_EQ(device.call(Slot::reclaim_resources, &reclaim), hr_ok);
  reclaim.pResources = nullptr;
  CHECK_EQ(device.call(Slot::reclaim_resources, &reclaim), hr_invalid_arg);
  std::uint32_t results[2] = {2u, 1u};
  DxgiDdiArgReclaimResources1Record reclaim1 = {};
  reclaim1.hDevice = device.block();
  reclaim1.pResources = handles;
  reclaim1.pResults = results;
  reclaim1.Resources = 2u;
  CHECK_EQ(device.call(Slot::reclaim_resources1, &reclaim1), hr_ok);
  CHECK_EQ(results[0], 0u);
  CHECK_EQ(results[1], 0u);
  reclaim1.pResults = nullptr;
  CHECK_EQ(device.call(Slot::reclaim_resources1, &reclaim1), hr_invalid_arg);
  reclaim1.Resources = 0u;
  CHECK_EQ(device.call(Slot::reclaim_resources1, &reclaim1), hr_ok);
  offer.hDevice = nullptr;
  CHECK_EQ(device.call(Slot::offer_resources1, &offer), hr_invalid_arg);
  reclaim.hDevice = nullptr;
  CHECK_EQ(device.call(Slot::reclaim_resources, &reclaim), hr_invalid_arg);
  reclaim1.hDevice = nullptr;
  CHECK_EQ(device.call(Slot::reclaim_resources1, &reclaim1), hr_invalid_arg); }

TEST(dxgi_seam, a_slot_called_with_a_foreign_device_is_refused)
{ Device device;
  CHECK_EQ(device.adopt(), hr_ok);
  alignas(16) unsigned char foreign[64] = {};
  DxgiDdiArgRecord          args = {};
  args.hDevice = foreign;
  CHECK_EQ(device.call(Slot::present, &args), hr_invalid_arg);
  CHECK_EQ(device.call(Slot::resource_priority, &args), hr_invalid_arg);
  DxgiDdiArgGetGammaControlCapsRecord gamma = {};
  gamma.hDevice = foreign;
  CHECK_EQ(device.call(Slot::gamma_caps, &gamma), hr_invalid_arg);
  args.hDevice = nullptr;
  CHECK_EQ(device.call(Slot::present, &args), hr_invalid_arg);
  CHECK_EQ(device.call(Slot::present, static_cast<DxgiDdiArgRecord *>(nullptr)), hr_invalid_arg); }
