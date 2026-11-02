#include "core/log/driver_log.hpp"
#include "core/trace/state_writer.hpp"
#include "executor/sharing/host_shares.hpp"
#include "harness/executor/counted_object.hpp"
#include "harness/executor/host_shares_probe.hpp"
#include "harness/executor/mock_session_backend.hpp"
#include "executor/sharing/shared_image_shape.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>
#include <cstring>

using av::CountedObject;
using av::HostShares;
using av::MockPlatform;
using av::MockSessionBackend;
using av::ShareOrigin;
using av::StateWriter;
using av::Status;

namespace {

constexpr std::uint32_t domain = 1u;
constexpr std::uint32_t kind = 7u;
constexpr std::uint64_t size = 4096u;
constexpr std::uint64_t handle = 0x2001u;

class Fixture {
public:
    Fixture() : lines(log, "shares: "), shares(platform, lines) {}

    void published(std::uint64_t id)
    {
        CHECK_STATUS(shares.publish(domain, id, kind, size, publisher, handle), Status::ok);
    }

    std::uint64_t opened(std::uint64_t id, MockSessionBackend &session)
    {
        std::uint64_t made = 0u;
        CHECK_STATUS(shares.open(domain, id, kind, size, session, made), Status::ok);
        return made;
    }

    std::uint32_t drained()
    {
        IUnknown     *objects[8] = {};
        std::uint32_t total = 0u;
        for (std::uint32_t taken = shares.take_retired(objects, 8u); taken != 0u;
             taken = shares.take_retired(objects, 8u)) {
            for (std::uint32_t index = 0u; index < taken; index++) objects[index]->Release();
            total += taken;
        }
        return total;
    }

    MockPlatform       platform;
    av::RecordingLog   log;
    av::DriverLog      lines;
    HostShares         shares;
    MockSessionBackend publisher;
    MockSessionBackend reader;
    MockSessionBackend other;
};

bool reports(HostShares &shares, const char *expected)
{
    static char bytes[4096];
    StateWriter writer(bytes, sizeof bytes);
    shares.report(writer);
    return std::strstr(writer.dump(), expected) != nullptr;
}

class ReportingObject final : public IUnknown {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        if (shares != nullptr && references == 2u) saw_busy = reports(*shares, "busy=true");
        return --references;
    }

    ULONG       references = 1u;
    HostShares *shares = nullptr;
    bool        saw_busy = false;
};

}

TEST(host_shares, an_object_one_session_publishes_another_opens_as_its_own)
{ Fixture f;
  f.published(0x2Au);
  CHECK_EQ(f.publisher.exported_objects(), 1u);
  CHECK_EQ(f.shares.live(), 1u);
  const std::uint64_t made = f.opened(0x2Au, f.reader);
  CHECK_EQ(made, MockSessionBackend::object_handle + 1u);
  CHECK_EQ(f.reader.imported_objects(), 1u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, an_open_hands_the_importer_the_image_the_publisher_described)
{ Fixture f;
  f.published(0x2Au);
  CHECK(!f.shares.describe_image(domain, 0x2Bu, av::SharedImageShape(4u, 4u, 87u, 16u)));
  static_cast<void>(f.opened(0x2Au, f.reader));
  CHECK(!f.reader.imported_shape().described());
  f.published(0x2Cu);
  CHECK(f.shares.describe_image(domain, 0x2Cu, av::SharedImageShape(64u, 32u, 87u, 256u)));
  static_cast<void>(f.opened(0x2Cu, f.reader));
  CHECK(f.reader.imported_shape().described());
  CHECK_EQ(f.reader.imported_shape().width(), 64u);
  CHECK_EQ(f.reader.imported_shape().pitch(), 256u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, the_same_session_publishing_the_same_object_again_is_ok)
{ Fixture f;
  f.published(0x2Au);
  f.published(0x2Au);
  CHECK_EQ(f.publisher.exported_objects(), 1u);
  CHECK_EQ(f.shares.live(), 1u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, publishing_an_id_already_shared_otherwise_is_a_conflict)
{ Fixture f;
  f.published(0x2Au);
  CHECK_STATUS(f.shares.publish(domain, 0x2Au, kind, size, f.other, handle), Status::conflict);
  CHECK_STATUS(f.shares.publish(domain, 0x2Au, kind + 1u, size, f.publisher, handle), Status::conflict);
  CHECK_STATUS(f.shares.publish(domain, 0x2Au, kind, size * 2u, f.publisher, handle), Status::conflict);
  const std::uint64_t made = f.opened(0x2Au, f.reader);
  CHECK(made != 0u);
  CHECK_STATUS(f.shares.publish(domain, 0x2Au, kind, size, f.reader, made), Status::conflict);
  CHECK_EQ(f.shares.live(), 1u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, publishing_id_0_is_refused)
{ Fixture f;
  CHECK_STATUS(f.shares.publish(domain, 0u, kind, size, f.publisher, handle), Status::invalid_parameter);
  CHECK_EQ(f.publisher.exported_objects(), 0u);
  CHECK_EQ(f.shares.live(), 0u); }

TEST(host_shares, a_backend_that_exports_nothing_publishes_nothing)
{ Fixture f;
  f.publisher.set_shares(false);
  CHECK_STATUS(f.shares.publish(domain, 0x2Au, kind, size, f.publisher, handle), Status::not_supported);
  CHECK_EQ(f.shares.live(), 0u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, a_publish_with_no_memory_records_nothing)
{ Fixture f;
  f.platform.fail_allocation_at(1u);
  CHECK_STATUS(f.shares.publish(domain, 0x2Au, kind, size, f.publisher, handle), Status::no_memory);
  CHECK_EQ(f.shares.live(), 0u);
  f.platform.fail_allocation_at(2u);
  CHECK_STATUS(f.shares.publish(domain, 0x2Au, kind, size, f.publisher, handle), Status::no_memory);
  CHECK_EQ(f.shares.live(), 0u);
  CHECK_EQ(f.drained(), 0u);
  CHECK_EQ(f.log.holding("a share cannot be recorded, out of memory"), 1u);
  CHECK_EQ(f.log.holding("a share cannot be indexed, out of memory"), 1u);
  f.published(0x2Au);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, an_id_shared_during_the_export_is_a_conflict)
{ CountedObject resource;
  Fixture f;
  struct During {
      Fixture       *f;
      CountedObject *resource;
  } during{&f, &resource};
  f.publisher.before_next_export(
      [](void *context) {
          auto *const d = static_cast<During *>(context);
          CHECK_STATUS(d->f->shares.publish_resource(domain, 0x2Au, d->resource, ShareOrigin::d3d11, d->f->other),
                       Status::ok);
      },
      &during);
  CHECK_STATUS(f.shares.publish(domain, 0x2Au, kind, size, f.publisher, handle), Status::conflict);
  CHECK_EQ(f.shares.live(), 1u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, opening_what_is_not_shared_is_not_found)
{ Fixture f;
  std::uint64_t made = 1u;
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.reader, made), Status::not_found);
  CHECK_EQ(made, 0u);
  f.published(0x2Au);
  CHECK_STATUS(f.shares.open(domain + 1u, 0x2Au, kind, size, f.reader, made), Status::not_found);
  CHECK_EQ(f.reader.imported_objects(), 0u); }

TEST(host_shares, an_open_naming_another_kind_or_size_is_refused)
{ Fixture f;
  f.published(0x2Au);
  std::uint64_t made = 1u;
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind + 1u, size, f.reader, made), Status::invalid_parameter);
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size + 1u, f.reader, made), Status::invalid_parameter);
  CHECK_EQ(made, 0u);
  CHECK_EQ(f.reader.imported_objects(), 0u);
  f.shares.release(domain, 0x2Au);
  CHECK_EQ(f.shares.live(), 0u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, a_session_opens_a_share_once)
{ Fixture f;
  f.published(0x2Au);
  (void)f.opened(0x2Au, f.reader);
  std::uint64_t made = 1u;
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.reader, made), Status::conflict);
  CHECK_EQ(made, 0u);
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.publisher, made), Status::conflict);
  CHECK_EQ(f.reader.imported_objects(), 1u); }

TEST(host_shares, a_backend_that_imports_nothing_opens_nothing)
{ Fixture f;
  f.published(0x2Au);
  f.reader.set_shares(false);
  std::uint64_t made = 1u;
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.reader, made), Status::not_supported);
  CHECK_EQ(made, 0u);
  f.shares.release(domain, 0x2Au);
  CHECK_EQ(f.shares.live(), 0u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, an_open_with_no_memory_releases_what_it_imported)
{ Fixture f;
  f.published(0x2Au);
  MockSessionBackend sessions[64];
  for (MockSessionBackend &session : sessions) (void)f.opened(0x2Au, session);
  f.platform.fail_allocation_at(1u);
  std::uint64_t made = 1u;
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.reader, made), Status::no_memory);
  CHECK_EQ(made, 0u);
  CHECK_EQ(f.log.holding("a share cannot be opened, out of memory"), 1u);
  CHECK_EQ(f.reader.imported_objects(), 1u);
  CHECK_EQ(f.reader.released_objects(), 1u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, a_close_releases_only_what_was_imported)
{ Fixture f;
  f.published(0x2Au);
  (void)f.opened(0x2Au, f.reader);
  f.shares.close(domain, 0x2Au, f.reader);
  CHECK_EQ(f.reader.released_objects(), 1u);
  CHECK_EQ(f.reader.last_released_detail(), (MockSessionBackend::object_handle + 1u) ^ MockSessionBackend::detail_mask);
  f.shares.close(domain, 0x2Au, f.publisher);
  CHECK_EQ(f.publisher.released_objects(), 0u);
  CHECK_EQ(f.shares.live(), 1u);
  f.shares.close(domain, 0x2Au, f.reader);
  f.shares.close(domain, 0x2Bu, f.reader);
  CHECK_EQ(f.reader.released_objects(), 1u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, a_share_the_guest_released_lives_while_a_session_holds_it)
{ Fixture f;
  f.published(0x2Au);
  (void)f.opened(0x2Au, f.reader);
  f.shares.release(domain, 0x2Au);
  CHECK_EQ(f.shares.live(), 1u);
  CHECK(reports(f.shares, "released_by_guest=1"));
  f.shares.close(domain, 0x2Au, f.publisher);
  CHECK_EQ(f.shares.live(), 1u);
  f.shares.close(domain, 0x2Au, f.reader);
  CHECK_EQ(f.shares.live(), 0u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, releasing_what_is_not_shared_is_nothing)
{ Fixture f;
  f.published(0x2Au);
  f.shares.release(domain, 0x2Bu);
  f.shares.release(domain + 1u, 0x2Au);
  f.shares.release(domain + 1u, 0u);
  CHECK_EQ(f.shares.live(), 1u);
  CHECK(reports(f.shares, "released_by_guest=0")); }

TEST(host_shares, releasing_id_0_releases_the_whole_domain)
{ Fixture f;
  f.published(0x2Au);
  f.published(0x2Bu);
  CHECK_STATUS(f.shares.publish(domain + 1u, 0x2Au, kind, size, f.other, handle), Status::ok);
  (void)f.opened(0x2Bu, f.reader);
  f.shares.release(domain, 0u);
  CHECK_EQ(f.shares.live(), 2u);
  CHECK(reports(f.shares, "unindexed=1"));
  std::uint64_t made = 1u;
  CHECK_STATUS(f.shares.open(domain, 0x2Bu, kind, size, f.other, made), Status::not_found);
  f.published(0x2Bu);
  CHECK_EQ(f.shares.live(), 3u);
  f.shares.close(domain, 0x2Bu, f.reader);
  CHECK_EQ(f.reader.released_objects(), 0u);
  f.shares.session_closed(f.reader);
  CHECK_EQ(f.shares.live(), 2u);
  CHECK(reports(f.shares, "unindexed=0"));
  CHECK_STATUS(f.shares.open(domain + 1u, 0x2Au, kind, size, f.reader, made), Status::ok);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, a_publisher_ending_takes_no_one_else_s_hold)
{ Fixture f;
  f.published(0x2Au);
  (void)f.opened(0x2Au, f.reader);
  f.shares.session_closed(f.publisher);
  f.shares.release(domain, 0x2Au);
  CHECK_EQ(f.shares.live(), 1u);
  CHECK_EQ(f.reader.released_objects(), 0u);
  f.shares.close(domain, 0x2Au, f.reader);
  CHECK_EQ(f.reader.released_objects(), 1u);
  CHECK_EQ(f.shares.live(), 0u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, a_publisher_that_ended_publishes_nothing_again)
{ Fixture f;
  f.published(0x2Au);
  f.shares.session_closed(f.publisher);
  CHECK(reports(f.shares, "published=false"));
  CHECK_STATUS(f.shares.publish(domain, 0x2Au, kind, size, f.publisher, handle), Status::conflict);
  CHECK(f.opened(0x2Au, f.publisher) != 0u);
  CHECK_EQ(f.shares.live(), 1u); }

TEST(host_shares, a_session_closing_lets_go_of_all_it_held)
{ Fixture f;
  f.published(0x2Au);
  f.published(0x2Bu);
  (void)f.opened(0x2Au, f.reader);
  (void)f.opened(0x2Bu, f.reader);
  f.shares.release(domain, 0x2Au);
  f.shares.session_closed(f.publisher);
  CHECK_EQ(f.shares.live(), 2u);
  f.shares.session_closed(f.reader);
  CHECK_EQ(f.reader.released_objects(), 2u);
  CHECK_EQ(f.publisher.released_objects(), 0u);
  CHECK_EQ(f.shares.live(), 1u);
  f.shares.session_closed(f.other);
  CHECK_EQ(f.shares.live(), 1u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, clearing_forgets_every_share_and_hold)
{ CountedObject resource;
  Fixture f;
  f.published(0x2Au);
  (void)f.opened(0x2Au, f.reader);
  CHECK_STATUS(f.shares.publish_resource(domain + 1u, 0x2Bu, &resource, ShareOrigin::d3d11, f.other), Status::ok);
  f.shares.clear();
  CHECK_EQ(f.shares.live(), 0u);
  CHECK_EQ(f.drained(), 1u);
  CHECK_EQ(resource.references(), 1u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, the_live_count_is_logged_at_each_power_of_two_from_64)
{ CountedObject resource;
  Fixture f;
  for (std::uint64_t id = 1u; id < 64u; id++) f.published(id);
  CHECK_EQ(f.log.holding("shares live"), 0u);
  CHECK_STATUS(f.shares.publish_resource(domain, 64u, &resource, ShareOrigin::d3d12, f.publisher), Status::ok);
  CHECK_EQ(f.log.holding("shares live"), 1u);
  CHECK_EQ(f.log.holding("shares live: 64 (63 objects, 0 d3d11, 1 d3d12, 0 released by the guest, 0 holders)"), 1u);
  f.shares.release(domain, 1u);
  f.published(1u);
  CHECK_EQ(f.log.holding("shares live"), 1u);
  for (std::uint64_t id = 65u; id <= 128u; id++) f.published(id);
  CHECK_EQ(f.log.holding("shares live: 128"), 1u);
  CHECK_EQ(f.log.holding("shares live"), 2u);
  f.shares.clear();
  CHECK_EQ(f.drained(), 1u); }

TEST(host_shares, holds_more_shares_than_any_fixed_table_did)
{ Fixture f;
  for (std::uint64_t id = 1u; id <= 1000u; id++) f.published(id);
  CHECK_EQ(f.shares.live(), 1000u);
  CHECK(f.opened(999u, f.reader) != 0u);
  f.shares.session_closed(f.publisher);
  f.shares.release(domain, 0u);
  CHECK_EQ(f.shares.live(), 1u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, a_published_resource_opens_with_a_reference_of_the_caller_s)
{ CountedObject resource;
  Fixture f;
  CHECK_STATUS(f.shares.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher), Status::ok);
  CHECK_EQ(resource.references(), 2u);
  IUnknown *const opened = f.shares.open_resource(domain, 0x2Au);
  CHECK(opened == &resource);
  CHECK_EQ(resource.references(), 3u);
  opened->Release();
  CHECK(f.shares.open_resource(domain, 0x2Bu) == nullptr);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, publishing_a_resource_needs_an_id_and_a_resource)
{ CountedObject resource;
  Fixture f;
  CHECK_STATUS(f.shares.publish_resource(domain, 0u, &resource, ShareOrigin::d3d11, f.publisher),
               Status::invalid_parameter);
  CHECK_STATUS(f.shares.publish_resource(domain, 0x2Au, nullptr, ShareOrigin::d3d11, f.publisher),
               Status::invalid_parameter);
  CHECK_STATUS(f.shares.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher), Status::ok);
  CHECK_STATUS(f.shares.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.other), Status::conflict);
  CHECK_EQ(resource.references(), 2u); }

TEST(host_shares, a_resource_publish_with_no_memory_holds_no_reference)
{ CountedObject resource;
  Fixture f;
  f.platform.fail_allocation_at(1u);
  CHECK_STATUS(f.shares.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher),
               Status::no_memory);
  f.platform.fail_allocation_at(2u);
  CHECK_STATUS(f.shares.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher),
               Status::no_memory);
  CHECK_EQ(f.shares.live(), 0u);
  CHECK_EQ(resource.references(), 1u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, a_released_resource_lives_on_by_its_openers_references)
{ CountedObject resource;
  Fixture f;
  CHECK_STATUS(f.shares.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher), Status::ok);
  IUnknown *const opened = f.shares.open_resource(domain, 0x2Au);
  f.shares.open_resource(domain, 0x2Au)->Release();
  CHECK(reports(f.shares, "opens=2"));
  f.shares.release(domain, 0x2Au);
  CHECK_EQ(f.shares.live(), 0u);
  CHECK(f.shares.open_resource(domain, 0x2Au) == nullptr);
  CHECK_EQ(resource.references(), 3u);
  CHECK_EQ(f.drained(), 1u);
  CHECK_EQ(resource.references(), 2u);
  opened->Release();
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, opens_as_the_flip_copy_then_the_view_then_the_resource)
{ CountedObject resource, view, newer_view, shown, newer_shown;
  Fixture f;
  CHECK_STATUS(f.shares.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d12, f.publisher), Status::ok);
  IUnknown *found = f.shares.view(domain, 0x2Au);
  CHECK(found == &resource);
  found->Release();
  CHECK(f.shares.keep_view(domain, 0x2Au, &view));
  CHECK(f.shares.keep_view(domain, 0x2Au, &newer_view));
  CHECK_EQ(view.references(), 2u);
  CHECK_EQ(resource.references(), 2u);
  found = f.shares.open_resource(domain, 0x2Au);
  CHECK(found == &newer_view);
  found->Release();
  CHECK(f.shares.shown(domain, 0x2Au) == nullptr);
  CHECK(f.shares.keep_shown(domain, 0x2Au, &shown));
  CHECK(f.shares.keep_shown(domain, 0x2Au, &newer_shown));
  found = f.shares.shown(domain, 0x2Au);
  CHECK(found == &newer_shown);
  found->Release();
  found = f.shares.open_resource(domain, 0x2Au);
  CHECK(found == &newer_shown);
  found->Release();
  found = f.shares.resource(domain, 0x2Au);
  CHECK(found == &resource);
  found->Release();
  CHECK_EQ(f.drained(), 2u);
  CHECK_EQ(view.references(), 1u);
  CHECK_EQ(shown.references(), 1u);
  f.shares.clear();
  CHECK_EQ(f.drained(), 3u);
  CHECK_EQ(newer_view.references(), 1u);
  CHECK_EQ(newer_shown.references(), 1u);
  CHECK_EQ(resource.references(), 1u); }

TEST(host_shares_lose_direct3d11, gives_up_the_lost_device_s_objects_and_keeps_the_shares)
{ CountedObject resource11, shown11, resource12, view12, shown12;
  Fixture f;
  CHECK_STATUS(f.shares.publish_resource(domain, 0x11u, &resource11, ShareOrigin::d3d11, f.publisher), Status::ok);
  CHECK(f.shares.keep_shown(domain, 0x11u, &shown11));
  CHECK_STATUS(f.shares.publish_resource(domain, 0x12u, &resource12, ShareOrigin::d3d12, f.publisher), Status::ok);
  CHECK(f.shares.keep_view(domain, 0x12u, &view12));
  CHECK(f.shares.keep_shown(domain, 0x12u, &shown12));
  f.shares.lose_direct3d11();
  CHECK_EQ(f.drained(), 4u);
  CHECK_EQ(resource11.references(), 1u);
  CHECK_EQ(shown11.references(), 1u);
  CHECK_EQ(view12.references(), 1u);
  CHECK_EQ(shown12.references(), 1u);
  CHECK_EQ(resource12.references(), 2u);
  CHECK_EQ(f.shares.live(), 2u);
  CHECK(f.shares.open_resource(domain, 0x11u) == nullptr);
  IUnknown *const found = f.shares.open_resource(domain, 0x12u);
  CHECK(found == &resource12);
  found->Release();
  CHECK(f.shares.invariants_hold());
  f.shares.lose_direct3d11();
  CHECK_EQ(f.drained(), 0u);
  f.shares.clear();
  CHECK_EQ(f.drained(), 1u);
  CHECK_EQ(resource12.references(), 1u); }

TEST(host_shares_lose_direct3d12, gives_up_the_lost_device_s_resources_and_keeps_their_views)
{ CountedObject resource11, resource12, view12, shown12;
  Fixture f;
  CHECK_STATUS(f.shares.publish_resource(domain, 0x11u, &resource11, ShareOrigin::d3d11, f.publisher), Status::ok);
  CHECK_STATUS(f.shares.publish_resource(domain, 0x12u, &resource12, ShareOrigin::d3d12, f.publisher), Status::ok);
  CHECK(f.shares.keep_view(domain, 0x12u, &view12));
  CHECK(f.shares.keep_shown(domain, 0x12u, &shown12));
  f.shares.lose_direct3d12();
  CHECK_EQ(f.drained(), 1u);
  CHECK_EQ(resource12.references(), 1u);
  CHECK_EQ(view12.references(), 2u);
  CHECK_EQ(shown12.references(), 2u);
  IUnknown *const viewed = f.shares.view(domain, 0x12u);
  CHECK(viewed == &view12);
  viewed->Release();
  CHECK(f.shares.resource(domain, 0x12u) == nullptr);
  CHECK_EQ(resource11.references(), 2u);
  IUnknown *found = f.shares.open_resource(domain, 0x12u);
  CHECK(found == &shown12);
  found->Release();
  found = f.shares.open_resource(domain, 0x11u);
  CHECK(found == &resource11);
  found->Release();
  CHECK_EQ(f.shares.live(), 2u);
  f.shares.clear();
  CHECK_EQ(f.drained(), 3u); }

TEST(host_shares_restore_resource, a_resource_made_again_is_kept_by_the_share_it_was_published_as)
{ CountedObject resource11, resource12, object, remade, other;
  Fixture f;
  CHECK_STATUS(f.shares.publish_resource(domain, 0x11u, &resource11, ShareOrigin::d3d11, f.publisher), Status::ok);
  CHECK_STATUS(f.shares.publish_resource(domain, 0x12u, &resource12, ShareOrigin::d3d12, f.publisher), Status::ok);
  CHECK_STATUS(f.shares.publish_resource(domain + 1u, 0x13u, &object, ShareOrigin::d3d11, f.publisher), Status::ok);
  std::uint32_t visited = 0u;
  bool          saw_11 = false, saw_13 = false;
  f.shares.each_direct3d11([&](std::uint32_t in, std::uint64_t id, IUnknown *resource) {
      visited++;
      saw_11 = saw_11 || (in == domain && id == 0x11u && resource == &resource11);
      saw_13 = saw_13 || (in == domain + 1u && id == 0x13u && resource == &object);
  });
  CHECK_EQ(visited, 2u);
  CHECK(saw_11);
  CHECK(saw_13);
  CHECK(!f.shares.restore_resource(domain, 0x11u, &remade));
  f.shares.lose_direct3d11();
  CHECK(f.shares.restore_resource(domain, 0x11u, &remade));
  CHECK_EQ(remade.references(), 2u);
  IUnknown *const found = f.shares.open_resource(domain, 0x11u);
  CHECK(found == &remade);
  found->Release();
  CHECK(!f.shares.restore_resource(domain, 0x11u, &other));
  CHECK(!f.shares.restore_resource(domain, 0x12u, &other));
  CHECK(!f.shares.restore_resource(domain, 0x99u, &other));
  CHECK(!f.shares.restore_resource(domain + 1u, 0x13u, nullptr));
  CHECK_EQ(other.references(), 1u);
  f.shares.release(domain + 1u, 0x13u);
  CHECK(!f.shares.restore_resource(domain + 1u, 0x13u, &other));
  CHECK(f.shares.invariants_hold());
  f.shares.clear();
  static_cast<void>(f.drained());
  CHECK_EQ(remade.references(), 1u); }

TEST(host_shares, nothing_is_kept_for_an_unknown_id_or_no_object)
{ CountedObject object;
  Fixture f;
  CHECK(!f.shares.keep_view(domain, 0x2Au, &object));
  CHECK(!f.shares.keep_shown(domain, 0x2Au, &object));
  CHECK(f.shares.resource(domain, 0x2Au) == nullptr);
  CHECK(f.shares.view(domain, 0x2Au) == nullptr);
  CHECK(f.shares.shown(domain, 0x2Au) == nullptr);
  CHECK_STATUS(f.shares.publish_resource(domain, 0x2Au, &object, ShareOrigin::d3d12, f.publisher), Status::ok);
  CHECK(!f.shares.keep_view(domain, 0x2Au, nullptr));
  CHECK(!f.shares.keep_shown(domain, 0x2Au, nullptr));
  CHECK_EQ(object.references(), 2u); }

TEST(host_shares, a_reference_that_cannot_be_retired_is_released_at_once)
{ CountedObject resource, view, newer_view, shown, newer_shown;
  Fixture f;
  CHECK_STATUS(f.shares.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d12, f.publisher), Status::ok);
  CHECK(f.shares.keep_view(domain, 0x2Au, &view));
  f.platform.fail_allocation_at(1u);
  CHECK(f.shares.keep_view(domain, 0x2Au, &newer_view));
  CHECK_EQ(view.references(), 1u);
  CHECK_EQ(f.drained(), 0u);
  CHECK(f.shares.keep_shown(domain, 0x2Au, &shown));
  f.platform.fail_allocation_at(1u);
  CHECK(f.shares.keep_shown(domain, 0x2Au, &newer_shown));
  CHECK_EQ(shown.references(), 1u);
  f.platform.fail_allocation_at(1u);
  f.shares.clear();
  CHECK_EQ(f.drained(), 2u);
  CHECK_EQ(resource.references(), 1u);
  CHECK_EQ(newer_view.references(), 1u);
  CHECK_EQ(newer_shown.references(), 1u); }

TEST(host_shares, take_retired_hands_over_no_more_than_asked)
{ CountedObject resources[3];
  Fixture f;
  for (std::uint64_t id = 1u; id <= 3u; id++)
      CHECK_STATUS(f.shares.publish_resource(domain, id, &resources[id - 1u], ShareOrigin::d3d11, f.publisher),
                   Status::ok);
  f.shares.clear();
  IUnknown *objects[2] = {};
  CHECK_EQ(f.shares.take_retired(objects, 2u), 2u);
  objects[0]->Release();
  objects[1]->Release();
  CHECK_EQ(f.shares.take_retired(objects, 2u), 1u);
  objects[0]->Release();
  CHECK_EQ(f.shares.take_retired(objects, 2u), 0u); }

TEST(host_shares, destroying_the_table_releases_what_was_waiting)
{ MockPlatform platform;
  CountedObject resource;
  MockSessionBackend session;
  av::RecordingLog   log;
  av::DriverLog      lines(log, "shares: ");
  {
      HostShares shares(platform, lines);
      CHECK_STATUS(shares.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, session), Status::ok);
  }
  CHECK_EQ(resource.references(), 1u); }

TEST(host_shares, counts_the_copies_written_into_a_share)
{ CountedObject resource;
  Fixture f;
  std::uint64_t count = 9u;
  CHECK(!f.shares.writes(domain, 0x2Au, count));
  CHECK_EQ(count, 0u);
  f.shares.written(domain, 0x2Au);
  CHECK_STATUS(f.shares.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher), Status::ok);
  f.shares.written(domain, 0x2Au);
  f.shares.written(domain, 0x2Au);
  CHECK(f.shares.writes(domain, 0x2Au, count));
  CHECK_EQ(count, 2u); }

TEST(host_shares, copies_are_counted_per_id_and_start_again_with_the_id)
{ CountedObject first, second, view;
  Fixture f;
  std::uint64_t count = 0u;
  CHECK_STATUS(f.shares.publish_resource(domain, 5u, &first, ShareOrigin::d3d12, f.publisher), Status::ok);
  CHECK_STATUS(f.shares.publish_resource(domain, 6u, &second, ShareOrigin::d3d11, f.publisher), Status::ok);
  f.shares.written(domain, 5u);
  f.shares.written(domain, 6u);
  f.shares.written(domain, 6u);
  f.shares.written(domain + 1u, 5u);
  CHECK(f.shares.keep_view(domain, 5u, &view));
  f.shares.written(domain, 5u);
  CHECK(f.shares.writes(domain, 5u, count));
  CHECK_EQ(count, 2u);
  CHECK(f.shares.writes(domain, 6u, count));
  CHECK_EQ(count, 2u);
  f.shares.release(domain, 6u);
  CHECK(!f.shares.writes(domain, 6u, count));
  CHECK_STATUS(f.shares.publish_resource(domain, 6u, &second, ShareOrigin::d3d11, f.publisher), Status::ok);
  CHECK(f.shares.writes(domain, 6u, count));
  CHECK_EQ(count, 0u);
  f.shares.clear();
  CHECK_EQ(f.drained(), 4u); }

TEST(host_shares, a_published_resource_gains_its_descriptor_when_published_by_handle)
{ CountedObject resource;
  Fixture f;
  CHECK_STATUS(f.shares.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher), Status::ok);
  std::uint64_t made = 1u;
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.reader, made), Status::not_found);
  f.published(0x2Au);
  CHECK_EQ(f.publisher.exported_objects(), 1u);
  CHECK(f.opened(0x2Au, f.reader) != 0u);
  f.published(0x2Au);
  CHECK_EQ(f.publisher.exported_objects(), 1u);
  CHECK_STATUS(f.shares.publish(domain, 0x2Au, kind + 1u, size, f.publisher, handle), Status::conflict);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, a_resource_whose_backend_exports_nothing_stays_unexported)
{ CountedObject resource;
  Fixture f;
  CHECK_STATUS(f.shares.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher), Status::ok);
  f.publisher.set_shares(false);
  CHECK_STATUS(f.shares.publish(domain, 0x2Au, kind, size, f.publisher, handle), Status::not_supported);
  std::uint64_t made = 1u;
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.reader, made), Status::not_found); }

TEST(host_shares, a_descriptor_made_during_another_export_is_kept_once)
{ CountedObject resource;
  Fixture f;
  CHECK_STATUS(f.shares.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher), Status::ok);
  f.publisher.before_next_export(
      [](void *context) {
          auto *const fixture = static_cast<Fixture *>(context);
          fixture->published(0x2Au);
      },
      &f);
  f.published(0x2Au);
  CHECK_EQ(f.publisher.exported_objects(), 2u);
  CHECK(f.opened(0x2Au, f.reader) != 0u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, reports_every_share_by_origin_and_holder)
{ CountedObject resource, buffer;
  Fixture f;
  f.published(0x2Au);
  (void)f.opened(0x2Au, f.reader);
  CHECK_STATUS(f.shares.publish_resource(domain, 0x2Bu, &resource, ShareOrigin::d3d11, f.publisher), Status::ok);
  CHECK_STATUS(f.shares.publish_resource(domain, 0x2Cu, &buffer, ShareOrigin::d3d12, f.publisher), Status::ok);
  CHECK(reports(f.shares, "live=3"));
  CHECK(reports(f.shares, "objects=1"));
  CHECK(reports(f.shares, "d3d11=1"));
  CHECK(reports(f.shares, "d3d12=1"));
  CHECK(reports(f.shares, "holders=1"));
  CHECK(reports(f.shares, "published=true"));
  CHECK(reports(f.shares, "exported=true"));
  CHECK(reports(f.shares, "exported=false")); }

TEST(host_shares, a_report_while_the_table_is_held_says_so)
{ CountedObject resource;
  ReportingObject view;
  CountedObject newer_view;
  Fixture f;
  view.shares = &f.shares;
  CHECK_STATUS(f.shares.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d12, f.publisher), Status::ok);
  CHECK(f.shares.keep_view(domain, 0x2Au, &view));
  f.platform.fail_allocation_at(1u);
  CHECK(f.shares.keep_view(domain, 0x2Au, &newer_view));
  CHECK(view.saw_busy);
  CHECK(reports(f.shares, "busy=false") || !reports(f.shares, "busy=true")); }

TEST(host_shares, a_direct3d_open_of_an_object_with_no_resource_gets_nothing)
{ Fixture f;
  f.published(0x2Au);
  CHECK(f.shares.open_resource(domain, 0x2Au) == nullptr);
  CHECK(reports(f.shares, "opens=0")); }

TEST(host_shares, a_second_reset_leaves_what_a_session_still_holds)
{ Fixture f;
  f.published(0x2Au);
  (void)f.opened(0x2Au, f.reader);
  f.shares.release(domain, 0u);
  f.shares.release(domain, 0u);
  CHECK_EQ(f.shares.live(), 1u);
  CHECK(reports(f.shares, "unindexed=1"));
  CHECK(f.shares.invariants_hold());
  f.shares.session_closed(f.reader);
  CHECK_EQ(f.shares.live(), 0u); }

TEST(host_shares, the_logged_count_says_what_the_guest_released)
{ Fixture f;
  for (std::uint64_t id = 1u; id < 64u; id++) f.published(id);
  (void)f.opened(1u, f.reader);
  f.shares.release(domain, 1u);
  f.published(64u);
  CHECK_EQ(f.log.holding("shares live: 64 (64 objects, 0 d3d11, 0 d3d12, 1 released by the guest, 1 holders)"), 1u); }

TEST(host_shares_invariants, catch_a_lost_share_a_stray_entry_and_a_stray_holder)
{ Fixture f;
  f.published(0x2Au);
  av::HostSharesProbe probe(f.shares);
  probe.unindex(domain, 0x2Au);
  CHECK(!f.shares.invariants_hold());
  probe.reindex(domain, 0x2Au);
  CHECK(f.shares.invariants_hold());
  probe.index_nothing(domain, 0x2Bu);
  CHECK(!f.shares.invariants_hold());
  probe.forget_nothing(domain, 0x2Bu);
  CHECK(f.shares.invariants_hold());
  av::ShareHolder *const stray = probe.stray_holder(f.reader);
  CHECK(!f.shares.invariants_hold());
  probe.drop_stray(stray);
  CHECK(f.shares.invariants_hold()); }
