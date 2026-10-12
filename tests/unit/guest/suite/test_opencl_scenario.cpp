/* Vector add over MockKmt, the test playing the host through ScriptedHost:
 * every step passes when the host answers as an OpenCL GPU does; the output is
 * judged element by element; a failure at each step is reported with the rest
 * skipped; and every object made is released in reverse, whatever failed, with
 * nothing that was never made named to the host. */
#include "core/support/status.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/suite/guest_report.hpp"
#include "guest/suite/opencl_scenario.hpp"
#include "guest/suite/scenario_transport.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_host.hpp"
#include "proto/escape/av_context_record.hpp"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::AdapterScenario;
using av::AdapterSearch;
using av::AvgpuAdapters;
using av::CommandHeader;
using av::GuestReport;
using av::MockKmt;
using av::OpenClScenario;
using av::PciIdentity;
using av::ScriptedHost;
using av::Status;
using av::WireReader;
using namespace av::clproto;

namespace {

const PciIdentity av(AvgpuAdapters::virtio_vendor, AvgpuAdapters::virtio_gpu_device,
                      AvgpuAdapters::virtio_vendor, AvgpuAdapters::av_subsystem);
constexpr std::uint32_t device_removed = 0xC00E0102u;
constexpr std::uint32_t elements = OpenClScenario::elements;
/* The commands a whole run sends before its first release. */
constexpr std::uint32_t first_release = 17u;

bool has_line(const GuestReport &report, const char *line)
{
    return std::strstr(report.text(), line) != nullptr;
}

class Rig {
public:
    Rig() : search_(kmt_), host_(kmt_)
    {
        kmt_.add_adapter(av, AdapterScenario::wddm_3_2);
        CHECK_STATUS(search_.find_avgpu(0u), Status::ok);
    }

    MockKmt       &kmt() { return kmt_; }
    AdapterSearch &search() { return search_; }
    ScriptedHost  &host() { return host_; }

    /* A command's body, past its header. */
    WireReader body(std::uint32_t index) const
    {
        return WireReader(kmt_.command_bytes(index) + CommandHeader::encoded_size,
                          MockKmt::journal_bytes - CommandHeader::encoded_size);
    }

    std::uint32_t releases() const
    {
        std::uint32_t count = 0u;
        for (std::uint32_t index = 0u; index < kmt_.commands(); index++) {
            const std::uint32_t id = kmt_.command_id(index);
            count += id == kCmd_clReleaseKernel || id == kCmd_clReleaseProgram || id == kCmd_clReleaseMemObject ||
                             id == kCmd_clReleaseCommandQueue || id == kCmd_clReleaseContext
                         ? 1u
                         : 0u;
        }
        return count;
    }

    /* Every answer queued was awaited, and only the adapter is left open. */
    void check_nothing_left_but_the_adapter()
    {
        CHECK_EQ(kmt_.queued_answers(), 0u);
        CHECK_EQ(kmt_.open_objects(), 1u);
        CHECK_EQ(kmt_.misuses(), 0u);
        CHECK_STATUS(search_.close(), Status::ok);
        CHECK_EQ(kmt_.open_objects(), 0u);
    }

private:
    MockKmt       kmt_;
    AdapterSearch search_;
    ScriptedHost  host_;
};

void correct_sum(float *sum)
{
    for (std::uint32_t index = 0u; index < elements; index++) sum[index] = static_cast<float>(3u * index);
}

/* The first `answers` answers of a successful run, in the order it waits. */
void answer_until(ScriptedHost &host, std::uint32_t answers)
{
    const std::uint64_t platforms[] = {ScriptedHost::cl_cpu_platform, ScriptedHost::cl_gpu_platform};
    const std::uint64_t device = ScriptedHost::cl_device;
    for (std::uint32_t answer = 0u; answer < answers; answer++) {
        switch (answer) {
            case 0u: host.cl_ids(0, platforms, 2u, 2u); break;
            case 1u: host.cl_ids(-1, nullptr, 0u, 0u); break;
            case 2u: host.cl_ids(0, &device, 1u, 1u); break;
            case 3u: host.cl_handle(ScriptedHost::cl_context, 0); break;
            case 4u: host.cl_handle(ScriptedHost::cl_queue, 0); break;
            case 5u: case 6u: case 7u: host.cl_handle(ScriptedHost::cl_buffer + (answer - 5u), 0); break;
            case 8u: host.cl_handle(ScriptedHost::cl_program, 0); break;
            case 10u: host.cl_handle(ScriptedHost::cl_kernel, 0); break;
            default: host.cl_result(0); break;
        }
    }
}

}  // namespace

TEST(opencl_scenario, vector_add_runs_on_the_host_and_everything_is_released_in_reverse)
{ Rig rig;
  rig.host().opencl_run();
  GuestReport report;
  OpenClScenario scenario(rig.kmt(), report, rig.search());
  scenario.run();
  CHECK(std::strcmp(report.text(),
                    "ok opencl.adapter\n"
                    "ok opencl.connection\n"
                    "ok opencl.initialize\n"
                    "ok opencl.platform 2\n"
                    "ok opencl.device\n"
                    "ok opencl.context\n"
                    "ok opencl.queue\n"
                    "ok opencl.buffers\n"
                    "ok opencl.program\n"
                    "ok opencl.kernel\n"
                    "ok opencl.run\n"
                    "ok opencl.read\n"
                    "ok opencl.sum 64\n"
                    "ok opencl.release\n") == 0);
  const std::uint32_t expected[] = {
      kCmd_clGetPlatformIDs, kCmd_clGetDeviceIDs, kCmd_clGetDeviceIDs, kCmd_clCreateContext,
      kCmd_clCreateCommandQueueWithProperties, kCmd_clCreateBuffer, kCmd_clCreateBuffer, kCmd_clCreateBuffer,
      kCmd_clCreateProgramWithSource, kCmd_clBuildProgram, kCmd_clCreateKernel, kCmd_clSetKernelArg,
      kCmd_clSetKernelArg, kCmd_clSetKernelArg, kCmd_clEnqueueNDRangeKernel, kCmd_clFinish, kCmd_clEnqueueReadBuffer,
      kCmd_clReleaseKernel, kCmd_clReleaseProgram, kCmd_clReleaseMemObject, kCmd_clReleaseMemObject,
      kCmd_clReleaseMemObject, kCmd_clReleaseCommandQueue, kCmd_clReleaseContext};
  CHECK_EQ(rig.kmt().commands(), sizeof expected / sizeof expected[0]);
  for (std::uint32_t index = 0u; index < sizeof expected / sizeof expected[0]; index++)
      CHECK_EQ(rig.kmt().command_id(index), expected[index]);
  /* Released newest first, each by the handle the host made. */
  const std::uint64_t released[] = {ScriptedHost::cl_kernel,       ScriptedHost::cl_program,
                                    ScriptedHost::cl_buffer + 2u,  ScriptedHost::cl_buffer + 1u,
                                    ScriptedHost::cl_buffer,       ScriptedHost::cl_queue,
                                    ScriptedHost::cl_context};
  for (std::uint32_t index = 0u; index < 7u; index++) {
      WireReader body = rig.body(first_release + index);
      CHECK_EQ(body.read_u64(), released[index]);
  }
  av::AvContextRecord record;
  CHECK_STATUS(record.decode(rig.kmt().last_private(), rig.kmt().last_private_size()), Status::ok);
  CHECK(record.sub_api() == av::SubApi::opencl);
  CHECK(report.invariants_hold());
  rig.check_nothing_left_but_the_adapter(); }

/* The device searched is the GPU platform's, not the CPU runtime listed first;
 * the kernel's arguments are the buffers in order, as cl_mem handles the host
 * made; the inputs cross copied from guest memory; and the read asks, blocking,
 * for exactly the output's bytes. The program command fits the transport. */
TEST(opencl_scenario, sends_the_handles_and_inputs_the_host_needs)
{ Rig rig;
  rig.host().opencl_run();
  GuestReport report;
  OpenClScenario scenario(rig.kmt(), report, rig.search());
  scenario.run();
  { WireReader body = rig.body(1u);
    CHECK_EQ(body.read_u64(), ScriptedHost::cl_cpu_platform);
    WireReader gpu = rig.body(2u);
    CHECK_EQ(gpu.read_u64(), ScriptedHost::cl_gpu_platform);
    std::uint64_t type = 0u;
    CHECK(gpu.read_varint(type));
    CHECK_EQ(type, static_cast<std::uint64_t>(CL_DEVICE_TYPE_GPU)); }
  for (std::uint32_t argument = 0u; argument < 3u; argument++) {
      WireReader body = rig.body(11u + argument);
      CHECK_EQ(body.read_u64(), ScriptedHost::cl_kernel);
      std::uint64_t index = 0u;
      std::uint64_t size = 0u;
      CHECK(body.read_varint(index));
      CHECK(body.read_varint(size));
      CHECK_EQ(index, argument);
      CHECK_EQ(size, sizeof(std::uint64_t));
      CHECK_EQ(body.read_u8(), 1u);
      CHECK(body.read_varint(size));
      CHECK_EQ(body.read_u64(), ScriptedHost::cl_buffer + argument);
  }
  { WireReader body = rig.body(6u);   /* the second input: 2i */
    CHECK_EQ(body.read_u64(), ScriptedHost::cl_context);
    std::uint64_t flags = 0u;
    std::uint64_t size = 0u;
    CHECK(body.read_varint(flags));
    CHECK_EQ(flags, static_cast<std::uint64_t>(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR));
    CHECK(body.read_varint(size));
    CHECK_EQ(size, elements * sizeof(float));
    CHECK_EQ(body.read_u8(), 1u);
    CHECK(body.read_varint(size));
    float values[4] = {};
    CHECK(body.read(values, sizeof values));
    CHECK(values[3] == 6.0f); }
  { WireReader body = rig.body(7u);   /* the output: written, never copied */
    CHECK_EQ(body.read_u64(), ScriptedHost::cl_context);
    std::uint64_t flags = 0u;
    std::uint64_t size = 0u;
    CHECK(body.read_varint(flags));
    CHECK_EQ(flags, static_cast<std::uint64_t>(CL_MEM_WRITE_ONLY));
    CHECK(body.read_varint(size));
    CHECK_EQ(body.read_u8(), 0u); }
  { WireReader body = rig.body(16u);
    CHECK_EQ(body.read_u64(), ScriptedHost::cl_queue);
    CHECK_EQ(body.read_u64(), ScriptedHost::cl_buffer + 2u);
    std::uint64_t value = 0u;
    CHECK(body.read_varint(value));
    CHECK_EQ(value, static_cast<std::uint64_t>(CL_TRUE));
    CHECK(body.read_varint(value));
    CHECK_EQ(value, 0u);
    CHECK(body.read_varint(value));
    CHECK_EQ(value, elements * sizeof(float)); }
  CHECK(rig.kmt().command_size(8u) <= av::ScenarioTransport::command_bytes);
  rig.check_nothing_left_but_the_adapter(); }

/* Each element is judged: three wrong answers leave 61 matched, which the step
 * reports, and everything made is still released. */
TEST(opencl_scenario, a_wrong_sum_fails_with_how_many_elements_matched)
{ Rig rig;
  float sum[ScriptedHost::cl_elements] = {};
  correct_sum(sum);
  sum[0] = 1.0f;
  sum[31] += 0.5f;
  sum[63] = 0.0f;
  rig.host().opencl_run_to_read(sum);
  rig.host().opencl_releases();
  GuestReport report;
  OpenClScenario scenario(rig.kmt(), report, rig.search());
  scenario.run();
  CHECK(has_line(report, "ok opencl.read\n"));
  CHECK(has_line(report, "FAIL opencl.sum status=4 61\n"));
  CHECK(!has_line(report, "opencl.release"));
  CHECK_EQ(report.failed(), 1u);
  CHECK_EQ(rig.releases(), 7u);
  rig.check_nothing_left_but_the_adapter(); }

/* A read that answers fewer bytes than the output leaves the rest unmatched,
 * even on a scenario whose previous run read every element right. */
TEST(opencl_scenario, a_short_read_matches_only_what_came_back)
{ Rig rig;
  float sum[ScriptedHost::cl_elements] = {};
  correct_sum(sum);
  rig.host().opencl_run();
  answer_until(rig.host(), 16u);
  rig.host().cl_read(0, sum, 10u);
  rig.host().opencl_releases();
  GuestReport report;
  OpenClScenario scenario(rig.kmt(), report, rig.search());
  scenario.run();
  scenario.run();
  CHECK(has_line(report, "ok opencl.sum 64\n"));
  CHECK(has_line(report, "FAIL opencl.sum status=4 10\n"));
  CHECK_EQ(report.failed(), 1u);
  rig.check_nothing_left_but_the_adapter(); }

/* Each step's failure is the last step reported, with the OpenCL error on the
 * line, and exactly what was made before it is released. */
TEST(opencl_scenario, a_failure_at_each_step_releases_only_what_was_made)
{ struct Case {
      std::uint32_t answers;
      std::uint32_t failing;   /* 0 platform, 1 no platform, 2 no GPU, 3 created, 4 result, 5 read */
      std::int32_t  error;
      const char   *line;
      const char   *skipped;
      std::uint32_t releases;
  };
  const Case cases[] = {
      {0u, 0u, -1001, "FAIL opencl.platform status=7 4294966295\n", "opencl.device", 0u},
      {0u, 1u, 0, "FAIL opencl.platform status=3 0\n", "opencl.device", 0u},
      {1u, 2u, -1, "FAIL opencl.device status=7 4294967295\n", "opencl.context", 0u},
      {3u, 3u, -6, "FAIL opencl.context status=7 4294967290\n", "opencl.queue", 0u},
      {4u, 3u, -34, "FAIL opencl.queue status=7 4294967262\n", "opencl.buffers", 1u},
      {5u, 3u, -61, "FAIL opencl.buffers status=7 4294967235\n", "opencl.program", 2u},
      {7u, 3u, -61, "FAIL opencl.buffers status=7 4294967235\n", "opencl.program", 4u},
      {8u, 3u, -6, "FAIL opencl.program status=7 4294967290\n", "opencl.kernel", 5u},
      {9u, 4u, -11, "FAIL opencl.program status=7 4294967285\n", "opencl.kernel", 6u},
      {10u, 3u, -46, "FAIL opencl.kernel status=7 4294967250\n", "opencl.run", 6u},
      {12u, 4u, -50, "FAIL opencl.run status=7 4294967246\n", "opencl.read", 7u},
      {14u, 4u, -54, "FAIL opencl.run status=7 4294967242\n", "opencl.read", 7u},
      {15u, 4u, -36, "FAIL opencl.run status=7 4294967260\n", "opencl.read", 7u},
      {16u, 5u, -30, "FAIL opencl.read status=7 4294967266\n", "opencl.sum", 7u}};
  for (const Case &failure : cases) {
      Rig rig;
      answer_until(rig.host(), failure.answers);
      switch (failure.failing) {
          case 0u: case 1u: rig.host().cl_ids(failure.error, nullptr, 0u, 0u); break;
          case 2u:   /* neither the CPU runtime nor the second platform has a GPU */
              rig.host().cl_ids(failure.error, nullptr, 0u, 0u);
              rig.host().cl_ids(failure.error, nullptr, 0u, 0u);
              break;
          case 3u: rig.host().cl_handle(0u, failure.error); break;
          case 4u: rig.host().cl_result(failure.error); break;
          default: rig.host().cl_read(failure.error, nullptr, 0u); break;
      }
      for (std::uint32_t release = 0u; release < failure.releases; release++) rig.host().cl_result(0);
      GuestReport report;
      OpenClScenario scenario(rig.kmt(), report, rig.search());
      scenario.run();
      CHECK(has_line(report, failure.line));
      CHECK(!has_line(report, failure.skipped));
      CHECK(!has_line(report, "opencl.release"));
      CHECK_EQ(report.failed(), 1u);
      CHECK_EQ(rig.releases(), failure.releases);
      rig.check_nothing_left_but_the_adapter();
  } }

/* Every release is sent whatever the host answers, and the step reports the
 * first refusal. */
TEST(opencl_scenario, a_refused_release_fails_release_with_the_first_error)
{ Rig rig;
  float sum[ScriptedHost::cl_elements] = {};
  correct_sum(sum);
  rig.host().opencl_run_to_read(sum);
  rig.host().cl_result(0);
  rig.host().cl_result(-44);   /* the program */
  for (std::uint32_t release = 0u; release < 4u; release++) rig.host().cl_result(0);
  rig.host().cl_result(-34);   /* the context */
  GuestReport report;
  OpenClScenario scenario(rig.kmt(), report, rig.search());
  scenario.run();
  CHECK(has_line(report, "ok opencl.sum 64\n"));
  CHECK(has_line(report, "FAIL opencl.release status=7 4294967252\n"));
  CHECK_EQ(rig.releases(), 7u);
  rig.check_nothing_left_but_the_adapter(); }

/* More platforms than the room: the count is the host's, the search covers the
 * ids that fit, and the GPU on the first of them is the one used. */
TEST(opencl_scenario, more_platforms_than_the_room_reports_the_host_s_count)
{ Rig rig;
  std::uint64_t platforms[16] = {};
  for (std::uint32_t index = 0u; index < 16u; index++) platforms[index] = ScriptedHost::cl_gpu_platform + index;
  const std::uint64_t device = ScriptedHost::cl_device;
  rig.host().cl_ids(0, platforms, 16u, 20u);
  rig.host().cl_ids(0, &device, 1u, 1u);
  float sum[ScriptedHost::cl_elements] = {};
  correct_sum(sum);
  for (std::uint32_t answer = 3u; answer < 16u; answer++) {
      switch (answer) {
          case 3u: rig.host().cl_handle(ScriptedHost::cl_context, 0); break;
          case 4u: rig.host().cl_handle(ScriptedHost::cl_queue, 0); break;
          case 5u: case 6u: case 7u: rig.host().cl_handle(ScriptedHost::cl_buffer + (answer - 5u), 0); break;
          case 8u: rig.host().cl_handle(ScriptedHost::cl_program, 0); break;
          case 10u: rig.host().cl_handle(ScriptedHost::cl_kernel, 0); break;
          default: rig.host().cl_result(0); break;
      }
  }
  rig.host().cl_read(0, sum, elements);
  rig.host().opencl_releases();
  GuestReport report;
  OpenClScenario scenario(rig.kmt(), report, rig.search());
  scenario.run();
  CHECK(has_line(report, "ok opencl.platform 20\n"));
  CHECK(has_line(report, "ok opencl.release\n"));
  CHECK_EQ(report.failed(), 0u);
  rig.check_nothing_left_but_the_adapter(); }

/* A context lost mid-run: the error output keeps the value it had before the
 * call, which is a failure, and the lost context sends no releases. */
TEST(opencl_scenario, a_lost_context_fails_the_step_it_was_lost_at)
{ Rig rig;
  answer_until(rig.host(), 3u);
  rig.kmt().fail_at(MockKmt::Call::wait_for_context, 4u, device_removed);
  GuestReport report;
  OpenClScenario scenario(rig.kmt(), report, rig.search());
  scenario.run();
  CHECK(has_line(report, "FAIL opencl.context status=7 4294967291\n"));
  CHECK_EQ(report.failed(), 1u);
  CHECK_EQ(rig.releases(), 0u);
  rig.check_nothing_left_but_the_adapter(); }

TEST(opencl_scenario, without_the_adapter_it_asks_the_driver_for_nothing)
{ MockKmt kmt;
  AdapterSearch search(kmt);
  GuestReport report;
  OpenClScenario scenario(kmt, report, search);
  scenario.run();
  CHECK(std::strcmp(report.text(), "FAIL opencl.adapter status=3 code=0x00000000\n") == 0);
  CHECK_EQ(kmt.calls(MockKmt::Call::create_device), 0u);
  CHECK_EQ(kmt.open_objects(), 0u); }
