#include "core/log/driver_log.hpp"
#include "executor/executor_log.hpp"
#include "executor/opencl/opencl_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"

#include <CL/cl.h>
#include <cstdint>

namespace {

constexpr std::uint32_t most_finished = 16u;

cl_command_queue finished[most_finished];
std::uint32_t    finished_count = 0u;
cl_int           finish_answer = CL_SUCCESS;

void forget_finished()
{
    finished_count = 0u;
    finish_answer = CL_SUCCESS;
}

std::uint32_t times_finished(cl_command_queue queue)
{
    std::uint32_t times = 0u;
    for (std::uint32_t at = 0u; at < finished_count; at++)
        if (finished[at] == queue) times++;
    return times;
}

cl_command_queue fake_queue(std::uintptr_t number) { return reinterpret_cast<cl_command_queue>(number); }

av::RecordingLog sink;
av::ExecutorLog  executor_log(sink);

}

extern "C" cl_int clFinish(cl_command_queue queue)
{
    if (finished_count < most_finished) finished[finished_count++] = queue;
    return finish_answer;
}

namespace av {
struct OpenClBackendProbe {
    static void borrow_queue(OpenClBackend &backend, void *queue) { backend.queue_ = queue; }
    static void keep(OpenClBackend &backend, clproto::HandleKind kind, void *object)
    {
        CHECK_STATUS(backend.hold(kind, object), Status::ok);
    }
};
}

using av::OpenClBackend;
using av::OpenClBackendProbe;
using av::clproto::HandleKind;

TEST(opencl_backend_drain, finishes_nothing_for_a_backend_that_holds_no_queue)
{ forget_finished();
  OpenClBackend backend(executor_log.opencl());
  backend.drain();
  CHECK_EQ(finished_count, 0u); }

TEST(opencl_backend_drain, finishes_the_queue_the_backend_made_and_every_queue_the_guest_holds_once_each)
{ forget_finished();
  OpenClBackend backend(executor_log.opencl());
  OpenClBackendProbe::borrow_queue(backend, fake_queue(0x1000u));
  OpenClBackendProbe::keep(backend, HandleKind::command_queue, fake_queue(0x2000u));
  OpenClBackendProbe::keep(backend, HandleKind::command_queue, fake_queue(0x3000u));
  backend.drain();
  CHECK_EQ(finished_count, 3u);
  CHECK_EQ(times_finished(fake_queue(0x1000u)), 1u);
  CHECK_EQ(times_finished(fake_queue(0x2000u)), 1u);
  CHECK_EQ(times_finished(fake_queue(0x3000u)), 1u);
  OpenClBackendProbe::borrow_queue(backend, nullptr); }

TEST(opencl_backend_drain, leaves_alone_every_held_object_that_is_not_a_command_queue)
{ forget_finished();
  OpenClBackend backend(executor_log.opencl());
  OpenClBackendProbe::keep(backend, HandleKind::mem, fake_queue(0x4000u));
  OpenClBackendProbe::keep(backend, HandleKind::context, fake_queue(0x5000u));
  OpenClBackendProbe::keep(backend, HandleKind::command_queue, fake_queue(0x6000u));
  backend.drain();
  CHECK_EQ(finished_count, 1u);
  CHECK_EQ(times_finished(fake_queue(0x6000u)), 1u);
  CHECK_EQ(times_finished(fake_queue(0x4000u)), 0u);
  CHECK_EQ(times_finished(fake_queue(0x5000u)), 0u); }

TEST(opencl_backend_drain, goes_on_to_the_next_queue_when_one_fails_to_finish)
{ forget_finished();
  finish_answer = CL_OUT_OF_RESOURCES;
  OpenClBackend backend(executor_log.opencl());
  OpenClBackendProbe::borrow_queue(backend, fake_queue(0x1000u));
  OpenClBackendProbe::keep(backend, HandleKind::command_queue, fake_queue(0x2000u));
  backend.drain();
  CHECK_EQ(finished_count, 2u);
  OpenClBackendProbe::borrow_queue(backend, nullptr); }
