#include "core/trace/trace_host.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/trace/mock_trace_host.hpp"

#include <cstdint>

#include <pthread.h>

using av::MockTraceHost;
using av::TraceHost;

namespace {

void count_call(void *context) { ++*static_cast<int *>(context); }

}

TEST(mock_trace_host, answers_through_trace_host_what_the_test_set)
{ MockTraceHost mock;
  TraceHost &host = mock;
  mock.run_as(0x55u, 7u);
  mock.set_clock(123u);
  mock.set_level(2u);
  CHECK_EQ(host.thread_key(), 0x55u);
  CHECK_EQ(host.thread_number(), 7u);
  CHECK_EQ(host.clock(), 123u);
  CHECK_EQ(host.level(), 2u); }

TEST(mock_trace_host, swaps_only_an_expected_word_and_fails_the_scripted_one)
{ MockTraceHost mock;
  TraceHost &host = mock;
  std::uint64_t word = 5u;
  CHECK(!host.exchange(word, 4u, 9u));
  CHECK_EQ(word, 5u);
  CHECK(host.exchange(word, 5u, 9u));
  CHECK_EQ(word, 9u);
  mock.fail_exchange_at(2u);
  CHECK(host.exchange(word, 9u, 10u));
  CHECK(!host.exchange(word, 10u, 11u));
  CHECK_EQ(word, 10u);
  CHECK(host.exchange(word, 10u, 11u));
  mock.fail_every_exchange(true);
  CHECK(!host.exchange(word, 11u, 12u));
  mock.fail_every_exchange(false);
  CHECK(host.exchange(word, 11u, 12u));
  CHECK_EQ(mock.exchanges(), 7u);
  mock.fail_exchange_at(1u, 2u);
  CHECK(!host.exchange(word, 12u, 13u));
  CHECK(!host.exchange(word, 12u, 13u));
  CHECK(host.exchange(word, 12u, 13u)); }

TEST(mock_trace_host, runs_the_level_hook_once)
{ MockTraceHost mock;
  int calls = 0;
  mock.on_next_level(&count_call, &calls);
  static_cast<void>(mock.level());
  static_cast<void>(mock.level());
  CHECK_EQ(calls, 1); }

namespace {

void *key_on_this_thread(void *host)
{
    return reinterpret_cast<void *>(static_cast<std::uintptr_t>(static_cast<MockTraceHost *>(host)->thread_key()));
}

}

TEST(mock_trace_host, another_thread_is_given_no_key)
{ MockTraceHost mock;
  mock.run_as(0x77u, 7u);
  pthread_t thread;
  CHECK_EQ(pthread_create(&thread, nullptr, &key_on_this_thread, &mock), 0);
  void *key = reinterpret_cast<void *>(1);
  CHECK_EQ(pthread_join(thread, &key), 0);
  CHECK(key == nullptr);
  CHECK_EQ(mock.thread_key(), 0x77u); }
