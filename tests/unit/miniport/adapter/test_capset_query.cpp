/* Reading the device's av capset, one request and one answer at a time, and
 * every answer a device might give judged. */
#include "core/platform/platform.hpp"
#include "miniport/adapter/capset_query.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/virtio_gpu/capset_request.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/virtio_gpu/av_request_writer.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::CapsetInfo;
using av::CapsetQuery;
using av::CapsetRequest;
using av::GpuIdentity;
using av::GpuResponse;
using av::Irql;
using av::MockPlatform;
using av::ProtocolVersion;
using av::AvRequestWriter;
using av::Status;
using av::SubApi;
using av::Violation;
using av::WireReader;

namespace av {

struct CapsetQueryProbe {
    static void complete_without_capset(CapsetQuery &query) { query.step_ = CapsetQuery::Step::done; }
};

}  // namespace av

namespace {

constexpr std::uint8_t uuid[GpuIdentity::uuid_bytes] = {5};

CapsetInfo capset(std::uint16_t major = 1u)
{
    return CapsetInfo(ProtocolVersion(major, 3u), static_cast<std::uint32_t>(SubApi::vulkan), 64u, 12u, 24u, 1u,
                      GpuIdentity(1u, 2u, GpuIdentity::cpu, uuid, 0u, 1u, "llvmpipe"));
}

class Fixture {
public:
    Fixture() : query_(platform_) { platform_.set_irql(Irql::dispatch); }
    ~Fixture() { platform_.set_irql(Irql::passive); }

    MockPlatform &platform() { return platform_; }
    CapsetQuery  &query() { return query_; }

    /* The next request, decoded: whether it was GET_CAPSET_INFO (else GET_CAPSET). */
    Status next(CapsetRequest &decoded, bool &info)
    {
        std::uint8_t bytes[AvRequestWriter::max_request_size] = {};
        std::size_t  length = 0u;
        const Status status = query_.next_request(bytes, sizeof bytes, length);
        if (status != Status::ok) return status;
        std::uint32_t type = 0u;
        std::memcpy(&type, bytes, sizeof type);
        info = type == 0x0108u;
        WireReader reader(bytes, length);
        return info ? decoded.decode_info(reader) : decoded.decode_capset(reader);
    }
    Status answer(const GpuResponse &response) { return query_.accept(response.bytes(), response.length()); }

private:
    MockPlatform platform_;
    CapsetQuery  query_;
};

GpuResponse info(std::uint32_t id = 0x72u, std::uint32_t version = 1u, std::uint32_t size = 128u)
{
    GpuResponse response;
    response.set_capset_info(id, version, size);
    return response;
}

GpuResponse whole(const CapsetInfo &value = capset())
{
    GpuResponse response;
    response.set_capset(value);
    return response;
}

GpuResponse plain(std::uint32_t type)
{
    GpuResponse response;
    response.set(type);
    return response;
}

}  // namespace

TEST(capset_query, reads_the_av_capset_in_two_requests)
{ Fixture fixture;
  CapsetRequest decoded;
  bool is_info = false;
  CHECK(!fixture.query().complete());
  CHECK_STATUS(fixture.next(decoded, is_info), Status::ok);
  CHECK(is_info);
  CHECK_EQ(decoded.index(), 0u);
  CHECK(fixture.query().invariants_hold());
  CHECK_STATUS(fixture.next(decoded, is_info), Status::invalid_state);   /* waiting for the answer */
  CHECK_STATUS(fixture.answer(info()), Status::ok);
  CHECK_STATUS(fixture.next(decoded, is_info), Status::ok);
  CHECK(!is_info);
  CHECK_EQ(decoded.capset_id(), 0x72u);
  CHECK_EQ(decoded.capset_version(), 1u);
  CHECK_STATUS(fixture.answer(whole()), Status::ok);
  CHECK(fixture.query().complete());
  CHECK(!fixture.query().failed());
  CHECK(fixture.query().invariants_hold());
  CHECK_EQ(fixture.query().capset().gpu().device_type(), GpuIdentity::cpu);
  CHECK_EQ(fixture.query().capset().version().minor_version(), 3u);
  CHECK_STATUS(fixture.next(decoded, is_info), Status::invalid_state);
  CHECK_STATUS(fixture.answer(whole()), Status::invalid_state); }

TEST(capset_query_accept, a_device_without_the_av_capset_or_of_another_major_is_not_supported)
{ struct Case {
      const char       *name;
      GpuResponse       first;
      GpuResponse       second;
      bool              fails_first;
  };
  const Case cases[] = {
      {"refused info", plain(GpuResponse::err_invalid_parameter), GpuResponse(), true},
      {"another capset", info(0x04u), GpuResponse(), true},
      {"no version", info(0x72u, 0u), GpuResponse(), true},
      {"refused capset", info(), plain(GpuResponse::err_invalid_parameter), false},
      {"another major", info(), whole(capset(2u)), false},
  };
  for (const Case &c : cases) {
      Fixture fixture;
      CapsetRequest decoded;
      bool is_info = false;
      CHECK_STATUS(fixture.next(decoded, is_info), Status::ok);
      Status status = fixture.answer(c.first);
      if (!c.fails_first) {
          CHECK_STATUS(status, Status::ok);
          CHECK_STATUS(fixture.next(decoded, is_info), Status::ok);
          status = fixture.answer(c.second);
      }
      if (status != Status::not_supported || !fixture.query().failed())
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, c.name);
      CHECK(!fixture.query().complete());
      CHECK_STATUS(fixture.next(decoded, is_info), Status::invalid_state);
      CHECK_EQ(fixture.platform().logged(), 1u);
  } }

TEST(capset_query_accept, an_answer_no_device_gives_is_malformed)
{ std::uint8_t short_bytes[10] = {};
  struct Case {
      const char  *name;
      GpuResponse  first;
      GpuResponse  second;
      bool         fails_first;
  };
  const Case cases[] = {
      {"info answered with nodata", plain(GpuResponse::ok_nodata), GpuResponse(), true},
      {"info smaller than a capset", info(0x72u, 1u, 127u), GpuResponse(), true},
      {"capset answered with info", info(), info(), false},
  };
  for (const Case &c : cases) {
      Fixture fixture;
      CapsetRequest decoded;
      bool is_info = false;
      CHECK_STATUS(fixture.next(decoded, is_info), Status::ok);
      Status status = fixture.answer(c.first);
      if (!c.fails_first) {
          CHECK_STATUS(fixture.next(decoded, is_info), Status::ok);
          status = fixture.answer(c.second);
      }
      if (status != Status::malformed || !fixture.query().failed())
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, c.name);
  }
  Fixture truncated_capset;
  CapsetRequest decoded;
  bool is_info = false;
  CHECK_STATUS(truncated_capset.next(decoded, is_info), Status::ok);
  CHECK_STATUS(truncated_capset.answer(info()), Status::ok);
  CHECK_STATUS(truncated_capset.next(decoded, is_info), Status::ok);
  const GpuResponse capset_answer = whole();
  CHECK_STATUS(truncated_capset.query().accept(capset_answer.bytes(), capset_answer.length() - 1u), Status::malformed);
  CHECK(truncated_capset.query().failed());
  Fixture truncated;
  CHECK_STATUS(truncated.next(decoded, is_info), Status::ok);
  CHECK_STATUS(truncated.query().accept(short_bytes, sizeof short_bytes), Status::malformed);
  CHECK(truncated.query().failed()); }

TEST(capset_query_next_request, a_destination_too_small_sends_nothing)
{ Fixture fixture;
  std::uint8_t bytes[16] = {};
  std::size_t  length = 99u;
  CHECK_STATUS(fixture.query().next_request(bytes, sizeof bytes, length), Status::too_large);
  CHECK_EQ(length, 0u);
  std::uint8_t enough[32] = {};
  CHECK_STATUS(fixture.query().next_request(enough, sizeof enough, length), Status::ok);
  CHECK_EQ(length, 32u); }

TEST(capset_query, runs_at_dispatch_at_most)
{ MockPlatform platform;
  CapsetQuery query(platform);
  platform.set_irql(Irql::isr);
  std::uint8_t bytes[32] = {};
  std::size_t  length = 0u;
  static_cast<void>(query.next_request(bytes, sizeof bytes, length));
  static_cast<void>(query.accept(bytes, 0u));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  CHECK(platform.claim(Violation::irql_ceiling)); }

TEST(capset_query_invariants, a_query_complete_without_a_capset_is_detected)
{ Fixture fixture;
  av::CapsetQueryProbe::complete_without_capset(fixture.query());
  CHECK(!fixture.query().invariants_hold()); }
