#include "core/log/driver_log.hpp"
#include "executor/sharing/d3d_shares.hpp"
#include "executor/sharing/host_shares.hpp"
#include "executor/sharing/session_shares.hpp"
#include "executor/sharing/shared_image_shape.hpp"
#include "harness/executor/counted_object.hpp"
#include "harness/executor/mock_session_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"

#include <stdint.h>

using av::CountedObject;
using av::D3dShares;
using av::DriverLog;
using av::HostShares;
using av::MockPlatform;
using av::MockSessionBackend;
using av::RecordingLog;
using av::SessionShares;
using av::SharedImageShape;
using av::ShareOrigin;
using av::Status;

namespace {

constexpr uint32_t domain = 3u;
constexpr uint32_t kind = 7u;
constexpr uint64_t size = 4096u;

}

TEST(session_shares, unbound_shares_and_opens_nothing)
{ SessionShares unbound;
  CountedObject resource;
  CHECK(!unbound.bound());
  CHECK_EQ(unbound.domain(), 0u);
  CHECK(unbound.shares() == nullptr);
  CHECK_STATUS(unbound.publish(0x2Au, kind, size, 0x40u), Status::invalid_state);
  uint64_t made = 9u;
  CHECK_STATUS(unbound.open(0x2Au, kind, size, 0u, made), Status::invalid_state);
  CHECK_EQ(made, 0u);
  CHECK_STATUS(unbound.publish_resource(0x2Au, &resource, ShareOrigin::d3d11), Status::invalid_state);
  unbound.close(0x2Au, 0x9001u);
  unbound.written(0x2Au);
  CHECK(!unbound.describe_image(0x2Au, SharedImageShape(4u, 4u, 87u, 16u)));
  unbound.end();
  CHECK_EQ(resource.references(), 1u); }

TEST(session_shares, bound_it_shares_and_opens_as_its_session_in_its_domain)
{ CountedObject      resource;
  MockPlatform       platform;
  RecordingLog       log;
  DriverLog          lines(log, "shares: ");
  HostShares         shares(platform, lines);
  D3dShares          d3d(shares);
  MockSessionBackend maker_backend, reader_backend;
  SessionShares      maker, reader, elsewhere;
  maker.bind(shares, domain, maker_backend);
  reader.bind(shares, domain, reader_backend);
  elsewhere.bind(shares, domain + 1u, reader_backend);
  CHECK(maker.bound());
  CHECK_EQ(maker.domain(), domain);
  CHECK(maker.shares() == &shares);

  CHECK_STATUS(maker.publish(0x2Au, kind, size, 0x40u), Status::ok);
  uint64_t made = 0u;
  CHECK_STATUS(elsewhere.open(0x2Au, kind, size, 0u, made), Status::not_found);
  CHECK_STATUS(reader.open(0x2Au, kind, size, 0u, made), Status::ok);
  CHECK_EQ(made, MockSessionBackend::object_handle + 1u);
  reader.close(0x2Au, made);
  CHECK_EQ(reader_backend.released_objects(), 1u);

  CHECK_STATUS(maker.publish_resource(0x2Bu, &resource, ShareOrigin::d3d11), Status::ok);
  maker.written(0x2Bu);
  uint64_t count = 0u;
  CHECK(d3d.writes(domain, 0x2Bu, count));
  CHECK_EQ(count, 1u);

  CHECK_STATUS(reader.open(0x2Au, kind, size, 0u, made), Status::ok);
  reader.end();
  CHECK(!reader.bound());
  CHECK_EQ(reader.domain(), 0u);
  CHECK_EQ(reader_backend.released_objects(), 2u);
  shares.release(domain, 0x2Au);
  CHECK_EQ(shares.live(), 1u);
  shares.clear();
  IUnknown *objects[1] = {};
  while (d3d.take_retired(objects, 1u) != 0u) objects[0]->Release();
  CHECK_EQ(resource.references(), 1u); }

TEST(session_shares, an_open_names_what_it_is_made_into_and_a_close_names_the_open)
{ MockPlatform       platform;
  RecordingLog       log;
  DriverLog          lines(log, "shares: ");
  HostShares         shares(platform, lines);
  MockSessionBackend maker_backend, reader_backend;
  SessionShares      maker, reader;
  maker.bind(shares, domain, maker_backend);
  reader.bind(shares, domain, reader_backend);
  CHECK_STATUS(maker.publish(0x2Au, kind, size, 0x40u), Status::ok);
  uint64_t into_device = 0u;
  uint64_t into_semaphore = 0u;
  CHECK_STATUS(reader.open(0x2Au, kind, size, 0x7001u, into_device), Status::ok);
  CHECK_STATUS(reader.open(0x2Au, kind, size, 0x7002u, into_semaphore), Status::ok);
  CHECK_EQ(into_device, 0x7001u);
  CHECK_EQ(into_semaphore, 0x7002u);
  uint64_t again = 0u;
  CHECK_STATUS(reader.open(0x2Au, kind, size, 0x7001u, again), Status::conflict);
  CHECK_EQ(again, 0u);
  reader.close(0x2Au, 0x7001u);
  CHECK_EQ(reader_backend.released_objects(), 1u);
  CHECK_EQ(reader_backend.last_released_detail(), 0x7001u ^ MockSessionBackend::detail_mask);
  reader.close(0x2Au, 0x7001u);
  CHECK_EQ(reader_backend.released_objects(), 1u);
  CHECK_STATUS(reader.open(0x2Au, kind, size, 0x7001u, again), Status::ok);
  reader.end();
  CHECK_EQ(reader_backend.released_objects(), 3u);
  CHECK(shares.invariants_hold()); }

TEST(session_shares, describes_the_image_of_a_share_in_its_domain)
{ MockPlatform       platform;
  RecordingLog       log;
  DriverLog          lines(log, "shares: ");
  HostShares         shares(platform, lines);
  MockSessionBackend maker_backend, reader_backend;
  SessionShares      maker, reader;
  maker.bind(shares, domain, maker_backend);
  reader.bind(shares, domain, reader_backend);
  CHECK(!maker.describe_image(0x2Au, SharedImageShape(64u, 32u, 87u, 256u)));
  CHECK_STATUS(maker.publish(0x2Au, kind, size, 0x40u), Status::ok);
  CHECK(maker.describe_image(0x2Au, SharedImageShape(64u, 32u, 87u, 256u)));
  uint64_t made = 0u;
  CHECK_STATUS(reader.open(0x2Au, kind, size, 0u, made), Status::ok);
  CHECK(reader_backend.imported_shape().described());
  CHECK_EQ(reader_backend.imported_shape().width(), 64u);
  CHECK_EQ(reader_backend.imported_shape().height(), 32u);
  CHECK_EQ(reader_backend.imported_shape().pitch(), 256u);
  maker.end();
  reader.end();
  CHECK_EQ(shares.live(), 1u);
  shares.release(domain, 0x2Au);
  CHECK_EQ(shares.live(), 0u); }
