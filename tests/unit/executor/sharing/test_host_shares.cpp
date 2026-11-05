#include "core/log/driver_log.hpp"
#include "core/trace/state_writer.hpp"
#include "executor/sharing/d3d_shares.hpp"
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
using av::D3dShares;
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
    Fixture() : lines(log, "shares: "), shares(platform, lines), d3d(shares) {}

    void published(std::uint64_t id)
    {
        CHECK_STATUS(shares.publish(domain, id, kind, size, publisher, handle), Status::ok);
    }

    std::uint64_t opened(std::uint64_t id, MockSessionBackend &session)
    {
        std::uint64_t made = 0u;
        CHECK_STATUS(shares.open(domain, id, kind, size, session, 0u, made), Status::ok);
        return made;
    }

    std::uint32_t drained()
    {
        IUnknown     *objects[8] = {};
        std::uint32_t total = 0u;
        for (std::uint32_t taken = d3d.take_retired(objects, 8u); taken != 0u;
             taken = d3d.take_retired(objects, 8u)) {
            for (std::uint32_t index = 0u; index < taken; index++) objects[index]->Release();
            total += taken;
        }
        return total;
    }

    MockPlatform       platform;
    av::RecordingLog   log;
    av::DriverLog      lines;
    HostShares         shares;
    D3dShares          d3d;
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
          CHECK_STATUS(d->f->d3d.publish_resource(domain, 0x2Au, d->resource, ShareOrigin::d3d11, d->f->other),
                       Status::ok);
      },
      &during);
  CHECK_STATUS(f.shares.publish(domain, 0x2Au, kind, size, f.publisher, handle), Status::conflict);
  CHECK_EQ(f.shares.live(), 1u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, opening_what_is_not_shared_is_not_found)
{ Fixture f;
  std::uint64_t made = 1u;
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.reader, 0u, made), Status::not_found);
  CHECK_EQ(made, 0u);
  f.published(0x2Au);
  CHECK_STATUS(f.shares.open(domain + 1u, 0x2Au, kind, size, f.reader, 0u, made), Status::not_found);
  CHECK_EQ(f.reader.imported_objects(), 0u); }

TEST(host_shares, an_open_naming_another_kind_or_size_is_refused)
{ Fixture f;
  f.published(0x2Au);
  std::uint64_t made = 1u;
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind + 1u, size, f.reader, 0u, made), Status::invalid_parameter);
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size + 1u, f.reader, 0u, made), Status::invalid_parameter);
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
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.reader, 0u, made), Status::conflict);
  CHECK_EQ(made, 0u);
  CHECK_EQ(f.reader.imported_objects(), 1u); }

TEST(host_shares, a_session_opens_a_share_once_for_each_thing_it_is_made_into)
{ Fixture f;
  f.published(0x2Au);
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  std::uint64_t again = 1u;
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.reader, 0x7001u, first), Status::ok);
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.reader, 0x7002u, second), Status::ok);
  CHECK_EQ(first, 0x7001u);
  CHECK_EQ(second, 0x7002u);
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.reader, 0x7001u, again), Status::conflict);
  CHECK_EQ(again, 0u);
  CHECK_EQ(f.reader.imported_objects(), 2u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, the_session_that_published_a_share_opens_it_like_any_other)
{ Fixture f;
  f.published(0x2Au);
  std::uint64_t made = 0u;
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.publisher, 0x7001u, made), Status::ok);
  CHECK_EQ(made, 0x7001u);
  CHECK_EQ(f.publisher.imported_objects(), 1u);
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.publisher, 0x7001u, made), Status::conflict);
  f.shares.close(domain, 0x2Au, f.publisher, 0x7001u);
  CHECK_EQ(f.publisher.released_objects(), 1u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, a_backend_that_imports_nothing_opens_nothing)
{ Fixture f;
  f.published(0x2Au);
  f.reader.set_shares(false);
  std::uint64_t made = 1u;
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.reader, 0u, made), Status::not_supported);
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
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.reader, 0u, made), Status::no_memory);
  CHECK_EQ(made, 0u);
  CHECK_EQ(f.log.holding("a share cannot be opened, out of memory"), 1u);
  CHECK_EQ(f.reader.imported_objects(), 1u);
  CHECK_EQ(f.reader.released_objects(), 1u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, a_close_releases_only_what_was_imported)
{ Fixture f;
  f.published(0x2Au);
  const std::uint64_t made = f.opened(0x2Au, f.reader);
  f.shares.close(domain, 0x2Au, f.reader, made + 1u);
  f.shares.close(domain, 0x2Au, f.other, made);
  CHECK_EQ(f.reader.released_objects(), 0u);
  CHECK_EQ(f.other.released_objects(), 0u);
  f.shares.close(domain, 0x2Au, f.reader, made);
  CHECK_EQ(f.reader.released_objects(), 1u);
  CHECK_EQ(f.reader.last_released_detail(), (MockSessionBackend::object_handle + 1u) ^ MockSessionBackend::detail_mask);
  f.shares.close(domain, 0x2Au, f.publisher, made);
  CHECK_EQ(f.publisher.released_objects(), 0u);
  CHECK_EQ(f.shares.live(), 1u);
  f.shares.close(domain, 0x2Au, f.reader, made);
  f.shares.close(domain, 0x2Bu, f.reader, made);
  CHECK_EQ(f.reader.released_objects(), 1u);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, a_share_the_guest_released_lives_while_a_session_holds_it)
{ Fixture f;
  f.published(0x2Au);
  const std::uint64_t made = f.opened(0x2Au, f.reader);
  f.shares.release(domain, 0x2Au);
  CHECK_EQ(f.shares.live(), 1u);
  CHECK(reports(f.shares, "released_by_guest=1"));
  f.shares.close(domain, 0x2Au, f.publisher, made);
  CHECK_EQ(f.shares.live(), 1u);
  f.shares.close(domain, 0x2Au, f.reader, made);
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
  CHECK_STATUS(f.shares.open(domain, 0x2Bu, kind, size, f.other, 0u, made), Status::not_found);
  f.published(0x2Bu);
  CHECK_EQ(f.shares.live(), 3u);
  f.shares.close(domain, 0x2Bu, f.reader, MockSessionBackend::object_handle + 1u);
  CHECK_EQ(f.reader.released_objects(), 0u);
  f.shares.session_closed(f.reader);
  CHECK_EQ(f.shares.live(), 2u);
  CHECK(reports(f.shares, "unindexed=0"));
  CHECK_STATUS(f.shares.open(domain + 1u, 0x2Au, kind, size, f.reader, 0u, made), Status::ok);
  CHECK(f.shares.invariants_hold()); }

TEST(host_shares, a_publisher_ending_takes_no_one_else_s_hold)
{ Fixture f;
  f.published(0x2Au);
  const std::uint64_t made = f.opened(0x2Au, f.reader);
  f.shares.session_closed(f.publisher);
  f.shares.release(domain, 0x2Au);
  CHECK_EQ(f.shares.live(), 1u);
  CHECK_EQ(f.reader.released_objects(), 0u);
  f.shares.close(domain, 0x2Au, f.reader, made);
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
  CHECK_STATUS(f.d3d.publish_resource(domain + 1u, 0x2Bu, &resource, ShareOrigin::d3d11, f.other), Status::ok);
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
  CHECK_STATUS(f.d3d.publish_resource(domain, 64u, &resource, ShareOrigin::d3d12, f.publisher), Status::ok);
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

TEST(host_shares, destroying_the_table_releases_what_was_waiting)
{ MockPlatform platform;
  CountedObject resource;
  MockSessionBackend session;
  av::RecordingLog   log;
  av::DriverLog      lines(log, "shares: ");
  {
      HostShares shares(platform, lines);
      av::D3dShares d3d(shares);
      CHECK_STATUS(d3d.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, session), Status::ok);
  }
  CHECK_EQ(resource.references(), 1u); }

TEST(host_shares, a_published_resource_gains_its_descriptor_when_published_by_handle)
{ CountedObject resource;
  Fixture f;
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher), Status::ok);
  std::uint64_t made = 1u;
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.reader, 0u, made), Status::not_found);
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
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher), Status::ok);
  f.publisher.set_shares(false);
  CHECK_STATUS(f.shares.publish(domain, 0x2Au, kind, size, f.publisher, handle), Status::not_supported);
  std::uint64_t made = 1u;
  CHECK_STATUS(f.shares.open(domain, 0x2Au, kind, size, f.reader, 0u, made), Status::not_found); }

TEST(host_shares, a_descriptor_made_during_another_export_is_kept_once)
{ CountedObject resource;
  Fixture f;
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher), Status::ok);
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
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x2Bu, &resource, ShareOrigin::d3d11, f.publisher), Status::ok);
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x2Cu, &buffer, ShareOrigin::d3d12, f.publisher), Status::ok);
  CHECK(reports(f.shares, "live=3"));
  CHECK(reports(f.shares, "objects=1"));
  CHECK(reports(f.shares, "d3d11=1"));
  CHECK(reports(f.shares, "d3d12=1"));
  CHECK(reports(f.shares, "holders=1"));
  CHECK(reports(f.shares, "published=true"));
  CHECK(reports(f.shares, "exported=true"));
  CHECK(reports(f.shares, "exported=false")); }

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
