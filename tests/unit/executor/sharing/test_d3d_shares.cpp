#include "core/log/driver_log.hpp"
#include "core/trace/state_writer.hpp"
#include "executor/sharing/d3d_shares.hpp"
#include "executor/sharing/host_shares.hpp"
#include "harness/executor/counted_object.hpp"
#include "harness/executor/mock_session_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstring>
#include <stdint.h>

using av::CountedObject;
using av::D3dShares;
using av::HostShares;
using av::MockPlatform;
using av::MockSessionBackend;
using av::ShareOrigin;
using av::StateWriter;
using av::Status;

namespace {

constexpr uint32_t domain = 1u;
constexpr uint32_t kind = 7u;
constexpr uint64_t size = 4096u;
constexpr uint64_t handle = 0x2001u;

class Fixture {
public:
    Fixture() : lines(log, "shares: "), shares(platform, lines), d3d(shares) {}

    void published(uint64_t id)
    {
        CHECK_STATUS(shares.publish(domain, id, kind, size, publisher, handle), Status::ok);
    }

    IUnknown *opened_resource(uint64_t id)
    {
        bool live = false;
        return d3d.open_resource(domain, id, live);
    }

    uint32_t drained()
    {
        IUnknown *objects[8] = {};
        uint32_t  total = 0u;
        for (uint32_t taken = d3d.take_retired(objects, 8u); taken != 0u; taken = d3d.take_retired(objects, 8u)) {
            for (uint32_t index = 0u; index < taken; index++) objects[index]->Release();
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

TEST(d3d_shares, a_published_resource_opens_with_a_reference_of_the_caller_s)
{ CountedObject resource;
  Fixture f;
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher), Status::ok);
  CHECK_EQ(resource.references(), 2u);
  IUnknown *const opened = f.opened_resource(0x2Au);
  CHECK(opened == &resource);
  CHECK_EQ(resource.references(), 3u);
  opened->Release();
  CHECK(f.opened_resource(0x2Bu) == nullptr);
  CHECK(f.shares.invariants_hold()); }

TEST(d3d_shares, publishing_a_resource_needs_an_id_and_a_resource)
{ CountedObject resource;
  Fixture f;
  CHECK_STATUS(f.d3d.publish_resource(domain, 0u, &resource, ShareOrigin::d3d11, f.publisher),
               Status::invalid_parameter);
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x2Au, nullptr, ShareOrigin::d3d11, f.publisher),
               Status::invalid_parameter);
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher), Status::ok);
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.other), Status::conflict);
  CHECK_EQ(resource.references(), 2u); }

TEST(d3d_shares, a_resource_publish_with_no_memory_holds_no_reference)
{ CountedObject resource;
  Fixture f;
  f.platform.fail_allocation_at(1u);
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher),
               Status::no_memory);
  f.platform.fail_allocation_at(2u);
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher),
               Status::no_memory);
  CHECK_EQ(f.shares.live(), 0u);
  CHECK_EQ(resource.references(), 1u);
  CHECK(f.shares.invariants_hold()); }

TEST(d3d_shares, a_released_resource_lives_on_by_its_openers_references)
{ CountedObject resource;
  Fixture f;
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher), Status::ok);
  IUnknown *const opened = f.opened_resource(0x2Au);
  f.opened_resource(0x2Au)->Release();
  CHECK(reports(f.shares, "opens=2"));
  f.shares.release(domain, 0x2Au);
  CHECK_EQ(f.shares.live(), 0u);
  CHECK(f.opened_resource(0x2Au) == nullptr);
  CHECK_EQ(resource.references(), 3u);
  CHECK_EQ(f.drained(), 1u);
  CHECK_EQ(resource.references(), 2u);
  opened->Release();
  CHECK(f.shares.invariants_hold()); }

TEST(d3d_shares, opens_as_the_flip_copy_then_the_view_then_the_resource)
{ CountedObject resource, view, newer_view, shown, newer_shown;
  Fixture f;
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d12, f.publisher), Status::ok);
  IUnknown *found = f.d3d.view(domain, 0x2Au);
  CHECK(found == &resource);
  found->Release();
  CHECK(f.d3d.keep_view(domain, 0x2Au, &view));
  CHECK(f.d3d.keep_view(domain, 0x2Au, &newer_view));
  CHECK_EQ(view.references(), 2u);
  CHECK_EQ(resource.references(), 2u);
  found = f.opened_resource(0x2Au);
  CHECK(found == &newer_view);
  found->Release();
  CHECK(f.d3d.shown(domain, 0x2Au) == nullptr);
  CHECK(f.d3d.keep_shown(domain, 0x2Au, &shown));
  CHECK(f.d3d.keep_shown(domain, 0x2Au, &newer_shown));
  found = f.d3d.shown(domain, 0x2Au);
  CHECK(found == &newer_shown);
  found->Release();
  found = f.opened_resource(0x2Au);
  CHECK(found == &newer_shown);
  found->Release();
  found = f.d3d.resource(domain, 0x2Au);
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

TEST(d3d_shares_lose_direct3d11, gives_up_the_lost_device_s_objects_and_keeps_the_shares)
{ CountedObject resource11, shown11, resource12, view12, shown12;
  Fixture f;
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x11u, &resource11, ShareOrigin::d3d11, f.publisher), Status::ok);
  CHECK(f.d3d.keep_shown(domain, 0x11u, &shown11));
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x12u, &resource12, ShareOrigin::d3d12, f.publisher), Status::ok);
  CHECK(f.d3d.keep_view(domain, 0x12u, &view12));
  CHECK(f.d3d.keep_shown(domain, 0x12u, &shown12));
  f.d3d.lose_direct3d11();
  CHECK_EQ(f.drained(), 4u);
  CHECK_EQ(resource11.references(), 1u);
  CHECK_EQ(shown11.references(), 1u);
  CHECK_EQ(view12.references(), 1u);
  CHECK_EQ(shown12.references(), 1u);
  CHECK_EQ(resource12.references(), 2u);
  CHECK_EQ(f.shares.live(), 2u);
  CHECK(f.opened_resource(0x11u) == nullptr);
  IUnknown *const found = f.opened_resource(0x12u);
  CHECK(found == &resource12);
  found->Release();
  CHECK(f.shares.invariants_hold());
  f.d3d.lose_direct3d11();
  CHECK_EQ(f.drained(), 0u);
  f.shares.clear();
  CHECK_EQ(f.drained(), 1u);
  CHECK_EQ(resource12.references(), 1u); }

TEST(d3d_shares_lose_direct3d12, gives_up_the_lost_device_s_resources_and_keeps_their_views)
{ CountedObject resource11, resource12, view12, shown12;
  Fixture f;
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x11u, &resource11, ShareOrigin::d3d11, f.publisher), Status::ok);
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x12u, &resource12, ShareOrigin::d3d12, f.publisher), Status::ok);
  CHECK(f.d3d.keep_view(domain, 0x12u, &view12));
  CHECK(f.d3d.keep_shown(domain, 0x12u, &shown12));
  f.d3d.lose_direct3d12();
  CHECK_EQ(f.drained(), 1u);
  CHECK_EQ(resource12.references(), 1u);
  CHECK_EQ(view12.references(), 2u);
  CHECK_EQ(shown12.references(), 2u);
  IUnknown *const viewed = f.d3d.view(domain, 0x12u);
  CHECK(viewed == &view12);
  viewed->Release();
  CHECK(f.d3d.resource(domain, 0x12u) == nullptr);
  CHECK_EQ(resource11.references(), 2u);
  IUnknown *found = f.opened_resource(0x12u);
  CHECK(found == &shown12);
  found->Release();
  found = f.opened_resource(0x11u);
  CHECK(found == &resource11);
  found->Release();
  CHECK_EQ(f.shares.live(), 2u);
  f.shares.clear();
  CHECK_EQ(f.drained(), 3u); }

TEST(d3d_shares_restore_resource, a_resource_made_again_is_kept_by_the_share_it_was_published_as)
{ CountedObject resource11, resource12, object, remade, other;
  Fixture f;
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x11u, &resource11, ShareOrigin::d3d11, f.publisher), Status::ok);
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x12u, &resource12, ShareOrigin::d3d12, f.publisher), Status::ok);
  CHECK_STATUS(f.d3d.publish_resource(domain + 1u, 0x13u, &object, ShareOrigin::d3d11, f.publisher), Status::ok);
  uint32_t visited = 0u;
  bool          saw_11 = false, saw_13 = false;
  f.d3d.each_direct3d11([&](uint32_t in, uint64_t id, IUnknown *resource) {
      visited++;
      saw_11 = saw_11 || (in == domain && id == 0x11u && resource == &resource11);
      saw_13 = saw_13 || (in == domain + 1u && id == 0x13u && resource == &object);
  });
  CHECK_EQ(visited, 2u);
  CHECK(saw_11);
  CHECK(saw_13);
  CHECK(!f.d3d.restore_resource(domain, 0x11u, &remade));
  f.d3d.lose_direct3d11();
  CHECK(f.d3d.restore_resource(domain, 0x11u, &remade));
  CHECK_EQ(remade.references(), 2u);
  IUnknown *const found = f.opened_resource(0x11u);
  CHECK(found == &remade);
  found->Release();
  CHECK(!f.d3d.restore_resource(domain, 0x11u, &other));
  CHECK(!f.d3d.restore_resource(domain, 0x12u, &other));
  CHECK(!f.d3d.restore_resource(domain, 0x99u, &other));
  CHECK(!f.d3d.restore_resource(domain + 1u, 0x13u, nullptr));
  CHECK_EQ(other.references(), 1u);
  f.shares.release(domain + 1u, 0x13u);
  CHECK(!f.d3d.restore_resource(domain + 1u, 0x13u, &other));
  CHECK(f.shares.invariants_hold());
  f.shares.clear();
  static_cast<void>(f.drained());
  CHECK_EQ(remade.references(), 1u); }

TEST(d3d_shares, nothing_is_kept_for_an_unknown_id_or_no_object)
{ CountedObject object;
  Fixture f;
  CHECK(!f.d3d.keep_view(domain, 0x2Au, &object));
  CHECK(!f.d3d.keep_shown(domain, 0x2Au, &object));
  CHECK(f.d3d.resource(domain, 0x2Au) == nullptr);
  CHECK(f.d3d.view(domain, 0x2Au) == nullptr);
  CHECK(f.d3d.shown(domain, 0x2Au) == nullptr);
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x2Au, &object, ShareOrigin::d3d12, f.publisher), Status::ok);
  CHECK(!f.d3d.keep_view(domain, 0x2Au, nullptr));
  CHECK(!f.d3d.keep_shown(domain, 0x2Au, nullptr));
  CHECK_EQ(object.references(), 2u); }

TEST(d3d_shares, a_reference_that_cannot_be_retired_is_released_at_once)
{ CountedObject resource, view, newer_view, shown, newer_shown;
  Fixture f;
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d12, f.publisher), Status::ok);
  CHECK(f.d3d.keep_view(domain, 0x2Au, &view));
  f.platform.fail_allocation_at(1u);
  CHECK(f.d3d.keep_view(domain, 0x2Au, &newer_view));
  CHECK_EQ(view.references(), 1u);
  CHECK_EQ(f.drained(), 0u);
  CHECK(f.d3d.keep_shown(domain, 0x2Au, &shown));
  f.platform.fail_allocation_at(1u);
  CHECK(f.d3d.keep_shown(domain, 0x2Au, &newer_shown));
  CHECK_EQ(shown.references(), 1u);
  f.platform.fail_allocation_at(1u);
  f.shares.clear();
  CHECK_EQ(f.drained(), 2u);
  CHECK_EQ(resource.references(), 1u);
  CHECK_EQ(newer_view.references(), 1u);
  CHECK_EQ(newer_shown.references(), 1u); }

TEST(d3d_shares, take_retired_hands_over_no_more_than_asked)
{ CountedObject resources[3];
  Fixture f;
  for (uint64_t id = 1u; id <= 3u; id++)
      CHECK_STATUS(f.d3d.publish_resource(domain, id, &resources[id - 1u], ShareOrigin::d3d11, f.publisher),
                   Status::ok);
  f.shares.clear();
  IUnknown *objects[2] = {};
  CHECK_EQ(f.d3d.take_retired(objects, 2u), 2u);
  objects[0]->Release();
  objects[1]->Release();
  CHECK_EQ(f.d3d.take_retired(objects, 2u), 1u);
  objects[0]->Release();
  CHECK_EQ(f.d3d.take_retired(objects, 2u), 0u); }

TEST(d3d_shares, counts_the_copies_written_into_a_share)
{ CountedObject resource;
  Fixture f;
  uint64_t count = 9u;
  CHECK(!f.d3d.writes(domain, 0x2Au, count));
  CHECK_EQ(count, 0u);
  f.d3d.written(domain, 0x2Au);
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d11, f.publisher), Status::ok);
  f.d3d.written(domain, 0x2Au);
  f.d3d.written(domain, 0x2Au);
  CHECK(f.d3d.writes(domain, 0x2Au, count));
  CHECK_EQ(count, 2u); }

TEST(d3d_shares, copies_are_counted_per_id_and_start_again_with_the_id)
{ CountedObject first, second, view;
  Fixture f;
  uint64_t count = 0u;
  CHECK_STATUS(f.d3d.publish_resource(domain, 5u, &first, ShareOrigin::d3d12, f.publisher), Status::ok);
  CHECK_STATUS(f.d3d.publish_resource(domain, 6u, &second, ShareOrigin::d3d11, f.publisher), Status::ok);
  f.d3d.written(domain, 5u);
  f.d3d.written(domain, 6u);
  f.d3d.written(domain, 6u);
  f.d3d.written(domain + 1u, 5u);
  CHECK(f.d3d.keep_view(domain, 5u, &view));
  f.d3d.written(domain, 5u);
  CHECK(f.d3d.writes(domain, 5u, count));
  CHECK_EQ(count, 2u);
  CHECK(f.d3d.writes(domain, 6u, count));
  CHECK_EQ(count, 2u);
  f.shares.release(domain, 6u);
  CHECK(!f.d3d.writes(domain, 6u, count));
  CHECK_STATUS(f.d3d.publish_resource(domain, 6u, &second, ShareOrigin::d3d11, f.publisher), Status::ok);
  CHECK(f.d3d.writes(domain, 6u, count));
  CHECK_EQ(count, 0u);
  f.shares.clear();
  CHECK_EQ(f.drained(), 4u); }

TEST(d3d_shares, a_report_while_the_table_is_held_says_so)
{ CountedObject resource;
  ReportingObject view;
  CountedObject newer_view;
  Fixture f;
  view.shares = &f.shares;
  CHECK_STATUS(f.d3d.publish_resource(domain, 0x2Au, &resource, ShareOrigin::d3d12, f.publisher), Status::ok);
  CHECK(f.d3d.keep_view(domain, 0x2Au, &view));
  f.platform.fail_allocation_at(1u);
  CHECK(f.d3d.keep_view(domain, 0x2Au, &newer_view));
  CHECK(view.saw_busy);
  CHECK(!reports(f.shares, "busy=true")); }

TEST(d3d_shares, a_direct3d_open_of_an_object_with_no_resource_gets_nothing)
{ Fixture f;
  f.published(0x2Au);
  CHECK(f.opened_resource(0x2Au) == nullptr);
  CHECK(reports(f.shares, "opens=0")); }
