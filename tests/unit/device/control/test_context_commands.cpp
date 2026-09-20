/* The context commands the core answers, request byte by request byte: the
 * capset queries, context creation and destruction, and context setup. */
#include "core/support/status.hpp"
#include "device/capset/device_capset.hpp"
#include "device/context/device_context.hpp"
#include "device/context/device_context_table.hpp"
#include "device/control/context_commands.hpp"
#include "device/recording/wire_recorder.hpp"
#include "device/control/control_request.hpp"
#include "device/execution/executor_library.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/abi/rvk_executor.h"
#include "harness/executor/mock_executor_ops.hpp"
#include "device/memory/blob_table.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/gpu_request_builder.hpp"
#include "harness/host/host_violation.hpp"
#include "device/memory/host_shared_objects.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "harness/ring/ring_blob_writer.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/virtio_gpu/context_request.hpp"

#include <cstdint>
#include <cstring>

using av::BlobTable;
using av::CapsetInfo;
using av::ContextCommands;
using av::ContextSetup;
using av::ControlRequest;
using av::DeviceCapset;
using av::DeviceContext;
using av::DeviceContextTable;
using av::GpuRequestBuilder;
using av::HostViolation;
using av::MockHost;
using av::MockRequest;
using av::ProtocolVersion;
using av::Status;
using av::SubApi;

namespace {

constexpr std::uint32_t ok_nodata = 0x1100u, ok_capset_info = 0x1102u, ok_capset = 0x1103u,
                        err_unanswered = 0u, err_out_of_memory = 0x1201u, err_invalid_resource_id = 0x1203u,
                        err_invalid_context_id = 0x1204u, err_invalid_parameter = 0x1205u;
constexpr std::uint32_t ring = 20u, fence = 21u, small_ring = 22u, headless_ring = 23u, second_ring = 24u,
                        third_ring = 25u;
constexpr std::uint32_t vulkan_only = static_cast<std::uint32_t>(SubApi::vulkan);
constexpr std::uint32_t vulkan_and_gl = vulkan_only | static_cast<std::uint32_t>(SubApi::opengl);
constexpr std::uint8_t  uuid[16] = {0x7b, 0x7a};

GpuRequestBuilder capset_info(std::uint32_t index)
{
    GpuRequestBuilder request(ContextCommands::get_capset_info);
    request.u32(index).u32(0u);
    return request;
}

GpuRequestBuilder capset(std::uint32_t id, std::uint32_t version)
{
    GpuRequestBuilder request(ContextCommands::get_capset);
    request.u32(id).u32(version);
    return request;
}

GpuRequestBuilder create(std::uint32_t id, std::uint32_t context_init = CapsetInfo::capset_id,
                         std::uint32_t name_length = 6u)
{
    GpuRequestBuilder request(ContextCommands::ctx_create);
    request.context(id).u32(name_length).u32(context_init).zeros(64u);
    return request;
}

GpuRequestBuilder destroy(std::uint32_t id)
{
    GpuRequestBuilder request(ContextCommands::ctx_destroy);
    request.context(id);
    return request;
}

/* The setup's own size unless `size` says otherwise. */
GpuRequestBuilder submit(std::uint32_t id, const ContextSetup &setup, std::uint32_t size = 0u)
{
    std::uint8_t bytes[ContextSetup::encoded_size] = {};
    static_cast<void>(setup.encode(bytes, sizeof bytes));
    GpuRequestBuilder request(ContextCommands::submit_3d);
    request.context(id).u32(size != 0u ? size : static_cast<std::uint32_t>(setup.size())).u32(0u);
    request.append(bytes, setup.size());
    return request;
}

ContextSetup setup(std::uint32_t ring_id = ring, std::uint8_t order = 12u, std::uint16_t major = 1u,
                   std::uint32_t doorbell = 0u, std::uint32_t reply = 0u, std::uint16_t minor = 0u)
{
    return ContextSetup(ProtocolVersion(major, minor), SubApi::vulkan, ring_id, order, fence, doorbell, reply);
}

class Fixture {
public:
    /* A device offering `sub_apis`, over a library of `abi_version`. */
    explicit Fixture(std::uint32_t sub_apis = vulkan_only, std::uint32_t abi_version = RVK_EXECUTOR_ABI_VERSION)
        : blobs_(host_), shared_(host_), interrupts_(host_), ops_(sub_apis), library_(), recorder_(host_),
          contexts_(host_, blobs_, shared_, interrupts_, library_, recorder_), capset_(host_),
          commands_(host_, contexts_, capset_)
    {
        static_cast<void>(blobs_.initialize());
        static_cast<void>(shared_.initialize());
        static_cast<void>(contexts_.initialize());
        ops_.editable().abi_version = abi_version;
        static_cast<void>(library_.accept(&ops_.table()));
        host_.add_gpu(uuid);
        host_.set_hostmem_size(0x10000u);
        capset_.describe(0u, sub_apis, av::CapsetInfo::feature_host_visible_memory);
        static_cast<void>(blobs_.create_host(ring, 0x2000u));
        static_cast<void>(blobs_.create_host(fence, 0x1000u));
        static_cast<void>(blobs_.create_host(small_ring, 0x1000u));
        static_cast<void>(blobs_.create_host(headless_ring, 0x2000u));
        static_cast<void>(blobs_.create_host(second_ring, 0x2000u));
        static_cast<void>(blobs_.create_host(third_ring, 0x2000u));
        av::RingBlobWriter writer(blobs_);
        writer.write_header(ring, 12u);
        writer.write_header(small_ring, 12u);
        writer.write_header(second_ring, 12u);
        writer.write_header(third_ring, 12u);
    }

    /* The response type, or 0 when the core left the request to the base class. */
    std::uint32_t send(const GpuRequestBuilder &builder)
    {
        MockRequest mock(builder.bytes(), builder.size());
        ControlRequest request(host_, &mock);
        std::uint32_t type = 0u;
        std::memcpy(&type, builder.bytes(), sizeof type);
        if (!commands_.answer(request, type)) return err_unanswered;
        std::memcpy(last_, mock.response_bytes(), mock.response_size());
        last_size_ = mock.response_size();
        return mock.response_type();
    }
    const std::uint8_t *last() const { return last_; }
    std::size_t         last_size() const { return last_size_; }
    MockHost           &host() { return host_; }
    BlobTable          &blobs() { return blobs_; }
    DeviceContextTable &contexts() { return contexts_; }
    av::MockExecutorOps &ops() { return ops_; }

private:
    MockHost            host_;
    BlobTable           blobs_;
    av::HostSharedObjects shared_;
    av::FenceInterrupts interrupts_;
    av::MockExecutorOps ops_;
    av::ExecutorLibrary library_;
    av::WireRecorder    recorder_;
    DeviceContextTable  contexts_;
    DeviceCapset       capset_;
    ContextCommands    commands_;
    std::uint8_t       last_[256] = {};
    std::size_t        last_size_ = 0u;
};

std::uint32_t u32(const std::uint8_t *bytes, std::size_t offset)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}

}  // namespace

TEST(context_commands_capset_info, names_one_capset_at_index_zero)
{ Fixture fixture;
  CHECK_EQ(fixture.send(capset_info(0u)), ok_capset_info);
  CHECK_EQ(fixture.last_size(), 40u);
  CHECK_EQ(u32(fixture.last(), 24u), 0x72u);
  CHECK_EQ(u32(fixture.last(), 28u), 1u);
  CHECK_EQ(u32(fixture.last(), 32u), 128u);
  CHECK_EQ(fixture.send(capset_info(1u)), err_invalid_parameter);
  CHECK_EQ(fixture.send(capset_info(0u).truncate(31u)), err_invalid_parameter); }

TEST(context_commands_capset, answers_the_described_capset_for_its_id_and_version_only)
{ Fixture fixture;
  CHECK_EQ(fixture.send(capset(0x72u, 1u)), ok_capset);
  CHECK_EQ(fixture.last_size(), 152u);
  CapsetInfo decoded;
  CHECK_STATUS(decoded.decode(fixture.last() + 24u, CapsetInfo::encoded_size), Status::ok);
  CHECK(decoded.offers(SubApi::vulkan));
  CHECK(decoded.has_feature(CapsetInfo::feature_host_visible_memory));
  CHECK(std::memcmp(decoded.gpu().uuid(), uuid, sizeof uuid) == 0);
  CHECK_EQ(fixture.send(capset(0x04u, 1u)), err_invalid_parameter);
  CHECK_EQ(fixture.send(capset(0x72u, 2u)), err_invalid_parameter);
  CHECK_EQ(fixture.send(capset(0x72u, 1u).truncate(31u)), err_invalid_parameter); }

TEST(context_commands_create, creates_a_context_on_the_rvk_capset)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create(1u)), ok_nodata);
  CHECK_EQ(fixture.send(create(2u, CapsetInfo::capset_id, av::ContextRequest::name_capacity)), ok_nodata);
  CHECK_EQ(fixture.contexts().live(), 2u);
  CHECK(fixture.contexts().invariants_hold()); }

TEST(context_commands_create, refuses_other_capsets_long_names_and_ids_it_cannot_take)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create(1u)), ok_nodata);
  CHECK_EQ(fixture.send(create(1u)), err_invalid_context_id);
  CHECK_EQ(fixture.send(create(0u)), err_invalid_context_id);
  CHECK_EQ(fixture.send(create(2u, 0u)), err_invalid_parameter);              /* virgl's default */
  CHECK_EQ(fixture.send(create(2u, 0x04u)), err_invalid_parameter);           /* venus */
  CHECK_EQ(fixture.send(create(2u, 0x172u)), err_invalid_parameter);          /* reserved bits */
  CHECK_EQ(fixture.send(create(2u, CapsetInfo::capset_id, 65u)), err_invalid_parameter);
  CHECK_EQ(fixture.send(create(2u).truncate(95u)), err_invalid_parameter);
  CHECK_EQ(fixture.contexts().live(), 1u);
  fixture.host().fail_thread_start_at(1u);
  CHECK_EQ(fixture.send(create(2u)), err_out_of_memory);
  CHECK_EQ(fixture.contexts().live(), 1u); }

TEST(context_commands_create, a_full_table_is_out_of_memory)
{ Fixture fixture;
  for (std::uint32_t id = 1u; id <= DeviceContextTable::max_contexts; id++) CHECK_EQ(fixture.send(create(id)), ok_nodata);
  CHECK_EQ(fixture.send(create(1000u)), err_out_of_memory);
  fixture.contexts().destroy_all(); }

TEST(context_commands_destroy, destroys_a_live_context_only)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create(3u)), ok_nodata);
  CHECK_EQ(fixture.send(destroy(3u)), ok_nodata);
  CHECK_EQ(fixture.send(destroy(3u)), err_invalid_context_id);
  CHECK_EQ(fixture.send(destroy(0u)), err_invalid_context_id);
  CHECK_EQ(fixture.send(destroy(3u).truncate(23u)), err_invalid_parameter);
  CHECK_EQ(fixture.contexts().live(), 0u); }

TEST(context_commands_submit, a_setup_binds_the_context_until_it_is_destroyed)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create(4u)), ok_nodata);
  CHECK_EQ(fixture.send(submit(4u, setup())), ok_nodata);
  const DeviceContext *const context = fixture.contexts().find(4u);
  CHECK(context != nullptr && context->binding().bound());
  CHECK_EQ(context->binding().ring_resource(), ring);
  CHECK_EQ(context->binding().fence_resource(), fence);
  CHECK_EQ(context->binding().version().major_version(), 1u);
  CHECK_STATUS(fixture.blobs().release(ring), Status::invalid_state);
  CHECK_EQ(fixture.send(submit(4u, setup())), err_invalid_parameter);   /* already set up */
  CHECK_EQ(fixture.send(destroy(4u)), ok_nodata);
  CHECK_STATUS(fixture.blobs().release(ring), Status::ok);
  CHECK_STATUS(fixture.blobs().release(fence), Status::ok); }

TEST(context_commands_submit, refuses_every_setup_it_cannot_bind)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create(5u)), ok_nodata);
  std::uint8_t bad_magic[ContextSetup::encoded_size] = {};
  static_cast<void>(setup().encode(bad_magic, sizeof bad_magic));
  bad_magic[0] ^= 0xFFu;
  GpuRequestBuilder malformed(ContextCommands::submit_3d);
  malformed.context(5u).u32(ContextSetup::encoded_size).u32(0u).append(bad_magic, sizeof bad_magic);
  struct Case {
      GpuRequestBuilder request;
      std::uint32_t     answer;
  } cases[] = {
      {submit(6u, setup()), err_invalid_context_id},
      {submit(5u, setup()).truncate(31u), err_invalid_parameter},
      {submit(5u, setup(), 31u), err_invalid_parameter},
      {submit(5u, setup()).truncate(48u), err_invalid_parameter},        /* the buffer is not all there */
      {submit(5u, setup(), 48u), err_invalid_parameter},                 /* a 32-byte setup named as 48 */
      {submit(5u, setup(ring, 12u, 1u, 0u, 0u, 1u), 32u), err_invalid_parameter},   /* a 48-byte one as 32 */
      {submit(5u, setup(ring, 12u, 1u, 0u, 0u, 1u)).truncate(79u), err_invalid_parameter},
      {submit(5u, setup(), 49u), err_invalid_parameter},                 /* past any setup */
      {malformed, err_invalid_parameter},
      {submit(5u, setup(99u)), err_invalid_resource_id},
      {submit(5u, setup(small_ring)), err_invalid_parameter},
      {submit(5u, setup(ring, 12u, 2u)), err_invalid_parameter},        /* another major */
      {submit(5u, setup(ring, 12u, 1u, 64u)), err_invalid_parameter},   /* no such doorbell */
      {submit(5u, setup(headless_ring)), err_invalid_parameter},        /* the guest wrote no ring header */
      {submit(5u, setup(ring, 12u, 1u, 0u, 99u)), err_invalid_resource_id},   /* no such reply slot */
  };
  for (Case &c : cases) CHECK_EQ(fixture.send(c.request), c.answer);
  CHECK(!fixture.contexts().find(5u)->binding().bound());
  CHECK_EQ(fixture.send(submit(5u, setup())), ok_nodata); }

/* The setup of the current minor is 48 bytes, and binds as the old one does. */
TEST(context_commands_submit, binds_a_setup_of_the_current_minor)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create(5u)), ok_nodata);
  const ContextSetup current = setup(ring, 12u, 1u, 0u, 0u, 1u);
  CHECK_EQ(current.size(), 48u);
  CHECK_EQ(fixture.send(submit(5u, current)), ok_nodata);
  CHECK(fixture.contexts().find(5u)->binding().bound()); }

TEST(context_commands_submit, a_host_that_cannot_stage_the_batches_is_out_of_memory)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create(1u)), ok_nodata);
  fixture.host().fail_allocation_at(1u);
  CHECK_EQ(fixture.send(submit(1u, setup())), err_out_of_memory);
  CHECK_EQ(fixture.send(submit(1u, setup())), ok_nodata); }

TEST(context_commands_submit, a_doorbell_another_context_holds_is_refused)
{ Fixture fixture;
  CHECK_EQ(fixture.send(create(1u)), ok_nodata);
  CHECK_EQ(fixture.send(create(2u)), ok_nodata);
  CHECK_EQ(fixture.send(submit(1u, setup(ring, 12u, 1u, 7u))), ok_nodata);
  CHECK_EQ(fixture.send(submit(2u, setup(second_ring, 12u, 1u, 7u))), err_invalid_parameter);
  CHECK(!fixture.contexts().find(2u)->binding().bound());
  CHECK_EQ(fixture.send(destroy(1u)), ok_nodata);
  CHECK_EQ(fixture.send(submit(2u, setup(second_ring, 12u, 1u, 7u))), ok_nodata); }

TEST(context_commands, a_device_without_a_capset_refuses_capset_queries_and_contexts)
{ MockHost host;
  host.add_gpu(uuid);
  BlobTable blobs(host);
  av::HostSharedObjects shared(host);
  av::FenceInterrupts interrupts(host);
  av::ExecutorLibrary library;
  av::WireRecorder recorder(host);
  DeviceContextTable contexts(host, blobs, shared, interrupts, library, recorder);
  const GpuRequestBuilder requests[] = {capset_info(0u), capset(0x72u, 1u), create(1u)};
  DeviceCapset undescribed(host);
  undescribed.describe(0u, 0u, 0u);
  ContextCommands commands(host, contexts, undescribed);
  for (const GpuRequestBuilder &builder : requests) {
      MockRequest mock(builder.bytes(), builder.size());
      ControlRequest request(host, &mock);
      std::uint32_t type = 0u;
      std::memcpy(&type, builder.bytes(), sizeof type);
      CHECK(commands.answer(request, type));
      CHECK_EQ(mock.response_type(), err_invalid_parameter);
  }
  CHECK_EQ(contexts.live(), 0u); }

TEST(context_commands, leaves_every_other_request_to_the_base_class)
{ Fixture fixture;
  CHECK_EQ(fixture.send(GpuRequestBuilder(0x0100u)), err_unanswered);
  CHECK_EQ(fixture.send(GpuRequestBuilder(0x0202u).u32(1u).u32(0u)), err_unanswered); }

TEST(context_commands, answer_is_main_loop_only)
{ Fixture fixture;
  struct Call {
      MockHost        *host;
      ContextCommands *commands;
  };
  DeviceCapset capset(fixture.host());
  ContextCommands commands(fixture.host(), fixture.contexts(), capset);
  Call call = {&fixture.host(), &commands};
  void *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread(
                   [](void *context) {
                       auto *const c = static_cast<Call *>(context);
                       ControlRequest request(*c->host, nullptr);
                       static_cast<void>(c->commands->answer(request, 0u));
                   },
                   &call, thread),
               Status::ok);
  fixture.host().join_thread(thread);
  CHECK(fixture.host().claim(HostViolation::main_loop_required)); }

/* ---- OpenGL options and sharing (feature 050) ---- */

namespace {

ContextSetup gl_setup(std::uint32_t ring_id, std::uint32_t doorbell, const av::GlContextOptions &options)
{
    return ContextSetup(ProtocolVersion(1u, ContextSetup::options_minor), SubApi::opengl, ring_id, 12u, fence,
                        doorbell, 0u, options);
}

const av::GlContextOptions core_33(3u, 3u, av::GlContextOptions::Profile::core, 0u, 0u);

}  // namespace

TEST(context_commands_submit, a_gl_context_shares_only_another_bound_gl_context)
{ Fixture fixture(vulkan_and_gl);
  CHECK_EQ(fixture.send(create(1u)), ok_nodata);
  CHECK_EQ(fixture.send(submit(1u, gl_setup(ring, 1u, core_33))), ok_nodata);
  CHECK_EQ(fixture.send(create(2u)), ok_nodata);
  CHECK_EQ(fixture.send(submit(2u, setup(second_ring, 12u, 1u, 2u))), ok_nodata);
  CHECK_EQ(fixture.send(create(3u)), ok_nodata);
  CHECK_EQ(fixture.send(create(4u)), ok_nodata);
  const std::uint32_t refused[] = {99u, 3u, 2u, 4u};
  for (const std::uint32_t share : refused) {
      CHECK_EQ(fixture.send(submit(3u, gl_setup(third_ring, 3u, core_33.sharing(share)))), err_invalid_parameter);
      CHECK(!fixture.contexts().find(3u)->binding().bound());
  }
  CHECK(!fixture.contexts().can_share(3u, 99u));
  CHECK(!fixture.contexts().can_share(1u, 1u));
  CHECK(!fixture.contexts().can_share(3u, 2u));
  CHECK(!fixture.contexts().can_share(3u, 4u));
  CHECK(!fixture.contexts().can_share(3u, 0u));
  CHECK(fixture.contexts().can_share(3u, 1u));
  CHECK_EQ(fixture.send(submit(3u, gl_setup(third_ring, 3u, core_33.sharing(1u)))), ok_nodata);
  CHECK(fixture.contexts().find(3u)->binding().bound());
  CHECK(fixture.contexts().invariants_hold()); }

/* A library before version 4 makes default contexts: options are refused
 * before anything is bound, and the defaults still bind. */
TEST(context_commands_submit, options_a_version_three_library_cannot_make_are_refused)
{ Fixture fixture(vulkan_and_gl, RVK_EXECUTOR_ABI_VERSION_HANDLES);
  CHECK_EQ(fixture.send(create(1u)), ok_nodata);
  CHECK_EQ(fixture.send(submit(1u, gl_setup(ring, 1u, core_33))), err_invalid_parameter);
  CHECK(!fixture.contexts().find(1u)->binding().bound());
  CHECK_EQ(fixture.host().live_doorbells(), 0u);
  CHECK_EQ(fixture.send(submit(1u, gl_setup(ring, 1u, av::GlContextOptions()))), ok_nodata);
  CHECK(fixture.contexts().find(1u)->binding().bound()); }
