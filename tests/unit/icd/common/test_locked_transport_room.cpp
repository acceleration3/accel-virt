#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/transport/mock_transport.hpp"
#include "icd/common/locked_transport.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::LockTrace;
using av::MockTransport;
using av::RecordingThreadLock;
using av::TransportResult;
using av::WireWriter;
using av::icd::LockedTransport;

namespace {

constexpr std::uint32_t command = 0x1234u;

}

TEST(locked_transport, room_and_writer_are_one_hold_so_no_other_thread_can_use_the_room_between_them)
{ MockTransport inner;
  LockTrace trace;
  RecordingThreadLock lock(trace, 'S');
  LockedTransport transport(inner, lock);
  const std::uint64_t sizes[2] = {1u << 20u, 0u};
  WireWriter writer = transport.request_writer_with_room(sizes, 2u);
  CHECK(lock.held());
  CHECK(transport.invariants_hold());
  CHECK_EQ(inner.room_requests(), 1u);
  CHECK_EQ(inner.last_room_blobs(), 2u);
  CHECK_EQ(inner.last_room_bytes(), 1u << 20u);
  CHECK_EQ(lock.acquisitions(), 1u);
  CHECK(transport.send_async(command, writer) == TransportResult::ok);
  CHECK(!lock.held());
  CHECK(std::strcmp(trace.text(), "Ss") == 0); }

TEST(locked_transport, a_room_writer_given_up_lets_the_lock_go)
{ MockTransport inner;
  LockTrace trace;
  RecordingThreadLock lock(trace, 'S');
  LockedTransport transport(inner, lock);
  const std::uint64_t sizes[1] = {4096u};
  static_cast<void>(transport.request_writer_with_room(sizes, 1u));
  transport.end_call();
  CHECK(!lock.held());
  CHECK(transport.invariants_hold());
  CHECK(std::strcmp(trace.text(), "Ss") == 0); }

TEST(client_transport, a_transport_that_does_not_lock_asks_for_room_and_then_lends_the_writer)
{ MockTransport inner;
  const std::uint64_t sizes[1] = {4096u};
  WireWriter writer = inner.request_writer_with_room(sizes, 1u);
  CHECK_EQ(inner.room_requests(), 1u);
  CHECK_EQ(inner.last_room_bytes(), 4096u);
  CHECK(inner.send_async(command, writer) == TransportResult::ok); }
