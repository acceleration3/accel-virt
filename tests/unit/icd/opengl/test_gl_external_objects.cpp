/* The memory objects and semaphores one GL context imported from a Win32
 * handle (feature 053, US2).
 *
 * An OpenGL name on this wire is the host's own and nothing translates one on
 * the way past, so a shared object needs a name before the host has made
 * anything: this hands one out at the create and learns the host's at the
 * import. Every test here therefore asks the same thing - does a name reach
 * the object it was imported for, and does a name that was never imported, or
 * never created, or looked up in the other GL namespace, reach nothing.
 */
#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/icd/lock_trace.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/transport/mock_transport.hpp"
#include "icd/common/shared_handles.hpp"
#include "icd/opengl/gl_external_objects.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/shared_object_answer.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

namespace av::glicd {

struct GlExternalObjectsProbe {
    static GlSharedObjectRow &row(GlExternalObjects &objects, std::uint32_t index)
    { return objects.rows_[index]; }
    static void set_live(GlExternalObjects &objects, std::uint32_t live) { objects.live_ = live; }
};

}  // namespace av::glicd

namespace {

using av::MockKmt;
using av::MockTransport;
using av::SharedObjectAnswer;
using av::SharedObjectKind;
using av::Status;
using av::glicd::GlExternalObjects;
using av::glicd::GlExternalObjectsProbe;
using av::glicd::GlSharedObjectRow;

constexpr std::uint64_t object_bytes = 4u * 4096u;
constexpr std::uint32_t host_name = 0x4321u;

/* The device's 16-byte answer, as the transport hands it back. */
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

/* One process: a kernel that mints identities and NT handles, the ICD's handle
 * table over it, and a transport playing the device. */
class Process {
public:
    Process() : trace_(), lock_(trace_, 'S'), kmt_(), log_(), device_(0u),
                handles_(kmt_, lock_), objects_(handles_, log_)
    {
        kmt_.add_adapter(av::PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u), 3200u);
        std::uint32_t adapter = 0u;
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter, 1u, count), Status::ok);
        CHECK_STATUS(kmt_.create_device(adapter, device_), Status::ok);
    }

    GlExternalObjects &objects() { return objects_; }
    MockTransport     &wire() { return wire_; }
    av::RecordingLog  &log() { return log_; }

    /* A handle this process may open: an allocation the kernel minted an
     * identity into, shared, and adopted into the ICD's table. */
    void *handle()
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
        std::uint64_t id = 0u;
        CHECK_STATUS(handles_.adopt(device_, named, id), Status::ok);
        return reinterpret_cast<void *>(static_cast<std::uintptr_t>(named));
    }

private:
    av::LockTrace            trace_;
    av::RecordingThreadLock  lock_;
    MockKmt                  kmt_;
    av::RecordingLog         log_;
    MockTransport            wire_;
    std::uint32_t            device_;
    av::icd::SharedHandles   handles_;
    GlExternalObjects        objects_;
};

std::uint64_t u64_at(const std::uint8_t *bytes, std::size_t at)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

}  // namespace

TEST(gl_external_objects, a_new_table_names_nothing)
{ Process one;
  CHECK_EQ(one.objects().live(), 0u);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, 1u), 0u);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, 0u), 0u);
  CHECK_EQ(one.objects().made(0u, 1u), 0u);
  CHECK(one.objects().invariants_hold()); }

/* The create asks the host nothing: there is no object to name yet. */
TEST(gl_external_objects_create, hands_out_names_of_its_own_and_sends_nothing)
{ Process       one;
  std::uint32_t names[3] = {};
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 3, names), GlExternalObjects::gl_no_error);
  CHECK_EQ(one.wire().sends(), 0u);
  CHECK(names[0] != 0u);
  CHECK(names[1] != names[0]);
  CHECK(names[2] != names[1]);
  CHECK_EQ(one.objects().live(), 3u);
  /* Created and not imported names no host object, so every use of it is
   * refused rather than sent with a number the host would read as its own. */
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, names[0]), 0u);
  CHECK(one.objects().invariants_hold()); }

/* The two GL namespaces do not share: one number is a memory object and a
 * semaphore at once, and neither is found in the other's. */
TEST(gl_external_objects_create, the_two_namespaces_are_separate)
{ Process       one;
  std::uint32_t memory = 0u;
  std::uint32_t semaphore = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &memory), GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().create(SharedObjectKind::monitored_fence, 1, &semaphore),
           GlExternalObjects::gl_no_error);
  const Answer opened(SharedObjectAnswer::done, host_name);
  one.wire().set_reply(opened.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), SharedObjectKind::memory, memory, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, memory), host_name);
  /* The same number asked for in the other namespace is not this object. */
  CHECK_EQ(one.objects().made(SharedObjectKind::monitored_fence, memory), 0u);
  CHECK(one.objects().invariants_hold()); }

TEST(gl_external_objects_import, opens_the_object_the_handle_names_and_remembers_what_the_host_called_it)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &name), GlExternalObjects::gl_no_error);
  const Answer opened(SharedObjectAnswer::done, host_name);
  one.wire().set_reply(opened.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), SharedObjectKind::memory, name, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(one.wire().last_command(), av::CommandHeader::open_shared_object_id);
  /* The body names the object and its size, and carries no handle: an NT
   * handle means nothing to the device. */
  CHECK(u64_at(one.wire().last_payload(), 0u) != 0u);
  CHECK_EQ(u64_at(one.wire().last_payload(), 16u), 0u);
  CHECK_EQ(u64_at(one.wire().last_payload(), 24u), object_bytes);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, name), host_name);
  CHECK(one.objects().invariants_hold());

  /* Imported twice would leak the first object, and GL has no error that says
   * so: the second is refused as a bad name. */
  CHECK_EQ(one.objects().import(one.wire(), SharedObjectKind::memory, name, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, name), host_name); }

/* A semaphore carries no size, which is the wire body's rule for its kind. */
TEST(gl_external_objects_import, a_semaphore_is_opened_with_no_size_at_all)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::monitored_fence, 1, &name), GlExternalObjects::gl_no_error);
  const Answer opened(SharedObjectAnswer::done, host_name);
  one.wire().set_reply(opened.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), SharedObjectKind::monitored_fence, name, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(u64_at(one.wire().last_payload(), 24u), 0u);
  CHECK_EQ(one.objects().made(SharedObjectKind::monitored_fence, name), host_name); }

/* The refusal this table exists for: a number an application invented is not
 * a handle, and nothing is sent for it. */
TEST(gl_external_objects_import, a_handle_from_nowhere_is_an_invalid_operation_and_reaches_no_device)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &name), GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().import(one.wire(), SharedObjectKind::memory, name, object_bytes,
                                GlExternalObjects::opaque_win32, reinterpret_cast<void *>(0xDEADu)),
           GlExternalObjects::gl_invalid_operation);
  CHECK_EQ(one.wire().sends(), 0u);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, name), 0u);
  CHECK_EQ(one.log().holding("neither exported nor opened"), 1u); }

TEST(gl_external_objects_import, refuses_a_handle_type_a_name_and_a_kind_it_does_not_take)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &name), GlExternalObjects::gl_no_error);
  void *const handle = one.handle();
  /* GL_HANDLE_TYPE_OPAQUE_FD_EXT: a type this path does not answer. */
  CHECK_EQ(one.objects().import(one.wire(), SharedObjectKind::memory, name, object_bytes, 0x9586u, handle),
           GlExternalObjects::gl_invalid_operation);
  /* A name this context never created, and one of the other namespace. */
  CHECK_EQ(one.objects().import(one.wire(), SharedObjectKind::memory, name + 0x1000u, object_bytes,
                                GlExternalObjects::opaque_win32, handle),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.objects().import(one.wire(), SharedObjectKind::monitored_fence, name, object_bytes,
                                GlExternalObjects::opaque_win32, handle),
           GlExternalObjects::gl_invalid_value);
  /* A kind the wire does not have. */
  CHECK_EQ(one.objects().import(one.wire(), SharedObjectKind::native_fence + 1u, name, object_bytes,
                                GlExternalObjects::opaque_win32, handle),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.wire().sends(), 0u); }

/* A device that will not open it, and one that answers a number no GL name
 * could be: neither leaves a row claiming an object. */
TEST(gl_external_objects_import, a_device_that_answers_no_usable_name_leaves_the_row_empty)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &name), GlExternalObjects::gl_no_error);
  const Answer refused(SharedObjectAnswer::unknown_object, 0u);
  one.wire().set_reply(refused.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), SharedObjectKind::memory, name, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_out_of_memory);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, name), 0u);
  CHECK(one.objects().invariants_hold());

  const Answer too_wide(SharedObjectAnswer::done, 0x1'0000'0001ull);
  one.wire().set_reply(too_wide.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), SharedObjectKind::memory, name, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_out_of_memory);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, name), 0u);
  CHECK(one.objects().invariants_hold()); }

TEST(gl_external_objects_destroy, releases_what_it_imported_and_ignores_a_name_it_never_made)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &name), GlExternalObjects::gl_no_error);
  const Answer opened(SharedObjectAnswer::done, host_name);
  one.wire().set_reply(opened.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), SharedObjectKind::memory, name, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_no_error);
  const Answer done(SharedObjectAnswer::done, 0u);
  one.wire().set_reply(done.bytes(), SharedObjectAnswer::encoded_size);
  const std::uint32_t before = one.wire().sends();
  CHECK_EQ(one.objects().destroy(one.wire(), SharedObjectKind::memory, 1, &name),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(one.wire().last_command(), av::CommandHeader::release_shared_object_id);
  CHECK_EQ(one.wire().sends(), before + 1u);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, name), 0u);
  CHECK_EQ(one.objects().live(), 0u);
  /* A name it never made is ignored, as GL ignores one it never generated. */
  const std::uint32_t quiet = one.wire().sends();
  CHECK_EQ(one.objects().destroy(one.wire(), SharedObjectKind::memory, 1, &name),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(one.wire().sends(), quiet);
  CHECK(one.objects().invariants_hold()); }

/* A name created and never imported has nothing on the device to release. */
TEST(gl_external_objects_destroy, a_name_that_was_never_imported_sends_nothing)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &name), GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().destroy(one.wire(), SharedObjectKind::memory, 1, &name),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(one.wire().sends(), 0u);
  CHECK_EQ(one.objects().live(), 0u); }

TEST(gl_external_objects, refuses_a_count_or_an_array_no_call_can_have)
{ Process       one;
  std::uint32_t names[2] = {};
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, -1, names), GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, nullptr), GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.objects().destroy(one.wire(), SharedObjectKind::memory, -1, names),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.objects().destroy(one.wire(), SharedObjectKind::memory, 1, nullptr),
           GlExternalObjects::gl_invalid_value);
  /* A count of zero is a call that asks for nothing, which GL allows. */
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 0, nullptr), GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().destroy(one.wire(), SharedObjectKind::memory, 0, nullptr),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().live(), 0u); }

/* All of them or none: a half-filled array is one an application reads whole,
 * and the names it did not get would be whatever was on its stack. */
TEST(gl_external_objects_create, fills_up_and_then_writes_no_names_at_all)
{ Process       one;
  std::uint32_t names[GlExternalObjects::max_objects] = {};
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, GlExternalObjects::max_objects - 1, names),
           GlExternalObjects::gl_no_error);
  std::uint32_t two[2] = {0xAAAAu, 0xBBBBu};
  CHECK_EQ(one.objects().create(SharedObjectKind::monitored_fence, 2, two), GlExternalObjects::gl_out_of_memory);
  CHECK_EQ(two[0], 0xAAAAu);
  CHECK_EQ(two[1], 0xBBBBu);
  CHECK_EQ(one.objects().create(SharedObjectKind::monitored_fence, 1, two), GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().live(), GlExternalObjects::max_objects);
  CHECK(one.objects().invariants_hold()); }

/* A context torn down gives every object back: the device counts it as a
 * holder of each, and one left behind would outlive the context naming it. */
TEST(gl_external_objects_release_all, gives_back_every_object_it_still_holds)
{ Process       one;
  std::uint32_t names[2] = {};
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 2, names), GlExternalObjects::gl_no_error);
  const Answer opened(SharedObjectAnswer::done, host_name);
  one.wire().set_reply(opened.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), SharedObjectKind::memory, names[0], object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_no_error);
  const Answer done(SharedObjectAnswer::done, 0u);
  one.wire().set_reply(done.bytes(), SharedObjectAnswer::encoded_size);
  const std::uint32_t before = one.wire().sends();
  one.objects().release_all(one.wire());
  CHECK_EQ(one.objects().live(), 0u);
  /* One release for the object it imported, and none for the name it did
   * not. */
  CHECK_EQ(one.wire().sends(), before + 1u);
  CHECK(one.objects().invariants_hold());
  one.objects().release_all(one.wire());
  CHECK_EQ(one.wire().sends(), before + 1u); }

TEST(gl_external_objects_report, says_how_many_it_holds)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &name), GlExternalObjects::gl_no_error);
  char             text[256] = {};
  av::StateWriter  writer(text, sizeof text);
  one.objects().report(writer);
  CHECK(std::strstr(writer.dump(), "external_objects") != nullptr);
  CHECK(std::strstr(writer.dump(), "live=1") != nullptr); }

TEST(gl_external_objects_invariants, refuse_each_state_a_create_or_an_import_never_leaves)
{ Process       one;
  std::uint32_t names[2] = {};
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 2, names), GlExternalObjects::gl_no_error);
  CHECK(one.objects().invariants_hold());
  GlSharedObjectRow &second = GlExternalObjectsProbe::row(one.objects(), 1u);
  const GlSharedObjectRow whole = second;

  second.name_ = 0u;
  CHECK(!one.objects().invariants_hold());
  second.name_ = names[0];
  CHECK(!one.objects().invariants_hold());   /* one name, two rows */
  second = whole;
  CHECK(one.objects().invariants_hold());
  /* A name this table never handed out. */
  second.name_ = 0xFFFFu;
  CHECK(!one.objects().invariants_hold());
  second = whole;
  /* Half an import either way. */
  second.made_ = host_name;
  CHECK(!one.objects().invariants_hold());
  second = whole;
  second.object_id_ = 9u;
  CHECK(!one.objects().invariants_hold());
  second = whole;
  /* Memory imported with no size, and a semaphore claiming one. */
  second.made_ = host_name;
  second.object_id_ = 9u;
  CHECK(!one.objects().invariants_hold());
  second.size_ = object_bytes;
  CHECK(one.objects().invariants_hold());
  second.memory_ = false;
  CHECK(!one.objects().invariants_hold());
  second = whole;
  CHECK(one.objects().invariants_hold());
  GlExternalObjectsProbe::set_live(one.objects(), GlExternalObjects::max_objects + 1u);
  CHECK(!one.objects().invariants_hold()); }
