#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/transport/mock_transport.hpp"
#include "icd/common/opened_objects.hpp"
#include "icd/common/shared_handles.hpp"
#include "icd/opencl/cl_external_memory.hpp"
#include "icd/opencl/cl_property_list.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/shared_object_answer.hpp"

#include <CL/cl_ext.h>

#include <cstdint>
#include <cstring>

namespace av::icd {

struct OpenedObjectsProbe {
    static void add(OpenedObjects &opened, const OpenedObjectRow &row)
    {
        OpenedObjectRow *made = nullptr;
        CHECK_STATUS(opened.rows_.create(made, row), Status::ok);
    }
};

}

namespace av::clicd {

struct ClExternalMemoryProbe {
    static icd::OpenedObjects &opened(ClExternalMemory &memory) { return memory.opened_; }
};

}

namespace {

using av::MockKmt;
using av::MockTransport;
using av::SharedObjectAnswer;
using av::SharedObjectKind;
using av::Status;
using av::clicd::ClExternalMemory;
using av::clicd::ClExternalMemoryProbe;
using av::clicd::ClPropertyList;

constexpr std::uint64_t object_bytes = 4u * 4096u;
constexpr std::uint64_t host_buffer = 0x5150u;

class Answer {
public:
    Answer(std::uint32_t status, std::uint64_t made) : bytes_{}
    {
        std::memcpy(bytes_, &status, sizeof status);
        std::memcpy(bytes_ + 8u, &made, sizeof made);
    }
    const std::uint8_t *bytes() const { return bytes_; }

private:
    std::uint8_t bytes_[SharedObjectAnswer::encoded_size];
};

class Process {
public:
    Process() : blocks_(), trace_(), lock_(trace_, 'S'), kmt_(), log_(), wire_(), device_(0u),
                handles_(kmt_, lock_, blocks_), memory_(handles_, blocks_, log_)
    {
        kmt_.add_adapter(av::PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u), 3200u);
        std::uint32_t adapter = 0u;
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter, 1u, count), Status::ok);
        CHECK_STATUS(kmt_.create_device(adapter, device_), Status::ok);
    }

    ClExternalMemory &memory() { return memory_; }
    av::MockPlatform &blocks() { return blocks_; }
    MockTransport    &wire() { return wire_; }
    av::RecordingLog &log() { return log_; }
    std::uint32_t     device() const { return device_; }

    std::uint64_t handle()
    {
        const av::AllocationDescriptor descriptor(av::AllocationKind::client, true, av::SurfaceLayout(),
                                                  4096u, av::ScanoutTarget());
        std::uint8_t bytes[av::AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size] = {};
        CHECK_STATUS(descriptor.encode(bytes, av::AllocationDescriptor::encoded_size), Status::ok);
        const av::SharedObjectRecord asking(0u, av::SharedObjectKind(SharedObjectKind::memory), 0u,
                                            object_bytes);
        CHECK_STATUS(asking.encode(bytes + av::AllocationDescriptor::encoded_size,
                                   av::SharedObjectRecord::encoded_size),
                     Status::ok);
        std::uint32_t allocation = 0u;
        CHECK_STATUS(kmt_.create_shared_allocation(device_, bytes, sizeof bytes, allocation), Status::ok);
        std::uint64_t named = 0u;
        CHECK_STATUS(kmt_.share_object(device_, allocation, named), Status::ok);
        carrier_ = allocation;
        return named;
    }

    std::uint64_t share_again()
    {
        std::uint64_t named = 0u;
        CHECK_STATUS(kmt_.share_object(device_, carrier_, named), Status::ok);
        return named;
    }

private:
    av::MockPlatform        blocks_;
    av::LockTrace           trace_;
    av::RecordingThreadLock lock_;
    MockKmt                 kmt_;
    av::RecordingLog        log_;
    MockTransport           wire_;
    std::uint32_t           device_;
    av::icd::SharedHandles  handles_;
    ClExternalMemory        memory_;
    std::uint32_t           carrier_ = 0u;
};

class Properties {
public:
    Properties() : pairs_{}, list_() { CHECK(list_.read(pairs_)); }
    Properties(std::uint64_t name, std::uint64_t value) : pairs_{name, value, 0u, 0u}, list_()
    { CHECK(list_.read(pairs_)); }
    Properties(std::uint64_t name, std::uint64_t value, std::uint64_t second, std::uint64_t second_value)
        : pairs_{name, value, second, second_value}, list_()
    { CHECK(list_.read(pairs_)); }
    Properties(const Properties &) = delete;
    Properties &operator=(const Properties &) = delete;

    const ClPropertyList &list() const { return list_; }

private:
    std::uint64_t  pairs_[6];
    ClPropertyList list_;
};

}

TEST(cl_external_memory, a_new_table_holds_nothing_and_asks_for_nothing)
{ Process          process;
  const Properties empty;
  CHECK(!process.memory().asked_for(empty.list()));
  CHECK_EQ(process.memory().live(), 0u);
  CHECK(process.memory().invariants_hold()); }

TEST(cl_external_memory, every_external_handle_property_is_an_external_create)
{ Process process;
  const std::uint64_t external[] = {CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR,
                                    CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KMT_KHR,
                                    CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_NAME_KHR,
                                    CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_FD_KHR,
                                    CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR};
  for (const std::uint64_t name : external) {
      const Properties properties(name, 7u);
      CHECK(process.memory().asked_for(properties.list()));
  }
  const Properties other(CL_MEM_DEVICE_HANDLE_LIST_KHR, 0u);
  CHECK(!process.memory().asked_for(other.list()));
  const Properties none;
  CHECK(!process.memory().asked_for(none.list())); }

TEST(cl_external_memory, a_handle_this_process_opened_reaches_the_object_behind_it)
{ Process             process;
  const std::uint64_t named = process.handle();
  const Properties    properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named);
  const Answer        answered(0u, host_buffer);
  process.wire().set_reply(answered.bytes(), SharedObjectAnswer::encoded_size);
  std::uint64_t made = 0u;
  CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes,
                                 made),
           CL_SUCCESS);
  CHECK_EQ(made, host_buffer);
  CHECK_EQ(process.memory().live(), 1u);
  CHECK(process.memory().invariants_hold());
  CHECK_EQ(process.wire().last_command(), av::CommandHeader::open_shared_object_id);
  std::uint64_t object_id = 0u;
  std::memcpy(&object_id, process.wire().last_payload(), sizeof object_id);
  CHECK(object_id != 0u);
  CHECK(object_id != named);

  char            dump[512] = {};
  av::StateWriter writer(dump, sizeof dump);
  writer.open("context", 0u);
  process.memory().report(writer);
  writer.close();
  CHECK(std::strstr(writer.dump(), "external_memory/0 live=1") != nullptr);
  CHECK(std::strstr(writer.dump(), "size=16384") != nullptr);

  const Answer released(0u, 0u);
  process.wire().set_reply(released.bytes(), SharedObjectAnswer::encoded_size);
  CHECK(process.memory().give_back(process.wire(), host_buffer));
  CHECK_EQ(process.wire().last_command(), av::CommandHeader::release_shared_object_id);
  CHECK_EQ(process.memory().live(), 0u);
  CHECK(process.memory().invariants_hold());
  CHECK(!process.memory().give_back(process.wire(), 0u));

  const Answer again(0u, host_buffer + 1u);
  process.wire().set_reply(again.bytes(), SharedObjectAnswer::encoded_size);
  std::uint64_t second = 0u;
  CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes,
                                 second),
           CL_SUCCESS);
  CHECK_EQ(second, host_buffer + 1u);
  CHECK_EQ(process.log().holding("avcl: a Win32 handle this process may not open"), 0u);

  CHECK(process.memory().answered(host_buffer + 1u));
  CHECK(!process.memory().answered(host_buffer));
  CHECK(!process.memory().answered(0u));
  CHECK(!process.memory().answered(object_id)); }

TEST(cl_external_memory, each_way_an_open_is_refused_leaves_no_row)
{ {
    Process          process;
    const Properties properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, 0x4004u);
    std::uint64_t    made = 9u;
    CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes,
                                   made),
             CL_INVALID_VALUE);
    CHECK_EQ(made, 0u);
    CHECK_EQ(process.memory().live(), 0u);
    CHECK_EQ(process.wire().sends(), 0u);
    CHECK_EQ(process.log().holding("avcl: a Win32 handle this process may not open"), 1u); }

  {
    Process          process;
    const Properties properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, 0u);
    std::uint64_t    made = 9u;
    CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes,
                                   made),
             CL_INVALID_VALUE);
    CHECK_EQ(process.wire().sends(), 0u); }

  {
    Process             process;
    const std::uint64_t named = process.handle();
    const std::uint64_t refused[] = {CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KMT_KHR,
                                     CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_NAME_KHR,
                                     CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_FD_KHR,
                                     CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR};
    for (const std::uint64_t name : refused) {
        const Properties properties(name, named);
        std::uint64_t    made = 9u;
        CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(),
                                       object_bytes, made),
                 CL_INVALID_PROPERTY);
        CHECK_EQ(made, 0u);
    }
    CHECK_EQ(process.memory().live(), 0u);
    CHECK_EQ(process.wire().sends(), 0u); }

  {
    Process             process;
    const std::uint64_t named = process.handle();
    const Properties    both(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named,
                             CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR, 3u);
    CHECK(process.memory().asked_for(both.list()));
    std::uint64_t made = 9u;
    CHECK_EQ(process.memory().open(process.wire(), process.device(), both.list(), object_bytes, made),
             CL_INVALID_PROPERTY);
    CHECK_EQ(made, 0u);
    CHECK_EQ(process.memory().live(), 0u);
    CHECK_EQ(process.wire().sends(), 0u); }

  {
    Process             process;
    const std::uint64_t named = process.handle();
    const Properties    properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named);
    std::uint64_t       made = 9u;
    CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), 0u, made),
             CL_INVALID_VALUE);
    CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(),
                                   ~std::uint64_t{0}, made),
             CL_INVALID_VALUE);
    CHECK_EQ(process.wire().sends(), 0u);
    CHECK_EQ(process.memory().live(), 0u); }

  {
    Process          process;
    const Properties none;
    std::uint64_t    made = 9u;
    CHECK_EQ(process.memory().open(process.wire(), process.device(), none.list(), object_bytes, made),
             CL_INVALID_VALUE);
    CHECK_EQ(made, 0u);
    CHECK_EQ(process.wire().sends(), 0u); }

  {
    Process             process;
    const std::uint64_t named = process.handle();
    const Properties    properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named);
    const Answer        nothing(0u, 0u);
    process.wire().set_reply(nothing.bytes(), SharedObjectAnswer::encoded_size);
    std::uint64_t made = 9u;
    CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes,
                                   made),
             CL_OUT_OF_RESOURCES);
    CHECK_EQ(made, 0u);
    CHECK_EQ(process.memory().live(), 0u); }

  {
    Process             process;
    const std::uint64_t named = process.handle();
    const Properties    properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named);
    const Answer        refused(2u, 0u);
    process.wire().set_reply(refused.bytes(), SharedObjectAnswer::encoded_size);
    std::uint64_t made = 9u;
    CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes,
                                   made),
             CL_OUT_OF_RESOURCES);
    CHECK_EQ(made, 0u);
    CHECK_EQ(process.memory().live(), 0u);
    CHECK(process.memory().invariants_hold());
    CHECK_EQ(process.log().holding("avcl: the device would not open a shared object for this context"),
             1u); } }

TEST(cl_external_memory, the_size_named_is_the_application_s_rounded_to_pages)
{ Process             process;
  const std::uint64_t named = process.handle();
  const Properties    properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named);
  const Answer        answered(0u, host_buffer);
  process.wire().set_reply(answered.bytes(), SharedObjectAnswer::encoded_size);
  std::uint64_t made = 0u;
  CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), 4097u, made),
           CL_SUCCESS);
  std::uint64_t asked = 0u;
  std::memcpy(&asked, process.wire().last_payload() + 24u, sizeof asked);
  CHECK_EQ(asked, 8192u); }

TEST(cl_external_memory, a_context_going_gives_back_everything_it_opened)
{ Process process;
  for (std::uint32_t index = 0u; index < 3u; index++) {
      const std::uint64_t named = process.handle();
      const Properties    properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named);
      const Answer        answered(0u, host_buffer + index);
      process.wire().set_reply(answered.bytes(), SharedObjectAnswer::encoded_size);
      std::uint64_t made = 0u;
      CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes,
                                     made),
               CL_SUCCESS);
      CHECK_EQ(made, host_buffer + index);
      CHECK(process.memory().invariants_hold());
  }
  CHECK_EQ(process.memory().live(), 3u);
  const std::uint32_t sends = process.wire().sends();
  process.memory().give_back(process.wire(), host_buffer + 9u);
  CHECK_EQ(process.wire().sends(), sends);
  CHECK_EQ(process.memory().live(), 3u);

  const Answer released(0u, 0u);
  process.wire().set_reply(released.bytes(), SharedObjectAnswer::encoded_size);
  process.memory().release_all(process.wire());
  CHECK_EQ(process.memory().live(), 0u);
  CHECK_EQ(process.wire().sends(), sends + 3u);
  CHECK(process.memory().invariants_hold());
  process.memory().release_all(process.wire());
  CHECK_EQ(process.wire().sends(), sends + 3u); }

TEST(cl_external_memory, an_object_this_context_already_holds_is_not_opened_again)
{ Process             process;
  const std::uint64_t named = process.handle();
  const Properties    properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named);
  const Answer        answered(0u, host_buffer);
  process.wire().set_reply(answered.bytes(), SharedObjectAnswer::encoded_size);
  std::uint64_t made = 0u;
  CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes, made),
           CL_SUCCESS);
  const std::uint32_t sends = process.wire().sends();

  const std::uint64_t again = process.share_again();
  const Properties    twice(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, again);
  CHECK(again != named);
  std::uint64_t second = 9u;
  CHECK_EQ(process.memory().open(process.wire(), process.device(), twice.list(), object_bytes, second),
           CL_INVALID_VALUE);
  CHECK_EQ(second, 0u);
  CHECK_EQ(process.wire().sends(), sends);
  CHECK_EQ(process.memory().live(), 1u);
  CHECK(process.memory().invariants_hold());
  CHECK_EQ(process.log().holding("avcl: an object this context already imported"), 1u); }

TEST(cl_external_memory, no_memory_for_a_row_refuses_before_the_device_is_asked)
{ Process             process;
  const std::uint64_t named = process.handle();
  const Properties    properties(CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, named);
  const std::uint32_t sends = process.wire().sends();
  process.blocks().fail_allocation_at(2u);
  std::uint64_t made = 9u;
  CHECK_EQ(process.memory().open(process.wire(), process.device(), properties.list(), object_bytes, made),
           CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(made, 0u);
  CHECK_EQ(process.wire().sends(), sends);
  CHECK_EQ(process.memory().live(), 0u);
  CHECK(process.memory().invariants_hold()); }

TEST(cl_external_memory, invariants_reject_rows_no_open_leaves_behind)
{ Process process;
  CHECK(process.memory().invariants_hold());
  av::icd::OpenedObjectsProbe::add(ClExternalMemoryProbe::opened(process.memory()),
                                   av::icd::OpenedObjectRow{});
  CHECK(!process.memory().invariants_hold()); }
