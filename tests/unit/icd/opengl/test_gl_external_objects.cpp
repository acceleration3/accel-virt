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
#include "icd/common/opened_objects.hpp"
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

namespace av::icd {

struct OpenedObjectsProbe {
    static void set_live(OpenedObjects &opened, std::uint32_t live) { opened.live_ = live; }
};

}  // namespace av::icd

namespace av::glicd {

struct GlExternalObjectsProbe {
    static GlSharedObjectRow &row(GlExternalObjects &objects, std::uint32_t index)
    { return objects.rows_[index]; }
    static void set_live(GlExternalObjects &objects, std::uint32_t live) { objects.live_ = live; }
    static icd::OpenedObjects &opened(GlExternalObjects &objects) { return objects.opened_; }
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
    std::uint32_t      device() const { return device_; }

    /* A handle this process may open: an allocation the kernel minted an
     * identity into and shared - as another process would send it, so the
     * import is what adopts it into the ICD's table. */
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

std::uint32_t u32_at(const std::uint8_t *bytes, std::size_t at)
{
    std::uint32_t value = 0u;
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
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, memory, object_bytes,
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
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name, object_bytes,
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
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name, object_bytes,
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
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::monitored_fence, name, object_bytes,
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
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name, object_bytes,
                                GlExternalObjects::opaque_win32, reinterpret_cast<void *>(0xDEADu)),
           GlExternalObjects::gl_invalid_operation);
  CHECK_EQ(one.wire().sends(), 0u);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, name), 0u);
  CHECK_EQ(one.log().holding("avgl: a Win32 handle this process may not open"), 1u); }

TEST(gl_external_objects_import, refuses_a_handle_type_a_name_and_a_kind_it_does_not_take)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &name), GlExternalObjects::gl_no_error);
  void *const handle = one.handle();
  /* GL_HANDLE_TYPE_OPAQUE_FD_EXT: a type this path does not answer. */
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name, object_bytes, 0x9586u, handle),
           GlExternalObjects::gl_invalid_operation);
  /* A name this context never created, and one of the other namespace. */
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name + 0x1000u, object_bytes,
                                GlExternalObjects::opaque_win32, handle),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::monitored_fence, name, object_bytes,
                                GlExternalObjects::opaque_win32, handle),
           GlExternalObjects::gl_invalid_value);
  /* A kind the wire does not have. */
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::native_fence + 1u, name, object_bytes,
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
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_out_of_memory);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, name), 0u);
  CHECK(one.objects().invariants_hold());

  const Answer too_wide(SharedObjectAnswer::done, 0x1'0000'0001ull);
  one.wire().set_reply(too_wide.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name, object_bytes,
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
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name, object_bytes,
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
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, names[0], object_bytes,
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
  /* A host name nothing was opened for. */
  second.made_ = host_name;
  CHECK(!one.objects().invariants_hold());
  second = whole;
  /* One opened under the other namespace's kind is not this row's. */
  const Answer opened(SharedObjectAnswer::done, host_name);
  one.wire().set_reply(opened.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, names[1], object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_no_error);
  CHECK(one.objects().invariants_hold());
  second.memory_ = false;
  CHECK(!one.objects().invariants_hold());
  second.memory_ = true;
  /* An open with no row imported to answer for it. */
  second.made_ = 0u;
  CHECK(!one.objects().invariants_hold());
  second.made_ = host_name;
  CHECK(one.objects().invariants_hold());
  /* The opened objects' own invariants are this table's too. */
  av::icd::OpenedObjectsProbe::set_live(GlExternalObjectsProbe::opened(one.objects()),
                                        av::icd::OpenedObjects::max_objects + 1u);
  CHECK(!one.objects().invariants_hold());
  av::icd::OpenedObjectsProbe::set_live(GlExternalObjectsProbe::opened(one.objects()), 1u);
  CHECK(one.objects().invariants_hold());
  GlExternalObjectsProbe::set_live(one.objects(), GlExternalObjects::max_objects + 1u);
  CHECK(!one.objects().invariants_hold()); }

/* The three kinds the wire has, and the two it does not. A native fence is a
 * semaphore here exactly as a monitored one is - the guest tells them apart and
 * the host holds one timeline semaphore for both - and a kind from neither list
 * names nothing this table could make or destroy (feature 053). */
TEST(gl_external_objects, a_native_fence_is_a_semaphore_and_an_unknown_kind_is_neither)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::native_fence, 1, &name), GlExternalObjects::gl_no_error);
  CHECK(name != 0u);
  const Answer opened(SharedObjectAnswer::done, host_name);
  one.wire().set_reply(opened.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::native_fence, name, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_no_error);
  /* Asked for with no size, as both fence kinds are. */
  CHECK_EQ(u64_at(one.wire().last_payload(), 24u), 0u);
  CHECK_EQ(one.objects().made(SharedObjectKind::native_fence, name), host_name);
  /* And a monitored fence's name is the same namespace: one table, two. */
  CHECK_EQ(one.objects().made(SharedObjectKind::monitored_fence, name), host_name);

  /* A kind the wire does not have, on each call that takes one. */
  const std::uint32_t unknown = SharedObjectKind::native_fence + 1u;
  std::uint32_t       refused = 0u;
  CHECK_EQ(one.objects().create(unknown, 1, &refused), GlExternalObjects::gl_invalid_value);
  CHECK_EQ(refused, 0u);
  CHECK_EQ(one.objects().destroy(one.wire(), unknown, 1, &name), GlExternalObjects::gl_invalid_value);
  /* Nothing was given back, so it is still held. */
  CHECK_EQ(one.objects().made(SharedObjectKind::native_fence, name), host_name);
  CHECK(one.objects().invariants_hold());

  /* Destroying it for real releases it as a semaphore, which is the kind the
   * device was told - a fence released as memory is one the device would look
   * for in the wrong table. */
  const std::uint32_t sends = one.wire().sends();
  CHECK_EQ(one.objects().destroy(one.wire(), SharedObjectKind::native_fence, 1, &name),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(one.wire().sends(), sends + 1u);
  CHECK_EQ(u32_at(one.wire().last_payload(), 8u), SharedObjectKind::monitored_fence);
  CHECK_EQ(one.objects().made(SharedObjectKind::native_fence, name), 0u);
  CHECK(one.objects().invariants_hold()); }

/* Name zero is not a name. GL never generates it and an application that hands
 * it back is naming nothing, so every lookup answers nothing rather than the
 * first row it happens to hold. */
TEST(gl_external_objects, the_zero_name_belongs_to_no_row)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &name), GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, 0u), 0u);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, 0u, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_invalid_value);
  /* And destroying it is the same silence as destroying any name it never
   * made: no send, no refusal. */
  const std::uint32_t zero = 0u;
  CHECK_EQ(one.objects().destroy(one.wire(), SharedObjectKind::memory, 1, &zero),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(one.wire().sends(), 0u);
  CHECK_EQ(one.objects().live(), 1u);
  CHECK(one.objects().invariants_hold()); }

/* A device that answers a number whose low 32 bits are zero: not a GL name,
 * even though the wire accepted it as a handle, and the object is given back
 * rather than left held under a name nothing can use. */
TEST(gl_external_objects_import, a_device_that_answers_a_name_of_zero_in_32_bits_gives_it_back)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &name), GlExternalObjects::gl_no_error);
  const Answer truncating(SharedObjectAnswer::done, 0x1'0000'0000ull);
  one.wire().set_reply(truncating.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_out_of_memory);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, name), 0u);
  CHECK(one.objects().invariants_hold()); }

/* glIsMemoryObjectEXT and glIsSemaphoreEXT, which GL_EXT_memory_object and
 * GL_EXT_semaphore both promise. A created name is an object whether or not
 * anything was imported into it - that is what the extension says, and it is
 * why the table rather than the host is what answers: the host has no object
 * for a name until the import. */
TEST(gl_external_objects, a_created_name_is_an_object_before_it_is_imported)
{ Process       one;
  std::uint32_t memory = 0u;
  std::uint32_t semaphore = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &memory), GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().create(SharedObjectKind::monitored_fence, 1, &semaphore),
           GlExternalObjects::gl_no_error);
  CHECK(one.objects().holds(SharedObjectKind::memory, memory));
  CHECK(one.objects().holds(SharedObjectKind::monitored_fence, semaphore));
  /* Either fence kind asks about the one semaphore namespace. */
  CHECK(one.objects().holds(SharedObjectKind::native_fence, semaphore));
  /* And the two namespaces are separate: a memory name is no semaphore. */
  CHECK(!one.objects().holds(SharedObjectKind::monitored_fence, memory));
  CHECK(!one.objects().holds(SharedObjectKind::memory, semaphore));
  /* A name nothing made, the zero name, and a kind that is not one. */
  CHECK(!one.objects().holds(SharedObjectKind::memory, memory + 0x1000u));
  CHECK(!one.objects().holds(SharedObjectKind::memory, 0u));
  CHECK(!one.objects().holds(SharedObjectKind::native_fence + 1u, memory));
  /* Imported, it is still an object. */
  const Answer made(SharedObjectAnswer::done, host_name);
  one.wire().set_reply(made.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, memory, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_no_error);
  CHECK(one.objects().holds(SharedObjectKind::memory, memory));
  /* Deleted, it is not. */
  CHECK_EQ(one.objects().destroy(one.wire(), SharedObjectKind::memory, 1, &memory),
           GlExternalObjects::gl_no_error);
  CHECK(!one.objects().holds(SharedObjectKind::memory, memory));
  CHECK(one.objects().invariants_hold()); }

/* The two memory-object parameters, which are the reason this feature could
 * not report GL_EXT_memory_object until now (research.md §12). They are set
 * *before* the import and the import is what makes the host object, so there
 * is nothing to set them on when they are asked for - they are recorded here,
 * and the import refuses when either was asked for, which is a documented
 * error rather than a promise quietly broken. */
TEST(gl_external_objects, a_parameter_this_path_cannot_keep_is_refused_at_the_import)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &name), GlExternalObjects::gl_no_error);
  std::int32_t read = 9;
  CHECK_EQ(one.objects().get_parameter(SharedObjectKind::memory, name, GlExternalObjects::dedicated, &read),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(read, 0);
  CHECK_EQ(one.objects().set_parameter(SharedObjectKind::memory, name, GlExternalObjects::dedicated, 1),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().get_parameter(SharedObjectKind::memory, name, GlExternalObjects::dedicated, &read),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(read, 1);
  /* The import is refused, nothing is sent, and the name is still importable
   * once the application stops asking for dedicated memory. */
  const std::uint32_t sends = one.wire().sends();
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_invalid_operation);
  CHECK_EQ(one.wire().sends(), sends);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, name), 0u);
  CHECK_EQ(one.log().holding("avgl: a memory object asked for dedicated or protected memory"), 1u);

  CHECK_EQ(one.objects().set_parameter(SharedObjectKind::memory, name, GlExternalObjects::dedicated, 0),
           GlExternalObjects::gl_no_error);
  const Answer made(SharedObjectAnswer::done, host_name);
  one.wire().set_reply(made.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_no_error);
  CHECK(one.objects().invariants_hold()); }

/* Protected memory is the other one, and it is refused the same way. */
TEST(gl_external_objects, protected_memory_is_refused_at_the_import_too)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &name), GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().set_parameter(SharedObjectKind::memory, name,
                                       GlExternalObjects::protected_memory, 1),
           GlExternalObjects::gl_no_error);
  std::int32_t read = 0;
  CHECK_EQ(one.objects().get_parameter(SharedObjectKind::memory, name,
                                       GlExternalObjects::protected_memory, &read),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(read, 1);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_invalid_operation);
  CHECK(one.objects().invariants_hold()); }

/* Every way a parameter call can be wrong. A semaphore has no parameter this
 * path can keep: the one the registry gives it names a D3D12 fence's value,
 * and nothing here is ever imported from one. */
TEST(gl_external_objects, a_parameter_of_the_wrong_kind_name_or_pname_is_refused)
{ Process       one;
  std::uint32_t memory = 0u;
  std::uint32_t semaphore = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &memory), GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().create(SharedObjectKind::monitored_fence, 1, &semaphore),
           GlExternalObjects::gl_no_error);
  std::int32_t read = 9;
  /* A name this context did not make. */
  CHECK_EQ(one.objects().set_parameter(SharedObjectKind::memory, memory + 0x1000u,
                                       GlExternalObjects::dedicated, 1),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.objects().get_parameter(SharedObjectKind::memory, memory + 0x1000u,
                                       GlExternalObjects::dedicated, &read),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(read, 9);
  /* A pname the extension does not define for a memory object. */
  CHECK_EQ(one.objects().set_parameter(SharedObjectKind::memory, memory, 0x1234u, 1),
           GlExternalObjects::gl_invalid_enum);
  CHECK_EQ(one.objects().get_parameter(SharedObjectKind::memory, memory, 0x1234u, &read),
           GlExternalObjects::gl_invalid_enum);
  /* A semaphore has none at all: the registry's one names a D3D12 fence. */
  CHECK_EQ(one.objects().set_parameter(SharedObjectKind::monitored_fence, semaphore,
                                       GlExternalObjects::d3d12_fence_value, 7),
           GlExternalObjects::gl_invalid_enum);
  CHECK_EQ(one.objects().get_parameter(SharedObjectKind::monitored_fence, semaphore,
                                       GlExternalObjects::d3d12_fence_value, &read),
           GlExternalObjects::gl_invalid_enum);
  /* A kind that is not one, and nowhere to write the answer. */
  CHECK_EQ(one.objects().set_parameter(SharedObjectKind::native_fence + 1u, memory,
                                       GlExternalObjects::dedicated, 1),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.objects().get_parameter(SharedObjectKind::memory, memory,
                                       GlExternalObjects::dedicated, nullptr),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.objects().get_parameter(SharedObjectKind::native_fence + 1u, memory,
                                       GlExternalObjects::dedicated, &read),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(read, 9);
  CHECK(one.objects().invariants_hold()); }

/* The handle another process sent - one this process never held - is opened
 * through the kernel at the import, and a memory size the application gave in
 * bytes is named as the whole pages it was published as: the wire refuses
 * anything else. */
TEST(gl_external_objects_import, a_handle_another_process_sent_opens_and_its_size_is_whole_pages)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &name), GlExternalObjects::gl_no_error);
  const Answer opened(SharedObjectAnswer::done, host_name);
  one.wire().set_reply(opened.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name, 4097u,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(u64_at(one.wire().last_payload(), 24u), 2u * 4096u);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, name), host_name);
  CHECK(one.objects().invariants_hold()); }

/* A memory object of no bytes names nothing, and is refused before the kernel
 * or the device is asked. */
TEST(gl_external_objects_import, a_memory_object_of_no_bytes_is_an_invalid_value)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &name), GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name, 0u,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.wire().sends(), 0u);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, name), 0u); }

/* One object imported under two names is two GL names a context could release
 * one object through twice: the second is refused, and only the first is
 * open. */
TEST(gl_external_objects_import, one_object_under_a_second_name_is_an_invalid_operation)
{ Process       one;
  std::uint32_t names[2] = {};
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 2, names), GlExternalObjects::gl_no_error);
  void *const  handle = one.handle();
  const Answer opened(SharedObjectAnswer::done, host_name);
  one.wire().set_reply(opened.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, names[0], object_bytes,
                                GlExternalObjects::opaque_win32, handle),
           GlExternalObjects::gl_no_error);
  const std::uint32_t sends = one.wire().sends();
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, names[1], object_bytes,
                                GlExternalObjects::opaque_win32, handle),
           GlExternalObjects::gl_invalid_operation);
  CHECK_EQ(one.wire().sends(), sends);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, names[1]), 0u);
  CHECK_EQ(one.log().holding("avgl: an object this context already imported"), 1u);
  CHECK(one.objects().invariants_hold()); }
