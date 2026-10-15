/* What each of the driver's classes says is impossible, made to happen
 * (constitution IV.2: an invariant that can never return false is not an
 * invariant). Every probe below reaches into one class and breaks exactly the
 * rule its invariant states, so a rule deleted from the invariant fails here.
 *
 * The refusal paths that need the kernel to fail are here too, because they
 * are the same kind of thing: a branch a caller can reach and a test must. */
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/fake_d3d_runtime.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/umd_rig.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/d3d/adapter_table.hpp"
#include "icd/d3d/ddi/d3d10ddiarg_create_device_record.hpp"
#include "icd/d3d/ddi/d3dddi_adapter_callbacks_record.hpp"
#include "icd/d3d/ddi/dxgi_ddi_args_record.hpp"
#include "icd/d3d/ddi_version_list.hpp"
#include "icd/d3d/device_cookie.hpp"
#include "icd/d3d/device_removal.hpp"
#include "icd/d3d/device_table.hpp"
#include "icd/d3d/umd_connection.hpp"
#include "icd/d3d/umd_connections.hpp"
#include "icd/d3d/umd_root.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::AvgpuAdapters;
using av::FakeD3dRuntime;
using av::MockKmt;
using av::MockPlatform;
using av::PciIdentity;
using av::RecordingLog;
using av::Status;
using av::UmdRig;
using namespace av::d3d;

namespace av::d3d {

struct DdiVersionListProbe {
    static void empty(DdiVersionList &list) { list.count_ = 0u; }
    /* Three words whose first two differ, so the repeat is found on the second
     * pass rather than the first. */
    static void repeat_the_first_word_third(DdiVersionList &list)
    {
        list.words_[1] = list.words_[0] + 1ull;
        list.words_[2] = list.words_[0];
        list.count_ = 3u;
    }
    static void overfull(DdiVersionList &list) { list.count_ = DdiVersionList::max_words + 1u; }
    static void zero_word(DdiVersionList &list) { list.words_[0] = 0ull; }
    static void repeat(DdiVersionList &list)
    {
        list.words_[1] = list.words_[0];
        list.count_ = 2u;
    }
};

struct UmdAdapterProbe {
    static void forget_a_device(UmdAdapter &adapter) { adapter.devices_ = 0u; }
    static void forget_the_callbacks(UmdAdapter &adapter) { adapter.callbacks_ = nullptr; }
    static void break_its_versions(UmdAdapter &adapter) { DdiVersionListProbe::empty(adapter.versions_); }
    static void break_its_identity(UmdAdapter &adapter);
};

struct UmdDeviceProbe {
    static void forget_the_callbacks(UmdDevice &device) { device.kernel_callbacks_ = nullptr; }
    static void break_its_removal(UmdDevice &device);
    static void unpublish_its_dxgi(UmdDevice &device);
    static void forget_the_runtime_device(UmdDevice &device) { device.runtime_device_ = nullptr; }
    static void remember_a_version_it_never_adopted(UmdDevice &device) { device.interface_version_ = 7u; }
    static void remember_callbacks_it_never_adopted(UmdDevice &device)
    {
        device.kernel_callbacks_ = &device;
    }
};

struct DxgiBaseTableProbe {
    static void keep_callbacks_without_a_revision(DxgiBaseTable &table, const DxgiDdiBaseCallbacksRecord *callbacks)
    {
        table.callbacks_ = callbacks;
    }
    static void forget_the_revision(DxgiBaseTable &table) { table.revision_ = DxgiBaseTable::Revision::none; }
};

inline void UmdDeviceProbe::unpublish_its_dxgi(UmdDevice &device)
{
    DxgiBaseTableProbe::forget_the_revision(device.dxgi_);
}

struct UmdConnectionProbe {
    static void count_an_open_it_does_not_hold(UmdConnection &connection) { connection.opens_ = 1u; }
    static void count_one_more_than_it_holds(UmdConnection &connection) { connection.opens_++; }
};

struct UmdConnectionsProbe {
    static UmdConnection &first(UmdConnections &connections) { return connections.connections_[0].object(); }
    /* A second connection opened on the adapter the first holds: what acquire
     * never does. */
    static Status open_the_first_twice(UmdConnections &connections, std::uint64_t luid)
    {
        static_cast<void>(connections.connections_[0].object().acquire(luid));
        return connections.connections_[1].object().acquire(luid);
    }
};

struct UmdRootProbe {
    static UmdConnection &connection(UmdRoot &root) { return UmdConnectionsProbe::first(root.connections_); }
    static DeviceTable   &devices(UmdRoot &root) { return root.devices_; }
    static Hresult adopt(UmdRoot &root, void *adapter, D3d10ddiargCreateDeviceRecord &create, UmdDevice *&made)
    {
        UmdAdapter *const open_adapter = root.adapters_.adapter_of(adapter);
        if (open_adapter == nullptr) return hr_invalid_arg;
        return root.adopt_device(*open_adapter, create, made);
    }
    static void      release(UmdRoot &root, void *block) { root.forget_device(block); }
    static UmdAdapter *adapter_of(UmdRoot &root, void *handle) { return root.adapters_.adapter_of(handle); }
};

}  // namespace av::d3d

namespace {

constexpr std::uint32_t d3d11_interface = (11u << 16) | 45u;

const PciIdentity avgpu(AvgpuAdapters::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                         AvgpuAdapters::virtio_vendor, AvgpuAdapters::av_subsystem);

D3dddiAdapterCallbacksRecord callbacks;
int                          runtime_handle;
/* What an adapter made outside a root is opened over: none of these judge it. */
MockKmt                      no_kernel;
UmdConnection                unopened(no_kernel);

}  // namespace

TEST(ddi_version_list_invariants, fail_for_no_word_too_many_a_zero_and_a_repeat)
{ DdiVersionList empty(RuntimeKind::d3d10_11);
  DdiVersionListProbe::empty(empty);
  CHECK(!empty.invariants_hold());

  DdiVersionList overfull(RuntimeKind::d3d10_11);
  DdiVersionListProbe::overfull(overfull);
  CHECK(!overfull.invariants_hold());

  DdiVersionList zeroed(RuntimeKind::d3d10_11);
  DdiVersionListProbe::zero_word(zeroed);
  CHECK(!zeroed.invariants_hold());

  DdiVersionList repeated(RuntimeKind::d3d10_11);
  DdiVersionListProbe::repeat(repeated);
  CHECK(!repeated.invariants_hold()); }

TEST(device_cookie, is_written_read_and_cleared_and_refuses_a_foreign_magic)
{ DeviceCookie cookie;
  CHECK(cookie.invariants_hold());
  CHECK(cookie.named() == nullptr);

  UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(runtime.open(), hr_ok);
  /* Any address will do: the cookie records what it is given and DeviceTable
   * is what decides whether the pool still holds a device there. */
  alignas(UmdDevice) static unsigned char block[sizeof(UmdDevice)];
  UmdDevice *const somewhere = reinterpret_cast<UmdDevice *>(block);
  cookie.write(*somewhere);
  CHECK(cookie.invariants_hold());
  CHECK(cookie.named() == somewhere);

  /* A block another component scribbled on: the magic no longer this
   * driver's, so it names nothing however good the pointer looks. */
  std::uint64_t bytes[2] = {};
  std::memcpy(bytes, &cookie, sizeof bytes);
  bytes[0] ^= 1ull;
  DeviceCookie scribbled;
  std::memcpy(&scribbled, bytes, sizeof scribbled);
  CHECK(scribbled.named() == nullptr);
  /* Wrong magic with a pointer is not an invariant break - it is what foreign
   * memory looks like. */
  CHECK(scribbled.invariants_hold());

  cookie.clear();
  CHECK(cookie.named() == nullptr);
  CHECK(cookie.invariants_hold());
  CHECK_EQ(runtime.close_adapter(), hr_ok); }

TEST(device_cookie_invariants, fail_for_this_drivers_magic_over_no_device)
{ DeviceCookie cookie;
  const std::uint64_t magic = DeviceCookie::magic;
  std::uint64_t       bytes[2] = {magic, 0ull};
  std::memcpy(&cookie, bytes, sizeof cookie);
  CHECK(!cookie.invariants_hold()); }

/* device_gone is what CloseAdapter's refusal is built on, so an adapter that
 * lost count of a device it holds would let the adapter go under it. */
TEST(umd_adapter, forgetting_a_device_lets_an_adapter_close_that_should_not)
{ MockPlatform platform;
  AdapterTable table(platform);
  UmdAdapter  *adapter = nullptr;
  CHECK_STATUS(table.open(adapter, RuntimeKind::d3d10_11, &runtime_handle, &callbacks, unopened, d3d11_interface, 0u),
               Status::ok);
  if (adapter == nullptr) return;
  adapter->device_made();
  CHECK_STATUS(table.close(adapter), Status::conflict);
  UmdAdapterProbe::forget_a_device(*adapter);
  CHECK_STATUS(table.close(adapter), Status::ok);
  CHECK_EQ(table.open_adapters(), 0u); }

TEST(umd_connection_invariants, fail_for_an_open_counted_over_no_adapter)
{ MockKmt kmt;
  UmdConnection connection(kmt);
  UmdConnectionProbe::count_an_open_it_does_not_hold(connection);
  CHECK(!connection.invariants_hold()); }

/* AdapterSearch answers not_found when no adapter matches and its own failure
 * otherwise; the connection turns only the first into not_supported, because
 * an enumeration the kernel refused is not "this adapter is absent". */
TEST(umd_connection, an_enumeration_the_kernel_refuses_is_not_a_missing_adapter)
{ MockKmt kmt;
  kmt.add_adapter(avgpu, AdapterScenario::wddm_3_2);
  kmt.fail_at(MockKmt::Call::enumerate_adapters, 1u, MockKmt::invalid_parameter);
  UmdConnection connection(kmt);
  const Status asked = connection.acquire(0u);
  CHECK(asked != Status::ok);
  CHECK(asked != Status::not_supported);
  CHECK(!connection.open());
  CHECK(connection.invariants_hold()); }

TEST(device_removal_invariants, fail_for_a_cause_counted_on_a_live_device)
{ RecordingLog  log;
  DeviceRemoval removal;
  removal.remove(hr_dxgi_device_reset);
  CHECK_EQ(removal.check(log, "pfnBlt"), hr_dxgi_device_reset);
  CHECK(removal.invariants_hold());
  /* A removal that forgot its reason but kept the count and the report is a
   * device answering hr_ok after it was gone. */
  DeviceRemoval blank;
  CHECK(blank.invariants_hold()); }

namespace av::d3d {

/* Reopened so the probe below can blank a reason the class never blanks. */
struct DeviceRemovalProbe {
    static void forget_the_reason(DeviceRemoval &removal) { removal.reason_ = hr_ok; }
    static void report_then_forget(DeviceRemoval &removal)
    {
        removal.reason_ = hr_dxgi_device_removed;
        av::RecordingLog log;
        static_cast<void>(removal.check(log, "pfnPresent"));
        removal.reason_ = hr_ok;
    }
};

}  // namespace av::d3d

TEST(device_removal_invariants, fail_when_a_reported_removal_has_no_reason)
{ RecordingLog  log;
  DeviceRemoval removal;
  removal.remove(hr_dxgi_device_removed);
  removal.remove(hr_dxgi_device_hung);
  CHECK_EQ(removal.check(log, "pfnPresent"), hr_dxgi_device_removed);
  DeviceRemovalProbe::forget_the_reason(removal);
  CHECK(!removal.invariants_hold()); }

namespace {

/* One adopted device, so the probes below have one to corrupt. */
class Adopted {
public:
    Adopted() : rig_(), runtime_(rig_.root(), RuntimeKind::d3d10_11)
    {
        if (runtime_.open() != hr_ok) return;
        create_.hRTDevice = &rt_device_;
        create_.Interface = d3d11_interface;
        create_.pKTCallbacks = &kt_;
        create_.hDrvDevice = block_;
        create_.DXGIBaseDDI.pDXGIBaseCallbacks = &dxgi_callbacks_;
        create_.DXGIBaseDDI.pDXGIDDIBaseFunctions = &table_;
        create_.hRTCoreLayer = &core_layer_;
        create_.pUMCallbacks = &um_;
        made_ = UmdRootProbe::adopt(rig_.root(), runtime_.adapter(), create_, device_) == hr_ok;
    }
    ~Adopted()
    {
        if (made_) UmdRootProbe::release(rig_.root(), block_);
    }
    Adopted(const Adopted &) = delete;
    Adopted &operator=(const Adopted &) = delete;

    bool                           made() const { return made_; }
    UmdRig                        &rig() { return rig_; }
    UmdDevice                     &device() { return *device_; }
    void                          *block() { return block_; }
    DxgiDdiBaseCallbacksRecord    &dxgi_callbacks() { return dxgi_callbacks_; }
    Dxgi161DdiBaseFunctionsRecord &table() { return table_; }

    template <typename Args>
    Hresult call(std::uint32_t index, Args *args)
    {
        void *const *const slots = reinterpret_cast<void *const *>(&table_);
        return reinterpret_cast<Hresult (*)(Args *)>(slots[index])(args);
    }

private:
    UmdRig                        rig_;
    FakeD3dRuntime                runtime_;
    D3d10ddiargCreateDeviceRecord create_ = {};
    Dxgi161DdiBaseFunctionsRecord table_ = {};
    DxgiDdiBaseCallbacksRecord    dxgi_callbacks_ = {};
    UmdDevice                    *device_ = nullptr;
    bool                          made_ = false;
    alignas(16) unsigned char     block_[64] = {};
    int                           rt_device_ = 0;
    int                           core_layer_ = 0;
    int                           kt_ = 0;
    int                           um_ = 0;
};

/* The fifteen slots whose thunk takes the plain argument view. */
constexpr std::uint32_t plain_slots[] = {0u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 13u, 14u, 15u, 16u, 18u, 20u};

}  // namespace

TEST(umd_device_invariants, fail_for_an_adopted_device_missing_what_the_adopt_recorded)
{ Adopted adopted;
  CHECK(adopted.made());
  if (!adopted.made()) return;
  CHECK(adopted.device().invariants_hold());
  UmdDeviceProbe::forget_the_callbacks(adopted.device());
  CHECK(!adopted.device().invariants_hold()); }

TEST(umd_device_invariants, fail_for_a_device_that_never_adopted_but_remembers_one)
{ UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(runtime.open(), hr_ok);
  UmdAdapter *const adapter = UmdRootProbe::adapter_of(rig.root(), runtime.adapter());
  CHECK(adapter != nullptr);
  if (adapter == nullptr) return;

  UmdDevice fresh(*adapter, rig.log());
  CHECK(fresh.invariants_hold());
  UmdDeviceProbe::remember_a_version_it_never_adopted(fresh);
  CHECK(!fresh.invariants_hold());

  UmdDevice second(*adapter, rig.log());
  UmdDeviceProbe::remember_callbacks_it_never_adopted(second);
  CHECK(!second.invariants_hold());
  CHECK_EQ(runtime.close_adapter(), hr_ok); }

TEST(dxgi_base_table_invariants, fail_for_callbacks_kept_without_a_revision)
{ Adopted adopted;
  if (!adopted.made()) return;
  UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(runtime.open(), hr_ok);
  UmdAdapter *const adapter = UmdRootProbe::adapter_of(rig.root(), runtime.adapter());
  if (adapter == nullptr) return;
  UmdDevice unpublished(*adapter, rig.log());
  CHECK(unpublished.dxgi().invariants_hold());
  DxgiBaseTableProbe::keep_callbacks_without_a_revision(unpublished.dxgi(), &adopted.dxgi_callbacks());
  CHECK(!unpublished.dxgi().invariants_hold());
  DxgiBaseTableProbe::keep_callbacks_without_a_revision(unpublished.dxgi(), nullptr);
  CHECK_EQ(runtime.close_adapter(), hr_ok); }

/* Every slot, not the three a spot check would reach: a thunk that followed a
 * device handle it was never given would be the one bug that matters most. */
TEST(dxgi_seam, every_slot_refuses_a_device_handle_this_driver_never_gave)
{ Adopted adopted;
  if (!adopted.made()) return;
  alignas(16) unsigned char foreign[64] = {};

  DxgiDdiArgRecord plain = {};
  plain.hDevice = foreign;
  for (const std::uint32_t slot : plain_slots) CHECK_EQ(adopted.call(slot, &plain), hr_invalid_arg);

  DxgiDdiArgGetGammaControlCapsRecord gamma = {};
  gamma.hDevice = foreign;
  CHECK_EQ(adopted.call(1u, &gamma), hr_invalid_arg);
  DxgiDdiArgGetMultiplaneOverlayCapsRecord planes = {};
  planes.hDevice = foreign;
  CHECK_EQ(adopted.call(11u, &planes), hr_invalid_arg);
  DxgiDdiArgGetMultiplaneOverlayGroupCapsRecord groups = {};
  groups.hDevice = foreign;
  CHECK_EQ(adopted.call(12u, &groups), hr_invalid_arg);
  DxgiDdiArgCheckPresentDurationSupportRecord duration = {};
  duration.hDevice = foreign;
  CHECK_EQ(adopted.call(17u, &duration), hr_invalid_arg);
  DxgiDdiArgCheckMultiplaneOverlayColorSpaceSupportRecord space = {};
  space.hDevice = foreign;
  CHECK_EQ(adopted.call(19u, &space), hr_invalid_arg);
  DxgiDdiArgReclaimResourcesRecord reclaim = {};
  reclaim.hDevice = foreign;
  CHECK_EQ(adopted.call(10u, &reclaim), hr_invalid_arg);
  DxgiDdiArgReclaimResources1Record reclaim1 = {};
  reclaim1.hDevice = foreign;
  CHECK_EQ(adopted.call(21u, &reclaim1), hr_invalid_arg);

  /* And a null argument, which is what a thunk sees if the runtime is the one
   * that is broken. */
  CHECK_EQ(adopted.call(0u, static_cast<DxgiDdiArgRecord *>(nullptr)), hr_invalid_arg);
  CHECK_EQ(adopted.call(1u, static_cast<DxgiDdiArgGetGammaControlCapsRecord *>(nullptr)), hr_invalid_arg);
  CHECK_EQ(adopted.call(3u, static_cast<DxgiDdiArgRecord *>(nullptr)), hr_invalid_arg);
  CHECK_EQ(adopted.call(11u, static_cast<DxgiDdiArgGetMultiplaneOverlayCapsRecord *>(nullptr)), hr_invalid_arg);
  CHECK_EQ(adopted.call(12u, static_cast<DxgiDdiArgGetMultiplaneOverlayGroupCapsRecord *>(nullptr)),
           hr_invalid_arg);
  CHECK_EQ(adopted.call(17u, static_cast<DxgiDdiArgCheckPresentDurationSupportRecord *>(nullptr)), hr_invalid_arg);
  CHECK_EQ(adopted.call(18u, static_cast<DxgiDdiArgRecord *>(nullptr)), hr_invalid_arg);
  CHECK_EQ(adopted.call(19u, static_cast<DxgiDdiArgCheckMultiplaneOverlayColorSpaceSupportRecord *>(nullptr)),
           hr_invalid_arg);
  CHECK_EQ(adopted.call(10u, static_cast<DxgiDdiArgReclaimResourcesRecord *>(nullptr)), hr_invalid_arg);
  CHECK_EQ(adopted.call(21u, static_cast<DxgiDdiArgReclaimResources1Record *>(nullptr)), hr_invalid_arg); }

/* Each capability slot on a device that is gone, because each answers the
 * removal before it writes anything into the runtime's buffer. */
TEST(dxgi_seam, every_capability_slot_answers_the_removal_before_it_writes)
{ Adopted adopted;
  if (!adopted.made()) return;
  adopted.device().removal().remove(hr_dxgi_device_removed);

  DxgiDdiArgGetMultiplaneOverlayCapsRecord planes = {};
  planes.hDevice = adopted.block();
  planes.MaxPlanes = 4u;
  CHECK_EQ(adopted.call(11u, &planes), hr_dxgi_device_removed);
  CHECK_EQ(planes.MaxPlanes, 4u);

  DxgiDdiArgGetMultiplaneOverlayGroupCapsRecord groups = {};
  groups.hDevice = adopted.block();
  groups.MaxPlanes = 3u;
  CHECK_EQ(adopted.call(12u, &groups), hr_dxgi_device_removed);
  CHECK_EQ(groups.MaxPlanes, 3u);

  DxgiDdiArgCheckPresentDurationSupportRecord duration = {};
  duration.hDevice = adopted.block();
  duration.ClosestLargerDuration = 5u;
  CHECK_EQ(adopted.call(17u, &duration), hr_dxgi_device_removed);
  CHECK_EQ(duration.ClosestLargerDuration, 5u);

  DxgiDdiArgCheckMultiplaneOverlayColorSpaceSupportRecord space = {};
  space.hDevice = adopted.block();
  space.Supported = 1;
  CHECK_EQ(adopted.call(19u, &space), hr_dxgi_device_removed);
  CHECK_EQ(space.Supported, 1); }

/* The refusals the root logs but cannot return, because the DDI slot they are
 * in has no way to say so. */
TEST(umd_root_open, a_kernel_that_refuses_the_adapter_close_is_logged_not_lost)
{ UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(runtime.open(), hr_ok);
  rig.kmt().fail_at(MockKmt::Call::close_adapter, 1u, MockKmt::invalid_parameter);
  CHECK_EQ(runtime.close_adapter(), hr_ok);
  CHECK_EQ(rig.log().holding("connection release, status"), 1u);
  CHECK(rig.root().invariants_hold()); }

/* The same refusal on the give-back path of a failed open: the adapter was
 * found, the identity was not, and what was made is handed back. */
TEST(umd_root_open, a_refused_identity_gives_the_adapter_back_and_logs_a_refused_close)
{ UmdRig rig;
  /* The identity's ask, after the one listing the adapters. */
  rig.kmt().fail_at(MockKmt::Call::adapter_luid, 2u, MockKmt::invalid_parameter);
  rig.kmt().fail_at(MockKmt::Call::close_adapter, 1u, MockKmt::invalid_parameter);
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(runtime.open(), hr_fail);
  CHECK_EQ(rig.log().holding("connection release, status"), 1u);
  CHECK(rig.root().invariants_hold()); }

TEST(umd_root_open, the_d3d12_open_refuses_a_null_table_and_a_missing_adapter)
{ UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d12);
  runtime.offer_no_table();
  CHECK_EQ(runtime.open(), hr_invalid_arg);

  UmdRig empty;
  empty.kmt().claim_adapters(0u);
  FakeD3dRuntime nothing(empty.root(), RuntimeKind::d3d12);
  CHECK_EQ(nothing.open(), hr_fail);
  CHECK(empty.root().invariants_hold());

  UmdRig starved;
  starved.platform().fail_allocation_at(1u);
  FakeD3dRuntime hungry(starved.root(), RuntimeKind::d3d12);
  CHECK_EQ(hungry.open(), hr_out_of_memory);
  CHECK_EQ(starved.platform().live_allocations(), 0u);
  CHECK(starved.root().invariants_hold()); }

TEST(umd_root_open, the_caps_and_create_slots_refuse_a_null_argument)
{ UmdRig rig;
  FakeD3dRuntime eleven(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(eleven.open(), hr_ok);
  CHECK_EQ(rig.root().caps(eleven.adapter(), nullptr), hr_invalid_arg);
  CHECK_EQ(rig.root().create_device(eleven.adapter(), nullptr), hr_invalid_arg);
  CHECK_EQ(rig.root().optional_ddi_tables(eleven.adapter(), nullptr, nullptr), hr_invalid_arg);
  CHECK_EQ(rig.root().create_device12(eleven.adapter(), nullptr), hr_invalid_arg);
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(eleven.close_adapter(), hr_ok); }

namespace av::d3d {

/* The rest of the corruptions the composite invariants above are built from:
 * each one breaks a class its owner only asks, so a rule dropped from the
 * owner's invariant stops being noticed. */
struct AdapterIdentityProbe {
    static void ids_without_a_luid(AdapterIdentity &identity)
    {
        identity.luid_ = 0ull;
        identity.vendor_id_ = 1u;
    }
    static void a_luid_without_a_description(AdapterIdentity &identity)
    {
        identity.luid_ = 1ull;
        identity.description_[0] = '\0';
    }
    static void a_device_id_without_a_luid(AdapterIdentity &identity)
    {
        identity.luid_ = 0ull;
        identity.device_id_ = 1u;
    }
    static void a_description_without_a_luid(AdapterIdentity &identity)
    {
        identity.luid_ = 0ull;
        identity.vendor_id_ = 0u;
        identity.device_id_ = 0u;
    }
    static void a_description_that_never_ends(AdapterIdentity &identity)
    {
        identity.luid_ = 1ull;
        for (char &byte : identity.description_) byte = 'x';
    }
};

inline void UmdAdapterProbe::break_its_identity(UmdAdapter &adapter)
{
    AdapterIdentityProbe::ids_without_a_luid(adapter.identity_);
}

inline void UmdDeviceProbe::break_its_removal(UmdDevice &device)
{
    DeviceRemovalProbe::report_then_forget(device.removal_);
}

}  // namespace av::d3d

TEST(ddi_version_list_invariants, fail_for_a_repeat_that_is_not_the_first_pair)
{ DdiVersionList list(RuntimeKind::d3d10_11);
  DdiVersionListProbe::repeat_the_first_word_third(list);
  CHECK(!list.invariants_hold()); }

TEST(device_removal_invariants, fail_for_a_removal_reported_and_then_forgotten)
{ DeviceRemoval removal;
  DeviceRemovalProbe::report_then_forget(removal);
  CHECK(!removal.invariants_hold()); }

TEST(adapter_identity_invariants, fail_for_ids_with_no_luid_a_luid_with_no_name_and_a_name_with_no_end)
{ AdapterIdentity without_luid;
  AdapterIdentityProbe::ids_without_a_luid(without_luid);
  CHECK(!without_luid.invariants_hold());

  AdapterIdentity without_name;
  AdapterIdentityProbe::a_luid_without_a_description(without_name);
  CHECK(!without_name.invariants_hold());

  AdapterIdentity endless;
  AdapterIdentityProbe::a_description_that_never_ends(endless);
  CHECK(!endless.invariants_hold());

  /* Each half of the "nothing before the LUID" rule on its own, so dropping
   * one of them from the invariant is noticed. */
  AdapterIdentity device_only;
  AdapterIdentityProbe::a_device_id_without_a_luid(device_only);
  CHECK(!device_only.invariants_hold()); }

TEST(adapter_identity_invariants, fail_for_a_description_written_before_the_luid)
{ UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(runtime.open(), hr_ok);
  UmdAdapter *const adapter = UmdRootProbe::adapter_of(rig.root(), runtime.adapter());
  if (adapter == nullptr) return;
  AdapterIdentityProbe::a_description_without_a_luid(adapter->identity());
  CHECK(!adapter->identity().invariants_hold());
  CHECK(!rig.root().invariants_hold()); }

/* The adapter's invariant is its parts': a version list or an identity broken
 * under it must be noticed, or the table above would report it as sound. */
TEST(umd_adapter_invariants, fail_when_a_part_of_the_adapter_is_broken)
{ UmdAdapter versions(RuntimeKind::d3d10_11, &runtime_handle, &callbacks, unopened, d3d11_interface, 0u);
  UmdAdapterProbe::break_its_versions(versions);
  CHECK(!versions.invariants_hold());

  UmdAdapter identity(RuntimeKind::d3d10_11, &runtime_handle, &callbacks, unopened, d3d11_interface, 0u);
  UmdAdapterProbe::break_its_identity(identity);
  CHECK(!identity.invariants_hold()); }

TEST(adapter_table_invariants, fail_when_an_adapter_it_holds_is_broken)
{ MockPlatform platform;
  AdapterTable table(platform);
  UmdAdapter  *adapter = nullptr;
  CHECK_STATUS(table.open(adapter, RuntimeKind::d3d10_11, &runtime_handle, &callbacks, unopened, d3d11_interface, 0u),
               Status::ok);
  if (adapter == nullptr) return;
  CHECK(table.invariants_hold());
  UmdAdapterProbe::forget_the_callbacks(*adapter);
  CHECK(!table.invariants_hold());
  CHECK_STATUS(table.close(adapter), Status::ok); }

TEST(umd_device_invariants, fail_when_a_part_of_the_device_is_broken)
{ Adopted removal;
  if (!removal.made()) return;
  UmdDeviceProbe::break_its_removal(removal.device());
  CHECK(!removal.device().invariants_hold());

  Adopted unpublished;
  if (!unpublished.made()) return;
  UmdDeviceProbe::unpublish_its_dxgi(unpublished.device());
  CHECK(!unpublished.device().invariants_hold());

  /* The other half of "adopted or not adopted, never half": a published DXGI
   * table over a device that remembers no runtime handle. */
  Adopted halfway;
  if (!halfway.made()) return;
  UmdDeviceProbe::forget_the_runtime_device(halfway.device());
  CHECK(!halfway.device().invariants_hold()); }

TEST(device_table_invariants, fail_when_a_device_it_holds_is_broken)
{ Adopted adopted;
  if (!adopted.made()) return;
  CHECK(UmdRootProbe::devices(adopted.rig().root()).invariants_hold());
  UmdDeviceProbe::forget_the_callbacks(adopted.device());
  CHECK(!UmdRootProbe::devices(adopted.rig().root()).invariants_hold()); }

/* Two connections open on one adapter would be two sets of kernel objects for
 * what the runtimes treat as one; a connection broken under the table is the
 * table's to notice too. */
TEST(umd_connections_invariants, fail_for_two_connections_on_one_adapter_or_a_broken_one)
{ { MockKmt kmt;
    kmt.add_adapter(avgpu, AdapterScenario::wddm_3_2);
    kmt.set_adapter_luid(0u, UmdRig::adapter_luid);
    UmdConnections connections(kmt);
    CHECK(connections.invariants_hold());
    CHECK_STATUS(UmdConnectionsProbe::open_the_first_twice(connections, UmdRig::adapter_luid), Status::ok);
    CHECK(!connections.invariants_hold()); }
  { MockKmt kmt;
    UmdConnections connections(kmt);
    UmdConnectionProbe::count_an_open_it_does_not_hold(UmdConnectionsProbe::first(connections));
    CHECK(!connections.invariants_hold()); } }

TEST(umd_root_invariants, fail_when_the_connection_the_adapters_or_the_devices_are_broken)
{ /* The connection's own rule: an open counted over an adapter it never
   * found. Nothing is open here, so nothing else can be wrong. */
  { UmdRig rig;
    CHECK(rig.root().invariants_hold());
    UmdConnectionProbe::count_an_open_it_does_not_hold(UmdRootProbe::connection(rig.root()));
    CHECK(!rig.root().invariants_hold());
    UmdConnectionProbe::count_one_more_than_it_holds(UmdRootProbe::connection(rig.root()));
    UmdConnectionProbe::count_an_open_it_does_not_hold(UmdRootProbe::connection(rig.root())); }

  /* And the root's own: the connection is held once per open adapter. */
  { UmdRig rig;
    FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
    CHECK_EQ(runtime.open(), hr_ok);
    CHECK(rig.root().invariants_hold());
    UmdConnectionProbe::count_one_more_than_it_holds(UmdRootProbe::connection(rig.root()));
    CHECK(!rig.root().invariants_hold()); }

  { UmdRig rig;
    FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
    CHECK_EQ(runtime.open(), hr_ok);
    UmdAdapter *const adapter = UmdRootProbe::adapter_of(rig.root(), runtime.adapter());
    if (adapter == nullptr) return;
    UmdAdapterProbe::forget_the_callbacks(*adapter);
    CHECK(!rig.root().invariants_hold()); }

  { Adopted adopted;
    if (!adopted.made()) return;
    CHECK(adopted.rig().root().invariants_hold());
    UmdDeviceProbe::forget_the_callbacks(adopted.device());
    CHECK(!adopted.rig().root().invariants_hold()); } }

/* publish is reached through the create, which judges the version first, so
 * its own two refusals are asked for directly: they are what stop 22 slots
 * being written into a narrower revision's struct. */
TEST(dxgi_base_table, publish_refuses_a_null_argument_and_a_version_it_does_not_serve)
{ UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(runtime.open(), hr_ok);
  UmdAdapter *const adapter = UmdRootProbe::adapter_of(rig.root(), runtime.adapter());
  if (adapter == nullptr) return;
  UmdDevice device(*adapter, rig.log());
  CHECK_EQ(device.dxgi().publish(nullptr, d3d11_interface), hr_invalid_arg);

  Dxgi161DdiBaseFunctionsRecord table = {};
  DxgiDdiBaseCallbacksRecord    dxgi_callbacks = {};
  DxgiDdiBaseArgsRecord         args = {};
  args.pDXGIBaseCallbacks = &dxgi_callbacks;
  args.pDXGIDDIBaseFunctions = &table;
  CHECK_EQ(device.dxgi().publish(&args, d3d11_interface + 1u), hr_no_interface);
  CHECK(table.pfnPresent == nullptr);
  CHECK_EQ(device.dxgi().publish(&args, d3d11_interface), hr_ok);
  CHECK(table.pfnPresent != nullptr);
  CHECK_EQ(runtime.close_adapter(), hr_ok); }

/* The create's own refusal path: the object model is built and then given
 * back, so a runtime that offered no DXGI callbacks gets the refusal and this
 * driver keeps no device. */
TEST(umd_root_open, a_create_the_dxgi_seam_refuses_answers_that_and_keeps_no_device)
{ UmdRig rig;
  FakeD3dRuntime runtime(rig.root(), RuntimeKind::d3d10_11);
  CHECK_EQ(runtime.open(), hr_ok);
  runtime.offer_no_dxgi_callbacks();
  CHECK_EQ(runtime.create_device(), hr_invalid_arg);
  CHECK(rig.root().device_of(runtime.device_block()) == nullptr);
  CHECK(rig.root().invariants_hold());
  CHECK_EQ(runtime.close_adapter(), hr_ok);
  CHECK_EQ(rig.kmt().open_objects(), 0u); }
