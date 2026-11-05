#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/transport/mock_transport.hpp"
#include "harness/transport/roomed_transport.hpp"
#include "icd/common/locked_transport.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/generated/vulkan/vk_handle_translation.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>
#include <ctime>

#include <pthread.h>

namespace av::icd {

struct LockedTransportProbe {
    static bool &holds(LockedTransport &transport) { return transport.holds_; }
};

}

using av::ClientTransport;
using av::LockTrace;
using av::MockTransport;
using av::RecordingThreadLock;
using av::TransportResult;
using av::WireReader;
using av::WireWriter;
using av::icd::LockedTransport;
using av::icd::LockedTransportProbe;

namespace {

constexpr std::uint32_t command = 0x1234u;

}

TEST(locked_transport, holds_the_lock_from_the_writer_until_an_async_send_returns)
{ MockTransport inner;
  LockTrace trace;
  RecordingThreadLock lock(trace, 'S');
  LockedTransport transport(inner, lock);
  WireWriter writer = transport.request_writer();
  CHECK(lock.held());
  writer.write_pod<std::uint32_t>(0xABCDu);
  CHECK(transport.send_async(command, writer) == TransportResult::ok);
  CHECK(!lock.held());
  CHECK_EQ(inner.sends(), 1u);
  CHECK_EQ(inner.last_command(), command);
  CHECK_EQ(inner.last_length(), 4u);
  CHECK(std::strcmp(trace.text(), "Ss") == 0); }

TEST(locked_transport, holds_the_lock_while_a_sync_reply_is_read_until_the_call_ends)
{ MockTransport inner;
  const std::uint8_t answer[] = {1u, 2u, 3u};
  inner.set_reply(answer, sizeof answer);
  LockTrace trace;
  RecordingThreadLock lock(trace, 'S');
  LockedTransport transport(inner, lock);
  WireWriter writer = transport.request_writer();
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(command, writer, reply) == TransportResult::ok);
  CHECK(lock.held());
  CHECK_EQ(reply.remaining(), 3u);
  CHECK_EQ(reply.read_u8(), 1u);
  CHECK(transport.invariants_hold());
  transport.end_call();
  CHECK(!lock.held());
  CHECK_EQ(inner.end_calls(), 1u);
  CHECK(transport.invariants_hold());
  CHECK_EQ(lock.acquisitions(), 1u);
  CHECK(std::strcmp(trace.text(), "Ss") == 0); }

TEST(locked_transport, a_failed_send_still_lets_the_lock_go_and_says_it_failed)
{ MockTransport inner;
  LockTrace trace;
  RecordingThreadLock lock(trace, 'S');
  LockedTransport transport(inner, lock);
  inner.fail_next();
  WireWriter writer = transport.request_writer();
  CHECK(transport.send_async(command, writer) == TransportResult::failed);
  CHECK(!lock.held());
  inner.fail_next();
  writer = transport.request_writer();
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(command, writer, reply) == TransportResult::failed);
  CHECK(lock.held());
  transport.end_call();
  CHECK(!lock.held());
  CHECK_EQ(inner.sends(), 0u);
  CHECK(std::strcmp(trace.text(), "SsSs") == 0); }

TEST(locked_transport, an_end_with_no_sync_call_open_releases_nothing)
{ MockTransport inner;
  LockTrace trace;
  RecordingThreadLock lock(trace, 'S');
  LockedTransport transport(inner, lock);
  transport.end_call();
  WireWriter writer = transport.request_writer();
  CHECK(transport.send_async(command, writer) == TransportResult::ok);
  transport.end_call();
  writer = transport.request_writer();
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(command, writer, reply) == TransportResult::ok);
  transport.end_call();
  transport.end_call();
  CHECK(!lock.held());
  CHECK(!lock.claim_violation());
  CHECK_EQ(inner.end_calls(), 4u);
  CHECK(std::strcmp(trace.text(), "SsSs") == 0);
  CHECK(transport.invariants_hold()); }

TEST(locked_transport, an_end_while_writing_lets_the_writer_go)
{ MockTransport inner;
  LockTrace trace;
  RecordingThreadLock lock(trace, 'S');
  LockedTransport transport(inner, lock);
  WireWriter writer = transport.request_writer();
  CHECK(lock.held());
  CHECK(transport.invariants_hold());
  transport.end_call();
  CHECK(!lock.held());
  CHECK(transport.invariants_hold());
  CHECK_EQ(inner.sends(), 0u);
  CHECK_EQ(inner.end_calls(), 1u);
  writer = transport.request_writer();
  CHECK(transport.send_async(command, writer) == TransportResult::ok);
  CHECK(!lock.held());
  CHECK(std::strcmp(trace.text(), "SsSs") == 0); }

TEST(locked_transport, invariants_refuse_a_held_lock_with_no_call_or_a_call_without_it)
{ MockTransport inner;
  LockTrace trace;
  RecordingThreadLock lock(trace, 'S');
  LockedTransport transport(inner, lock);
  CHECK(transport.invariants_hold());
  LockedTransportProbe::holds(transport) = true;
  CHECK(!transport.invariants_hold());
  LockedTransportProbe::holds(transport) = false;
  WireWriter writer = transport.request_writer();
  CHECK(transport.invariants_hold());
  LockedTransportProbe::holds(transport) = false;
  CHECK(!transport.invariants_hold());
  LockedTransportProbe::holds(transport) = true;
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(command, writer, reply) == TransportResult::ok);
  CHECK(transport.invariants_hold());
  LockedTransportProbe::holds(transport) = false;
  CHECK(!transport.invariants_hold());
  LockedTransportProbe::holds(transport) = true;
  transport.end_call();
  CHECK(transport.invariants_hold()); }

namespace {
class MutexLock final : public av::ThreadLock {
public:
    MutexLock() { pthread_mutex_init(&mutex_, nullptr); }
    ~MutexLock() { pthread_mutex_destroy(&mutex_); }
    void acquire() override { pthread_mutex_lock(&mutex_); }
    bool try_acquire() override { return pthread_mutex_trylock(&mutex_) == 0; }
    void release() override { pthread_mutex_unlock(&mutex_); }

private:
    pthread_mutex_t mutex_;
};

class SharedSlotTransport final : public av::RoomedTransport {
public:
    SharedSlotTransport() : av::RoomedTransport(64u, 64u)
    { pthread_mutex_init(&mutex_, nullptr);
      pthread_cond_init(&changed_, nullptr); }
    ~SharedSlotTransport()
    { pthread_cond_destroy(&changed_);
      pthread_mutex_destroy(&mutex_); }

    WireWriter request_writer() override { return WireWriter(request_, sizeof request_); }
    TransportResult send_async(std::uint32_t, WireWriter &) override { return TransportResult::failed; }
    TransportResult send_sync(std::uint32_t command_id, WireWriter &, WireReader &reply) override
    { pthread_mutex_lock(&mutex_);
      std::memcpy(slot_, &command_id, sizeof command_id);
      record(static_cast<char>('0' + command_id));
      pthread_cond_broadcast(&changed_);
      pthread_mutex_unlock(&mutex_);
      reply = WireReader(slot_, sizeof slot_);
      return TransportResult::ok; }
    void end_call() override
    { pthread_mutex_lock(&mutex_);
      record('e');
      pthread_mutex_unlock(&mutex_); }
    av::WireArena   &decode_arena() override { return arena_; }
    av::ClientBlobArena &blobs() override { return blobs_; }

    bool wait_for(char event, long milliseconds)
    { timespec deadline = {};
      clock_gettime(CLOCK_REALTIME, &deadline);
      deadline.tv_nsec += milliseconds * 1000000L;
      deadline.tv_sec += deadline.tv_nsec / 1000000000L;
      deadline.tv_nsec %= 1000000000L;
      pthread_mutex_lock(&mutex_);
      while (std::strchr(log_, event) == nullptr) {
          if (pthread_cond_timedwait(&changed_, &mutex_, &deadline) != 0) break;
      }
      const bool seen = std::strchr(log_, event) != nullptr;
      pthread_mutex_unlock(&mutex_);
      return seen; }
    const char *log() const { return log_; }

private:
    void record(char event)
    { if (length_ + 1u < sizeof log_) log_[length_++] = event; }

    pthread_mutex_t     mutex_;
    pthread_cond_t      changed_;
    std::uint8_t        request_[64] = {};
    std::uint8_t        slot_[4] = {};
    char                log_[16] = {};
    std::uint32_t       length_ = 0u;
    av::WireArena       arena_{nullptr, 0u};
    av::ClientBlobArena blobs_;
};

struct SecondCall {
    ClientTransport *transport;
    std::uint32_t    answer;
};

void *second_call(void *argument)
{
    SecondCall &call = *static_cast<SecondCall *>(argument);
    WireWriter writer = call.transport->request_writer();
    WireReader reply(nullptr, 0u);
    if (call.transport->send_sync(2u, writer, reply) == TransportResult::ok) call.answer = reply.read_u32();
    call.transport->end_call();
    return nullptr;
}

}

TEST(locked_transport, a_second_thread_cannot_replace_a_reply_before_the_first_call_ends)
{ SharedSlotTransport inner;
  MutexLock lock;
  LockedTransport transport(inner, lock);
  WireWriter writer = transport.request_writer();
  WireReader reply(nullptr, 0u);
  CHECK(transport.send_sync(1u, writer, reply) == TransportResult::ok);
  SecondCall call{&transport, 0u};
  pthread_t thread;
  CHECK_EQ(pthread_create(&thread, nullptr, second_call, &call), 0);
  CHECK(!inner.wait_for('2', 200L));
  CHECK_EQ(reply.read_u32(), 1u);
  transport.end_call();
  CHECK_EQ(pthread_join(thread, nullptr), 0);
  CHECK_EQ(call.answer, 2u);
  CHECK(std::strcmp(inner.log(), "1e2e") == 0);
  CHECK(transport.invariants_hold()); }

TEST(locked_transport, a_generated_command_takes_the_lock_once)
{ MockTransport inner;
  std::uint8_t answer[12] = {};
  constexpr std::uint64_t host_instance = 0x1100u;
  std::memcpy(answer + 4, &host_instance, sizeof host_instance);
  inner.set_reply(answer, sizeof answer);
  LockTrace trace;
  RecordingThreadLock lock(trace, 'S');
  LockedTransport transport(inner, lock);
  ClientTransport &seam = transport;
  av::vkproto::HandleIdentity identity;
  VkInstanceCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  std::uint64_t instance = 0u;
  const VkResult result = av::vkproto::av_vkCreateInstance(seam, identity, &create, nullptr, &instance);
  CHECK_EQ(result, VK_SUCCESS);
  CHECK_EQ(instance, host_instance);
  CHECK_EQ(inner.last_command(), static_cast<std::uint32_t>(av::vkproto::kCmd_vkCreateInstance));
  CHECK(std::strcmp(trace.text(), "Ss") == 0); }

TEST(locked_transport, lends_the_inner_transports_arena_and_blobs)
{ MockTransport inner;
  LockTrace trace;
  RecordingThreadLock lock(trace, 'S');
  LockedTransport transport(inner, lock);
  CHECK(&transport.decode_arena() == &inner.decode_arena());
  CHECK(&transport.blobs() == &inner.blobs());
  CHECK(!lock.held()); }

TEST(locked_transport, makes_blob_room_under_the_lock_and_lends_the_inner_room_and_counters)
{ MockTransport inner;
  LockTrace trace;
  RecordingThreadLock lock(trace, 'S');
  LockedTransport transport(inner, lock);
  CHECK(&transport.room() == &inner.room());
  CHECK(!lock.held());
  const std::uint64_t sizes[] = {1u << 30u};
  CHECK(!transport.make_room_for_blobs(sizes, 1u));
  CHECK(std::strcmp(trace.text(), "Ss") == 0);
  transport.note(av::ChannelCounter::split_pieces, 3u);
  CHECK_EQ(inner.noted(av::ChannelCounter::split_pieces), 3u);
  CHECK(!lock.held());
  CHECK(transport.invariants_hold()); }
