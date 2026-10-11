/* What one session shares through: its GPU's HostShares, in its domain, as
 * that session. Unbound it shares nothing and opens nothing, which is a
 * session slot nobody opened. */
#include "core/log/driver_log.hpp"
#include "executor/sharing/host_shares.hpp"
#include "executor/sharing/session_shares.hpp"
#include "harness/executor/counted_object.hpp"
#include "harness/executor/mock_session_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::CountedObject;
using av::DriverLog;
using av::HostShares;
using av::MockPlatform;
using av::MockSessionBackend;
using av::RecordingLog;
using av::SessionShares;
using av::ShareOrigin;
using av::Status;

namespace {

constexpr std::uint32_t domain = 3u;
constexpr std::uint32_t kind = 7u;
constexpr std::uint64_t size = 4096u;

}  // namespace

TEST(session_shares, unbound_shares_and_opens_nothing)
{ SessionShares unbound;
  CountedObject resource;
  CHECK(!unbound.bound());
  CHECK_EQ(unbound.domain(), 0u);
  CHECK(unbound.shares() == nullptr);
  CHECK_STATUS(unbound.publish(0x2Au, kind, size, 0x40u), Status::invalid_state);
  std::uint64_t made = 9u;
  CHECK_STATUS(unbound.open(0x2Au, kind, size, made), Status::invalid_state);
  CHECK_EQ(made, 0u);
  CHECK_STATUS(unbound.publish_resource(0x2Au, &resource, ShareOrigin::d3d11), Status::invalid_state);
  unbound.close(0x2Au);
  unbound.written(0x2Au);
  unbound.end();
  CHECK_EQ(resource.references(), 1u); }

/* Bound, each call is the session's in its domain: what it publishes another
 * session opens there, and nowhere else. */
TEST(session_shares, bound_it_shares_and_opens_as_its_session_in_its_domain)
{ CountedObject      resource;
  MockPlatform       platform;
  RecordingLog       log;
  DriverLog          lines(log, "shares: ");
  HostShares         shares(platform, lines);
  MockSessionBackend maker_backend, reader_backend;
  SessionShares      maker, reader, elsewhere;
  maker.bind(shares, domain, maker_backend);
  reader.bind(shares, domain, reader_backend);
  elsewhere.bind(shares, domain + 1u, reader_backend);
  CHECK(maker.bound());
  CHECK_EQ(maker.domain(), domain);
  CHECK(maker.shares() == &shares);

  CHECK_STATUS(maker.publish(0x2Au, kind, size, 0x40u), Status::ok);
  std::uint64_t made = 0u;
  CHECK_STATUS(elsewhere.open(0x2Au, kind, size, made), Status::not_found);
  CHECK_STATUS(reader.open(0x2Au, kind, size, made), Status::ok);
  CHECK(made != 0u);
  reader.close(0x2Au);
  CHECK_EQ(reader_backend.released_objects(), 1u);

  CHECK_STATUS(maker.publish_resource(0x2Bu, &resource, ShareOrigin::d3d11), Status::ok);
  maker.written(0x2Bu);
  std::uint64_t count = 0u;
  CHECK(shares.writes(domain, 0x2Bu, count));
  CHECK_EQ(count, 1u);

  /* Its end releases what it imported, through its backend, and leaves it
   * unbound for the next session in its slot. */
  CHECK_STATUS(reader.open(0x2Au, kind, size, made), Status::ok);
  reader.end();
  CHECK(!reader.bound());
  CHECK_EQ(reader.domain(), 0u);
  CHECK_EQ(reader_backend.released_objects(), 2u);
  shares.release(domain, 0x2Au);
  CHECK_EQ(shares.live(), 1u);   /* 0x2B, the guest's still */
  shares.clear();
  IUnknown *objects[1] = {};
  while (shares.take_retired(objects, 1u) != 0u) objects[0]->Release();
  CHECK_EQ(resource.references(), 1u); }
