#include "core/ddi/dxgkargcb_query_feature_support_record.hpp"
#include "core/ddi/dxgk_device_info_record.hpp"
#include "core/ddi/dxgkrnl_interface_record.hpp"
#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct DxgkrnlInterfaceProbe {
    explicit DxgkrnlInterfaceProbe(DxgkrnlInterface &target) : interface(target) {}
    void clear_callback() { interface.record_.DxgkCbQueueDpc = nullptr; }
    DxgkrnlInterface &interface;
};

}  // namespace av

using av::DxgkDeviceInfoRecord;
using av::DxgkrnlInterface;
using av::DxgkrnlInterfaceProbe;
using av::DxgkrnlInterfaceRecord;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace {

int handle_data_marker = 0;
int release_marker = 0;
unsigned released = 0u;

/* The dxgkrnl side of each callback: records its arguments, answers what the
 * test configured. */
class Callbacks {
public:
    Callbacks()
    {
        std::memset(&record_, 0, sizeof record_);
        record_.Size = sizeof record_;
        record_.Version = 0x11007u;
        record_.DeviceHandle = this;
        record_.DxgkCbGetDeviceInformation = +[](void *handle, DxgkDeviceInfoRecord *info) {
            return static_cast<Callbacks *>(handle)->get_device_information(info);
        };
        record_.DxgkCbMapMemory = +[](void *handle, std::int64_t address, std::uint32_t length,
                                      std::uint8_t io_space, std::uint8_t user_mode,
                                      std::int32_t cache, void **mapped) {
            return static_cast<Callbacks *>(handle)->map_memory(address, length, io_space, user_mode,
                                                                cache, mapped);
        };
        record_.DxgkCbQueueDpc = +[](void *handle) {
            return static_cast<Callbacks *>(handle)->queue_dpc();
        };
        /* Feature 049: what this OS answers about a feature the driver
         * implements. */
        record_.DxgkCbQueryFeatureSupport = +[](av::DxgkargcbQueryFeatureSupportRecord *request) {
            return static_cast<Callbacks *>(request->DeviceHandle)->query_feature_support(*request);
        };
        record_.DxgkCbReadDeviceSpace = +[](void *handle, std::uint32_t space, void *buffer,
                                            std::uint32_t offset, std::uint32_t length,
                                            std::uint32_t *read) {
            return static_cast<Callbacks *>(handle)->read_device_space(space, buffer, offset, length,
                                                                       read);
        };
        record_.DxgkCbUnmapMemory = +[](void *handle, void *mapped) {
            return static_cast<Callbacks *>(handle)->unmap_memory(mapped);
        };
        record_.DxgkCbSynchronizeExecution = +[](void *handle, av::SynchronizeRoutine routine, void *context,
                                                 std::uint32_t message, std::uint8_t *returned) {
            auto *self = static_cast<Callbacks *>(handle);
            self->synchronized_message = message;
            *returned = routine(context);
            return self->answer;
        };
        record_.DxgkCbNotifyInterrupt = +[](void *handle, const av::DxgkargcbNotifyInterruptDataRecord *data) {
            static_cast<Callbacks *>(handle)->last_interrupt_fence = data->FenceId;
        };
        record_.DxgkCbNotifyDpc = +[](void *handle) { static_cast<Callbacks *>(handle)->dpc_notifications++; };
        record_.DxgkCbQueryVidPnInterface = +[](void *vidpn, std::int32_t version,
                                                const av::DxgkVidPnInterfaceRecord **table) {
            auto *self = static_cast<Callbacks *>(vidpn);
            self->vidpn_version = version;
            *table = self->vidpn_table;
            return self->answer;
        };
        record_.DxgkCbAcquireHandleData =
            +[](const av::DxgkargcbGetHandleDataRecord *arguments, void **release_handle) -> void * {
                const bool known = arguments->hObject == 0x40000042u && arguments->Type == 1 && arguments->Flags == 0u;
                *release_handle = known ? &release_marker : nullptr;
                return known ? &handle_data_marker : nullptr;
            };
        record_.DxgkCbReleaseHandleData = +[](av::DxgkargcbReleaseHandleDataRecord arguments) {
            if (arguments.ReleaseHandle == &release_marker && arguments.Type == 1) released++;
        };
    }

    DxgkrnlInterfaceRecord &record() { return record_; }

    std::int32_t get_device_information(DxgkDeviceInfoRecord *info)
    {
        std::memset(info, 0, sizeof *info);
        info->TranslatedResourceList = resources;
        return answer;
    }

    std::int32_t map_memory(std::int64_t address, std::uint32_t length, std::uint8_t io_space,
                            std::uint8_t user_mode, std::int32_t cache, void **mapped)
    {
        mapped_address = address; mapped_length = length;
        mapped_io_space = io_space; mapped_user_mode = user_mode; mapped_cache = cache;
        *mapped = mapping;
        return answer;
    }

    std::int32_t query_feature_support(av::DxgkargcbQueryFeatureSupportRecord &request)
    {
        feature_queries++;
        feature_asked = request.FeatureId;
        feature_support_asked = request.DriverSupportState;
        request.Enabled = (allowed_features & (std::uint64_t{1} << (request.FeatureId % 64u))) != 0u ? 1u : 0u;
        return answer;
    }

    std::uint64_t allowed_features = 0u;
    std::uint32_t feature_queries = 0u;
    std::uint32_t feature_asked = 0xFFFFFFFFu;
    std::uint32_t feature_support_asked = 0xFFFFFFFFu;

    std::uint8_t queue_dpc() { dpcs++; return dpc_answer; }

    std::int32_t read_device_space(std::uint32_t space, void *buffer, std::uint32_t offset,
                                   std::uint32_t length, std::uint32_t *read)
    {
        read_space = space; read_offset = offset;
        std::memset(buffer, 0xA5, length);
        *read = length - short_by;
        return answer;
    }

    std::int32_t unmap_memory(void *mapped) { unmapped = mapped; return answer; }

    std::int32_t        answer = 0;
    std::uint8_t        dpc_answer = 1u;
    std::uint32_t       short_by = 0u;
    const std::uint8_t  resources[4] = {1u, 0u, 0u, 0u};
    void               *mapping = &record_;
    std::int64_t        mapped_address = 0;
    std::uint32_t       mapped_length = 0u;
    std::uint8_t        mapped_io_space = 9u;
    std::uint8_t        mapped_user_mode = 9u;
    std::int32_t        mapped_cache = 9;
    std::uint32_t       dpcs = 0u;
    std::uint32_t       read_space = 9u;
    std::uint32_t       read_offset = 9u;
    void               *unmapped = nullptr;
    std::uint32_t       synchronized_message = 9u;
    std::uint32_t       last_interrupt_fence = 0u;
    std::uint32_t       dpc_notifications = 0u;
    std::int32_t        vidpn_version = 0;
    av::DxgkVidPnInterfaceRecord  vidpn_record = {};
    const av::DxgkVidPnInterfaceRecord *vidpn_table = &vidpn_record;

private:
    DxgkrnlInterfaceRecord record_;
};

constexpr std::int32_t no_memory = static_cast<std::int32_t>(0xC0000017u);

}  // namespace

TEST(dxgkrnl_interface, starts_uninitialized)
{ MockPlatform platform; DxgkrnlInterface interface(platform);
  CHECK(!interface.initialized());
  CHECK(interface.invariants_hold());
  DxgkDeviceInfoRecord info;
  void *mapped = nullptr;
  std::uint8_t config[4];
  CHECK_STATUS(interface.get_device_information(info), Status::invalid_state);
  CHECK_STATUS(interface.read_config_space(config, 4u), Status::invalid_state);
  CHECK_STATUS(interface.map_memory(0x1000u, 0x1000u, mapped), Status::invalid_state);
  CHECK_STATUS(interface.unmap_memory(&info), Status::invalid_state);
  CHECK(!interface.queue_dpc()); }

TEST(dxgkrnl_interface_initialize, copies_the_table_dxgkrnl_passes)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks;
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::ok);
  CHECK(interface.initialized());
  CHECK(interface.invariants_hold());
  callbacks.record().DeviceHandle = nullptr;  /* dxgkrnl's copy may change; ours does not */
  CHECK(interface.queue_dpc());
  CHECK_EQ(callbacks.dpcs, 1u); }

TEST(dxgkrnl_interface_device_handle, is_dxgkrnl_s_handle_once_initialized)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks;
  CHECK(interface.device_handle() == nullptr);
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::ok);
  CHECK(interface.device_handle() == &callbacks); }

TEST(dxgkrnl_interface_initialize, refuses_a_missing_or_short_table)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks;
  CHECK_STATUS(interface.initialize(nullptr), Status::invalid_parameter);
  callbacks.record().Size = sizeof(DxgkrnlInterfaceRecord) - 8u;
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::not_supported);
  CHECK(!interface.initialized()); }

TEST(dxgkrnl_interface_initialize, refuses_a_table_missing_a_callback_core_calls)
{ MockPlatform platform; DxgkrnlInterface interface(platform);
  const auto clear = [](DxgkrnlInterfaceRecord &record, int which) {
      if (which == 0) record.DxgkCbGetDeviceInformation = nullptr;
      if (which == 1) record.DxgkCbMapMemory = nullptr;
      if (which == 2) record.DxgkCbQueueDpc = nullptr;
      if (which == 3) record.DxgkCbReadDeviceSpace = nullptr;
      if (which == 4) record.DxgkCbUnmapMemory = nullptr;
      if (which == 5) record.DxgkCbAcquireHandleData = nullptr;
      if (which == 6) record.DxgkCbSynchronizeExecution = nullptr;
      if (which == 7) record.DxgkCbNotifyInterrupt = nullptr;
      if (which == 8) record.DxgkCbNotifyDpc = nullptr;
      if (which == 9) record.DxgkCbQueryVidPnInterface = nullptr;
      if (which == 10) record.DxgkCbReleaseHandleData = nullptr;
  };
  for (int which = 0; which < 11; which++) {
      Callbacks callbacks;
      clear(callbacks.record(), which);
      CHECK_STATUS(interface.initialize(&callbacks.record()), Status::invalid_parameter);
      CHECK(!interface.initialized());
  } }

TEST(dxgkrnl_interface_initialize, a_later_table_replaces_the_first)
{ MockPlatform platform; DxgkrnlInterface interface(platform);
  Callbacks first, second;
  CHECK_STATUS(interface.initialize(&first.record()), Status::ok);
  CHECK_STATUS(interface.initialize(&second.record()), Status::ok);
  CHECK(interface.queue_dpc());
  CHECK_EQ(first.dpcs, 0u);
  CHECK_EQ(second.dpcs, 1u);
  Callbacks broken;
  broken.record().DxgkCbUnmapMemory = nullptr;
  CHECK_STATUS(interface.initialize(&broken.record()), Status::invalid_parameter);
  CHECK(!interface.initialized()); }

TEST(dxgkrnl_interface_initialize, is_passive_only)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks;
  platform.set_irql(Irql::dispatch);
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::ok);
  CHECK(platform.claim(Violation::irql_ceiling)); }

TEST(dxgkrnl_interface_get_device_information, answers_what_dxgkrnl_filled)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks;
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::ok);
  DxgkDeviceInfoRecord info;
  CHECK_STATUS(interface.get_device_information(info), Status::ok);
  CHECK(info.TranslatedResourceList == callbacks.resources);
  callbacks.answer = no_memory;
  CHECK_STATUS(interface.get_device_information(info), Status::no_memory); }

TEST(dxgkrnl_interface_read_config_space, reads_from_offset_zero_of_config_space)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks;
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::ok);
  std::uint8_t config[8] = {};
  CHECK_STATUS(interface.read_config_space(config, 8u), Status::ok);
  CHECK_EQ(callbacks.read_space, 0u);
  CHECK_EQ(callbacks.read_offset, 0u);
  CHECK_EQ(config[7], 0xA5u); }

TEST(dxgkrnl_interface_read_config_space, a_short_read_is_malformed_and_a_refusal_propagates)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks;
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::ok);
  std::uint8_t config[8] = {};
  callbacks.short_by = 1u;
  CHECK_STATUS(interface.read_config_space(config, 8u), Status::malformed);
  callbacks.short_by = 0u;
  callbacks.answer = no_memory;
  CHECK_STATUS(interface.read_config_space(config, 8u), Status::no_memory); }

TEST(dxgkrnl_interface_map_memory, maps_non_cached_kernel_memory)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks;
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::ok);
  void *mapped = nullptr;
  CHECK_STATUS(interface.map_memory(0x800000000ull, 0x4000u, mapped), Status::ok);
  CHECK(mapped == callbacks.mapping);
  CHECK(callbacks.mapped_address == 0x800000000ll);
  CHECK_EQ(callbacks.mapped_length, 0x4000u);
  CHECK_EQ(callbacks.mapped_io_space, 0u);
  CHECK_EQ(callbacks.mapped_user_mode, 0u);
  CHECK_EQ(callbacks.mapped_cache, 0);
  CHECK_STATUS(interface.unmap_memory(mapped), Status::ok);
  CHECK(callbacks.unmapped == mapped); }

TEST(dxgkrnl_interface_map_memory, a_refusal_leaves_no_mapping_and_propagates)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks;
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::ok);
  callbacks.answer = no_memory;
  void *mapped = &callbacks;
  CHECK_STATUS(interface.map_memory(0x1000u, 0x1000u, mapped), Status::no_memory);
  CHECK(mapped == nullptr);
  CHECK_STATUS(interface.unmap_memory(&callbacks), Status::no_memory); }

TEST(dxgkrnl_interface_queue_dpc, answers_whether_dxgkrnl_queued_it)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks;
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::ok);
  platform.set_irql(Irql::isr);
  callbacks.dpc_answer = 0u;
  CHECK(!interface.queue_dpc());
  platform.set_irql(Irql::passive); }

TEST(dxgkrnl_interface_invariants, an_initialized_table_missing_a_callback_is_detected)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks;
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::ok);
  DxgkrnlInterfaceProbe(interface).clear_callback();
  CHECK(!interface.invariants_hold()); }

/* An unknown handle answers invalid_parameter, never STATUS_NOT_FOUND, which
 * dxgkrnl logs as an invalid NTSTATUS and hands to the caller. */
TEST(dxgkrnl_interface_acquire_handle_data, lends_the_driver_data_behind_an_allocation_handle)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks;
  void *data = &callbacks;
  void *release = &callbacks;
  CHECK_STATUS(interface.acquire_handle_data(0x40000042u, data, release), Status::invalid_state);
  CHECK(data == nullptr);
  CHECK(release == nullptr);
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::ok);
  CHECK_STATUS(interface.acquire_handle_data(0x40000042u, data, release), Status::ok);
  CHECK(data == &handle_data_marker);
  CHECK(release == &release_marker);
  const unsigned before = released;
  interface.release_handle_data(release);
  CHECK_EQ(released, before + 1u);
  CHECK_STATUS(interface.acquire_handle_data(0x40000043u, data, release), Status::invalid_parameter);
  CHECK(data == nullptr);
  CHECK(release == nullptr); }

TEST(dxgkrnl_interface_acquire_handle_data, is_apc_at_most)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks;
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::ok);
  void *data = nullptr;
  void *release = nullptr;
  platform.set_irql(Irql::dispatch);
  static_cast<void>(interface.acquire_handle_data(1u, data, release));
  interface.release_handle_data(nullptr);
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.claim(Violation::irql_ceiling)); }

namespace {

std::uint8_t mark_ran(void *context)
{
    *static_cast<int *>(context) += 1;
    return 1u;
}

}  // namespace

TEST(dxgkrnl_interface_synchronize_execution, runs_the_routine_through_dxgkrnl_on_message_zero)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks; int ran = 0;
  CHECK_STATUS(interface.synchronize_execution(&mark_ran, &ran), Status::invalid_state);
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::ok);
  CHECK_STATUS(interface.synchronize_execution(&mark_ran, &ran), Status::ok);
  CHECK_EQ(ran, 1);
  CHECK_EQ(callbacks.synchronized_message, 0u);
  callbacks.answer = no_memory;
  CHECK_STATUS(interface.synchronize_execution(&mark_ran, &ran), Status::no_memory); }

TEST(dxgkrnl_interface_notify_interrupt, forwards_the_data_and_the_dpc_notification_once_initialized)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks;
  av::DxgkargcbNotifyInterruptDataRecord data = {};
  data.FenceId = 7u;
  interface.notify_interrupt(data);
  interface.notify_dpc();
  CHECK_EQ(callbacks.last_interrupt_fence, 0u);
  CHECK_EQ(callbacks.dpc_notifications, 0u);
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::ok);
  interface.notify_interrupt(data);
  interface.notify_dpc();
  CHECK_EQ(callbacks.last_interrupt_fence, 7u);
  CHECK_EQ(callbacks.dpc_notifications, 1u); }

TEST(dxgkrnl_interface_notify_dpc, is_dispatch_at_most_and_notify_interrupt_is_device_irql)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks; int ran = 0;
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::ok);
  av::DxgkargcbNotifyInterruptDataRecord data = {};
  platform.set_irql(Irql::isr);
  interface.notify_interrupt(data);
  CHECK_EQ(platform.unclaimed_violations(), 0u);
  interface.notify_dpc();
  CHECK(platform.claim(Violation::irql_ceiling));
  static_cast<void>(interface.synchronize_execution(&mark_ran, &ran));
  CHECK(platform.claim(Violation::irql_ceiling));
  platform.set_irql(Irql::passive); }

TEST(dxgkrnl_interface_query_vidpn_interface, answers_version_one_of_the_table)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks;
  const av::DxgkVidPnInterfaceRecord *table = &callbacks.vidpn_record;
  CHECK_STATUS(interface.query_vidpn_interface(&callbacks, table), Status::invalid_state);
  CHECK(table == nullptr);
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::ok);
  CHECK_STATUS(interface.query_vidpn_interface(&callbacks, table), Status::ok);
  CHECK(table == &callbacks.vidpn_record);
  CHECK_EQ(callbacks.vidpn_version, 1);
  callbacks.vidpn_table = nullptr;
  CHECK_STATUS(interface.query_vidpn_interface(&callbacks, table), Status::malformed);
  callbacks.answer = no_memory;
  CHECK_STATUS(interface.query_vidpn_interface(&callbacks, table), Status::no_memory);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(interface.query_vidpn_interface(&callbacks, table));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }

/* Feature 049: a driver must ask the OS before it reports a scheduling
 * capability, because reporting one the OS has not enabled fails the whole
 * adapter start (research.md §15). An answer this driver cannot get - no
 * callback, a refusal, or a table it never initialized - is "not allowed",
 * which is the answer that keeps the adapter alive. */
TEST(dxgkrnl_interface_feature_enabled, answers_what_the_os_allows_and_no_for_anything_else)
{ MockPlatform platform; DxgkrnlInterface interface(platform); Callbacks callbacks;
  CHECK(!interface.feature_enabled(av::feature_hwsch, av::feature_support_stable));
  CHECK_EQ(callbacks.feature_queries, 0u);
  CHECK_STATUS(interface.initialize(&callbacks.record()), Status::ok);
  CHECK(!interface.feature_enabled(av::feature_hwsch, av::feature_support_stable));
  CHECK_EQ(callbacks.feature_queries, 1u);
  CHECK_EQ(callbacks.feature_asked, av::feature_hwsch);
  CHECK_EQ(callbacks.feature_support_asked, av::feature_support_stable);

  callbacks.allowed_features = std::uint64_t{1} << av::feature_hwsch;
  CHECK(interface.feature_enabled(av::feature_hwsch, av::feature_support_stable));
  CHECK(!interface.feature_enabled(av::feature_user_mode_submission, av::feature_support_stable));

  /* A refusal is not an answer of yes. */
  callbacks.answer = no_memory;
  CHECK(!interface.feature_enabled(av::feature_hwsch, av::feature_support_stable));
  callbacks.answer = 0;

  /* A table without the callback at all is every table older than WDDM 2.9. */
  DxgkrnlInterface older(platform);
  Callbacks        without;
  without.record().DxgkCbQueryFeatureSupport = nullptr;
  CHECK_STATUS(older.initialize(&without.record()), Status::ok);
  CHECK(!older.feature_enabled(av::feature_hwsch, av::feature_support_stable));

  platform.set_irql(Irql::dispatch);
  static_cast<void>(interface.feature_enabled(av::feature_hwsch, av::feature_support_stable));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }
