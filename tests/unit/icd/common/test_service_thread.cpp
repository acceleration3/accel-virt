#include "harness/framework/test_registry.hpp"
#include "harness/icd/scripted_thread.hpp"
#include "icd/common/service_thread.hpp"

#include <cstdint>

using av::ScriptedThread;
using av::Status;

namespace {
class CountingLoop final : public av::icd::ServiceLoop {
public:
    explicit CountingLoop(std::uint32_t turns_before_done)
        : left_(turns_before_done), turned_(0u) {}

    bool turn() override
    {
        ++turned_;
        if (left_ == 0u) return false;
        --left_;
        return true;
    }

    std::uint32_t turned() const { return turned_; }

private:
    std::uint32_t left_;
    std::uint32_t turned_;
};

}

TEST(service_thread, a_thread_that_was_never_started_runs_nothing)
{ ScriptedThread thread;
  CHECK(!thread.running());
  CHECK(!thread.step());
  CHECK_EQ(thread.turns(), 0u);
  thread.stop();
  CHECK_EQ(thread.stops(), 0u); }

TEST(service_thread, a_started_loop_turns_once_per_step)
{ ScriptedThread thread;
  CountingLoop loop(8u);
  CHECK_STATUS(thread.start(loop), Status::ok);
  CHECK(thread.running());
  CHECK_EQ(loop.turned(), 0u);
  CHECK(thread.step());
  CHECK_EQ(loop.turned(), 1u);
  CHECK(thread.step());
  CHECK_EQ(loop.turned(), 2u);
  CHECK_EQ(thread.turns(), 2u); }

TEST(service_thread, a_loop_that_says_it_is_finished_is_not_turned_again)
{ ScriptedThread thread;
  CountingLoop loop(2u);
  CHECK_STATUS(thread.start(loop), Status::ok);
  CHECK(thread.step());
  CHECK(thread.step());
  CHECK(!thread.step());
  CHECK_EQ(loop.turned(), 3u);
  CHECK(!thread.running());
  CHECK(!thread.step());
  CHECK_EQ(loop.turned(), 3u); }

TEST(service_thread, stopping_ends_the_loop_and_nothing_turns_after_it)
{ ScriptedThread thread;
  CountingLoop loop(64u);
  CHECK_STATUS(thread.start(loop), Status::ok);
  CHECK(thread.step());
  thread.stop();
  CHECK(!thread.running());
  CHECK_EQ(thread.stops(), 1u);
  CHECK(!thread.step());
  CHECK_EQ(loop.turned(), 1u); }

TEST(service_thread, a_second_start_while_one_runs_is_refused)
{ ScriptedThread thread;
  CountingLoop first(4u);
  CountingLoop second(4u);
  CHECK_STATUS(thread.start(first), Status::ok);
  CHECK_STATUS(thread.start(second), Status::invalid_state);
  CHECK(thread.step());
  CHECK_EQ(first.turned(), 1u);
  CHECK_EQ(second.turned(), 0u);
  CHECK_EQ(thread.starts(), 1u); }

TEST(service_thread, a_thread_may_be_started_again_after_it_stops)
{ ScriptedThread thread;
  CountingLoop first(1u);
  CHECK_STATUS(thread.start(first), Status::ok);
  thread.stop();
  CountingLoop second(1u);
  CHECK_STATUS(thread.start(second), Status::ok);
  CHECK(thread.step());
  CHECK_EQ(second.turned(), 1u);
  CHECK_EQ(thread.starts(), 2u); }

TEST(service_thread, running_until_done_stops_at_its_limit_rather_than_for_ever)
{ ScriptedThread thread;
  CountingLoop loop(1000u);
  CHECK_STATUS(thread.start(loop), Status::ok);
  CHECK_EQ(thread.run_until_done(5u), 5u);
  CHECK(thread.running());
  CHECK_EQ(loop.turned(), 5u); }
