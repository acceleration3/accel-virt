#include "core/support/status.hpp"
#include "guest/transport/guest_connection.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "icd/d3d/d3d_command_stream.hpp"
#include "icd/d3d/d3d_device_selection.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/generated/d3d/d3d_dispatch_client.h"
#include "proto/generated/d3d/d3d_protocol_ids.h"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>

using av::GuestConnection;
using av::KmtTransport;
using av::MockKmt;
using av::PciIdentity;
using av::Status;
using av::SubApi;
using av::d3d::D3dCommandStream;
using av::d3d::D3dDeviceSelection;
using namespace av::d3dproto;

namespace {

const PciIdentity       avgpu(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u);
constexpr std::size_t   staging_bytes = 1024u;
constexpr std::size_t   block_bytes = 8192u;
constexpr std::uint32_t device_a = 3u;
constexpr std::uint32_t device_b = 5u;

class Fixture {
public:
    Fixture()
        : connection_(kmt_), transport_(connection_, SubApi::d3d, 16u, 4096u, staging_, staging_bytes),
          stream_(transport_), a_(stream_, device_a), b_(stream_, device_b), twelve_(stream_, 0u)
    {
        kmt_.add_adapter(avgpu, 3200u);
        std::uint32_t adapters[1] = {};
        std::uint32_t count = 0u;
        static_cast<void>(kmt_.enumerate_adapters(adapters, 1u, count));
        CHECK_STATUS(connection_.open(adapters[0]), Status::ok);
        CHECK_STATUS(transport_.initialize(), Status::ok);
        transport_.hold_in(block_, block_bytes);
    }
    ~Fixture() { transport_.teardown(); }

    MockKmt            &kmt() { return kmt_; }
    KmtTransport       &transport() { return transport_; }
    D3dCommandStream   &stream() { return stream_; }
    D3dDeviceSelection &a() { return a_; }
    D3dDeviceSelection &b() { return b_; }
    D3dDeviceSelection &twelve() { return twelve_; }

private:
    MockKmt            kmt_;
    GuestConnection    connection_;
    std::uint8_t       staging_[staging_bytes] = {};
    KmtTransport       transport_;
    std::uint8_t       block_[block_bytes] = {};
    D3dCommandStream   stream_;
    D3dDeviceSelection a_;
    D3dDeviceSelection b_;
    D3dDeviceSelection twelve_;
};

}

TEST(d3d_device_selection, a_command_goes_behind_its_devices_selection_when_another_spoke_last)
{ Fixture fixture;
  CHECK(av_d3d_Draw(fixture.a(), 3u, 0u));
  CHECK(av_d3d_Draw(fixture.a(), 4u, 0u));
  CHECK(av_d3d_Draw(fixture.b(), 5u, 0u));
  CHECK(av_d3d_Draw(fixture.twelve(), 6u, 0u));
  CHECK_STATUS(fixture.stream().flush(), Status::ok);
  const std::uint32_t expected[] = {kCmd_SelectDevice11, kCmd_Draw, kCmd_Draw, kCmd_SelectDevice11, kCmd_Draw,
                                    kCmd_Draw};
  CHECK_EQ(fixture.kmt().commands(), 6u);
  for (std::uint32_t index = 0u; index < 6u; index++) CHECK_EQ(fixture.kmt().command_id(index), expected[index]);
  CHECK(fixture.stream().invariants_hold()); }

TEST(d3d_device_selection, an_answered_call_and_the_arenas_are_the_streams)
{ Fixture fixture;
  const std::uint32_t removed = 0x887A0005u;
  std::uint8_t        answer[sizeof removed] = {};
  __builtin_memcpy(answer, &removed, sizeof removed);
  fixture.kmt().queue_answer(answer, sizeof answer);
  std::uint32_t answered = 0u;
  CHECK(av_d3d_Finish11(fixture.a(), &answered));
  CHECK_EQ(answered, removed);
  CHECK_EQ(fixture.kmt().command_id(0u), static_cast<std::uint32_t>(kCmd_SelectDevice11));
  CHECK_EQ(fixture.kmt().command_id(1u), static_cast<std::uint32_t>(kCmd_Finish11));
  CHECK(&fixture.a().blobs() == &fixture.transport().blobs());
  CHECK(&fixture.a().decode_arena() == &fixture.transport().decode_arena()); }
