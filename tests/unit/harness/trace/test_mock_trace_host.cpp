#include "core/trace/trace_host.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/trace/mock_trace_host.hpp"

#include <cstdint>

using av::MockTraceHost;
using av::TraceHost;

namespace {

void count_call(void *context) { ++*static_cast<int *>(context); }

}  // namespace

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
  CHECK_EQ(mock.exchanges(), 7u); }

TEST(mock_trace_host, runs_the_level_hook_once)
{ MockTraceHost mock;
  int calls = 0;
  mock.on_next_level(&count_call, &calls);
  static_cast<void>(mock.level());
  static_cast<void>(mock.level());
  CHECK_EQ(calls, 1); }
