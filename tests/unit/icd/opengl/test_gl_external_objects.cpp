#include "core/support/status.hpp"
#include "core/log/keyed_log.hpp"
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

namespace av {

struct KeyedSlotsProbe {
    template <typename Key, typename Entry>
    static void corrupt_count(KeyedSlots<Key, Entry> &table, std::uint32_t count) { table.count_ = count; }
};

}

namespace av::icd {

struct OpenedObjectsProbe {
    static constexpr std::uint64_t stray = 999u;
    static void add_empty(OpenedObjects &opened)
    {
        std::uint32_t slot = 0u;
        CHECK_STATUS(opened.opens_[0].insert(stray, OpenedObjectRow{}, slot), Status::ok);
    }
    static void drop_empty(OpenedObjects &opened) { static_cast<void>(opened.opens_[0].erase(stray)); }
};

}

namespace av::glicd {

struct GlExternalObjectsProbe {
    static GlSharedObjectRow &row(GlExternalObjects &objects, std::uint32_t name)
    { return *objects.rows_.find(name); }
    static void plant(GlExternalObjects &objects, std::uint32_t name)
    {
        std::uint32_t slot = 0u;
        CHECK_STATUS(objects.rows_.insert(name, GlSharedObjectRow{}, slot), Status::ok);
    }
    static void pull(GlExternalObjects &objects, std::uint32_t name) { static_cast<void>(objects.rows_.erase(name)); }
    static icd::OpenedObjects &opened(GlExternalObjects &objects) { return objects.opened_; }
    static void corrupt_rows(GlExternalObjects &objects, std::uint32_t count) { KeyedSlotsProbe::corrupt_count(objects.rows_, count); }
};

}

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
    Process() : memory_(), trace_(), lock_(trace_, 'S'), kmt_(), log_(), lines_(memory_, log_, "avgl: "), device_(0u),
                handles_(kmt_, lock_, memory_), objects_(handles_, memory_, lines_)
    {
        kmt_.add_adapter(av::PciIdentity(0x1AF4u, 0x1050u, 0x1AF4u, 0x5256u), 3200u);
        std::uint32_t adapter = 0u;
        std::uint32_t count = 0u;
        CHECK_STATUS(kmt_.enumerate_adapters(&adapter, 1u, count), Status::ok);
        CHECK_STATUS(kmt_.create_device(adapter, device_), Status::ok);
    }

    GlExternalObjects &objects() { return objects_; }
    av::MockPlatform  &memory() { return memory_; }
    MockTransport     &wire() { return wire_; }
    av::RecordingLog  &log() { return log_; }
    std::uint32_t      device() const { return device_; }

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
    av::MockPlatform         memory_;
    av::LockTrace            trace_;
    av::RecordingThreadLock  lock_;
    MockKmt                  kmt_;
    av::RecordingLog         log_;
    av::KeyedLog             lines_;
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

}

TEST(gl_external_objects, a_new_table_names_nothing)
{ Process one;
  CHECK_EQ(one.objects().live(), 0u);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, 1u), 0u);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, 0u), 0u);
  CHECK_EQ(one.objects().made(0u, 1u), 0u);
  CHECK(one.objects().invariants_hold()); }

TEST(gl_external_objects_create, hands_out_names_of_its_own_and_sends_nothing)
{ Process       one;
  std::uint32_t names[3] = {};
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 3, names), GlExternalObjects::gl_no_error);
  CHECK_EQ(one.wire().sends(), 0u);
  CHECK(names[0] != 0u);
  CHECK(names[1] != names[0]);
  CHECK(names[2] != names[1]);
  CHECK_EQ(one.objects().live(), 3u);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, names[0]), 0u);
  CHECK(one.objects().invariants_hold()); }

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
  CHECK(u64_at(one.wire().last_payload(), 0u) != 0u);
  CHECK_EQ(u64_at(one.wire().last_payload(), 16u), 0u);
  CHECK_EQ(u64_at(one.wire().last_payload(), 24u), object_bytes);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, name), host_name);
  CHECK(one.objects().invariants_hold());

  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, name), host_name); }

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
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name, object_bytes, 0x9586u, handle),
           GlExternalObjects::gl_invalid_operation);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name + 0x1000u, object_bytes,
                                GlExternalObjects::opaque_win32, handle),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::monitored_fence, name, object_bytes,
                                GlExternalObjects::opaque_win32, handle),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::native_fence + 1u, name, object_bytes,
                                GlExternalObjects::opaque_win32, handle),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.wire().sends(), 0u); }

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
  const std::uint32_t quiet = one.wire().sends();
  CHECK_EQ(one.objects().destroy(one.wire(), SharedObjectKind::memory, 1, &name),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(one.wire().sends(), quiet);
  CHECK(one.objects().invariants_hold()); }

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
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 0, nullptr), GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().destroy(one.wire(), SharedObjectKind::memory, 0, nullptr),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().live(), 0u); }

TEST(gl_external_objects_create, writes_no_names_at_all_when_one_row_cannot_be_had)
{ Process       one;
  std::uint32_t names[63] = {};
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 63, names), GlExternalObjects::gl_no_error);
  one.memory().fail_allocation_at(1u);
  std::uint32_t two[2] = {0xAAAAu, 0xBBBBu};
  CHECK_EQ(one.objects().create(SharedObjectKind::monitored_fence, 2, two), GlExternalObjects::gl_out_of_memory);
  CHECK_EQ(two[0], 0xAAAAu);
  CHECK_EQ(two[1], 0xBBBBu);
  CHECK_EQ(one.objects().live(), 63u);
  CHECK(!one.objects().holds(SharedObjectKind::monitored_fence, 64u));
  CHECK_EQ(one.log().holding("avgl: no memory for a memory object or semaphore name"), 1u);
  CHECK(one.objects().invariants_hold());
  CHECK_EQ(one.objects().create(SharedObjectKind::monitored_fence, 2, two), GlExternalObjects::gl_no_error);
  CHECK_EQ(two[0], 64u);
  CHECK_EQ(two[1], 65u);
  CHECK_EQ(one.objects().live(), 65u);
  CHECK(one.objects().invariants_hold()); }

TEST(gl_external_objects_create, a_name_in_the_run_that_is_taken_gives_the_made_ones_back)
{ Process       one;
  GlExternalObjectsProbe::plant(one.objects(), 2u);
  std::uint32_t names[2] = {0xAAAAu, 0xBBBBu};
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 2, names), GlExternalObjects::gl_out_of_memory);
  CHECK_EQ(names[0], 0xAAAAu);
  CHECK_EQ(names[1], 0xBBBBu);
  CHECK_EQ(one.objects().live(), 1u);
  CHECK(!one.objects().holds(SharedObjectKind::memory, 1u));
  CHECK_EQ(one.log().holding("avgl: every memory object and semaphore name is taken"), 1u); }

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
  GlSharedObjectRow &second = GlExternalObjectsProbe::row(one.objects(), names[1]);
  const GlSharedObjectRow whole = second;

  GlExternalObjectsProbe::plant(one.objects(), 0u);
  CHECK(!one.objects().invariants_hold());
  GlExternalObjectsProbe::pull(one.objects(), 0u);
  CHECK(one.objects().invariants_hold());
  second.made_ = host_name;
  CHECK(!one.objects().invariants_hold());
  second = whole;
  const Answer opened(SharedObjectAnswer::done, host_name);
  one.wire().set_reply(opened.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, names[1], object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_no_error);
  CHECK(one.objects().invariants_hold());
  second.memory_ = false;
  CHECK(!one.objects().invariants_hold());
  second.memory_ = true;
  second.made_ = 0u;
  CHECK(!one.objects().invariants_hold());
  second.made_ = host_name;
  CHECK(one.objects().invariants_hold());
  av::icd::OpenedObjectsProbe::add_empty(GlExternalObjectsProbe::opened(one.objects()));
  CHECK(!one.objects().invariants_hold());
  av::icd::OpenedObjectsProbe::drop_empty(GlExternalObjectsProbe::opened(one.objects()));
  CHECK(one.objects().invariants_hold());
  const std::uint32_t held = one.objects().live();
  GlExternalObjectsProbe::corrupt_rows(one.objects(), 99u);
  CHECK(!one.objects().invariants_hold());
  GlExternalObjectsProbe::corrupt_rows(one.objects(), held);
  CHECK(one.objects().invariants_hold()); }

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
  CHECK_EQ(u64_at(one.wire().last_payload(), 24u), 0u);
  CHECK_EQ(one.objects().made(SharedObjectKind::native_fence, name), host_name);
  CHECK_EQ(one.objects().made(SharedObjectKind::monitored_fence, name), host_name);

  const std::uint32_t unknown = SharedObjectKind::native_fence + 1u;
  std::uint32_t       refused = 0u;
  CHECK_EQ(one.objects().create(unknown, 1, &refused), GlExternalObjects::gl_invalid_value);
  CHECK_EQ(refused, 0u);
  CHECK_EQ(one.objects().destroy(one.wire(), unknown, 1, &name), GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.objects().made(SharedObjectKind::native_fence, name), host_name);
  CHECK(one.objects().invariants_hold());

  const std::uint32_t sends = one.wire().sends();
  CHECK_EQ(one.objects().destroy(one.wire(), SharedObjectKind::native_fence, 1, &name),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(one.wire().sends(), sends + 1u);
  CHECK_EQ(u32_at(one.wire().last_payload(), 8u), SharedObjectKind::monitored_fence);
  CHECK_EQ(one.objects().made(SharedObjectKind::native_fence, name), 0u);
  CHECK(one.objects().invariants_hold()); }

TEST(gl_external_objects, the_zero_name_belongs_to_no_row)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &name), GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, 0u), 0u);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, 0u, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_invalid_value);
  const std::uint32_t zero = 0u;
  CHECK_EQ(one.objects().destroy(one.wire(), SharedObjectKind::memory, 1, &zero),
           GlExternalObjects::gl_no_error);
  CHECK_EQ(one.wire().sends(), 0u);
  CHECK_EQ(one.objects().live(), 1u);
  CHECK(one.objects().invariants_hold()); }

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

TEST(gl_external_objects, a_created_name_is_an_object_before_it_is_imported)
{ Process       one;
  std::uint32_t memory = 0u;
  std::uint32_t semaphore = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &memory), GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().create(SharedObjectKind::monitored_fence, 1, &semaphore),
           GlExternalObjects::gl_no_error);
  CHECK(one.objects().holds(SharedObjectKind::memory, memory));
  CHECK(one.objects().holds(SharedObjectKind::monitored_fence, semaphore));
  CHECK(one.objects().holds(SharedObjectKind::native_fence, semaphore));
  CHECK(!one.objects().holds(SharedObjectKind::monitored_fence, memory));
  CHECK(!one.objects().holds(SharedObjectKind::memory, semaphore));
  CHECK(!one.objects().holds(SharedObjectKind::memory, memory + 0x1000u));
  CHECK(!one.objects().holds(SharedObjectKind::memory, 0u));
  CHECK(!one.objects().holds(SharedObjectKind::native_fence + 1u, memory));
  const Answer made(SharedObjectAnswer::done, host_name);
  one.wire().set_reply(made.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, memory, object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_no_error);
  CHECK(one.objects().holds(SharedObjectKind::memory, memory));
  CHECK_EQ(one.objects().destroy(one.wire(), SharedObjectKind::memory, 1, &memory),
           GlExternalObjects::gl_no_error);
  CHECK(!one.objects().holds(SharedObjectKind::memory, memory));
  CHECK(one.objects().invariants_hold()); }

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

TEST(gl_external_objects, a_parameter_of_the_wrong_kind_name_or_pname_is_refused)
{ Process       one;
  std::uint32_t memory = 0u;
  std::uint32_t semaphore = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &memory), GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().create(SharedObjectKind::monitored_fence, 1, &semaphore),
           GlExternalObjects::gl_no_error);
  std::int32_t read = 9;
  CHECK_EQ(one.objects().set_parameter(SharedObjectKind::memory, memory + 0x1000u,
                                       GlExternalObjects::dedicated, 1),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.objects().get_parameter(SharedObjectKind::memory, memory + 0x1000u,
                                       GlExternalObjects::dedicated, &read),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(read, 9);
  CHECK_EQ(one.objects().set_parameter(SharedObjectKind::memory, memory, 0x1234u, 1),
           GlExternalObjects::gl_invalid_enum);
  CHECK_EQ(one.objects().get_parameter(SharedObjectKind::memory, memory, 0x1234u, &read),
           GlExternalObjects::gl_invalid_enum);
  CHECK_EQ(one.objects().set_parameter(SharedObjectKind::monitored_fence, semaphore,
                                       GlExternalObjects::d3d12_fence_value, 7),
           GlExternalObjects::gl_invalid_enum);
  CHECK_EQ(one.objects().get_parameter(SharedObjectKind::monitored_fence, semaphore,
                                       GlExternalObjects::d3d12_fence_value, &read),
           GlExternalObjects::gl_invalid_enum);
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

TEST(gl_external_objects_import, a_memory_object_of_no_bytes_is_an_invalid_value)
{ Process       one;
  std::uint32_t name = 0u;
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 1, &name), GlExternalObjects::gl_no_error);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, name, 0u,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_invalid_value);
  CHECK_EQ(one.wire().sends(), 0u);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, name), 0u); }

TEST(gl_external_objects_import, one_object_the_device_already_opened_for_the_context_is_an_invalid_operation)
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
  const Answer        conflict(SharedObjectAnswer::conflict, 0u);
  one.wire().set_reply(conflict.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, names[1], object_bytes,
                                GlExternalObjects::opaque_win32, handle),
           GlExternalObjects::gl_invalid_operation);
  CHECK_EQ(one.wire().sends(), sends + 1u);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, names[1]), 0u);
  CHECK_EQ(one.log().holding("avgl: an object this context already opened into the same object"), 1u);
  CHECK(one.objects().invariants_hold()); }

TEST(gl_external_objects_import, a_device_that_answers_a_name_the_context_already_holds_is_refused_and_given_back)
{ Process       one;
  std::uint32_t names[2] = {};
  CHECK_EQ(one.objects().create(SharedObjectKind::memory, 2, names), GlExternalObjects::gl_no_error);
  const Answer opened(SharedObjectAnswer::done, host_name);
  one.wire().set_reply(opened.bytes(), SharedObjectAnswer::encoded_size);
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, names[0], object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_no_error);
  const std::uint32_t sends = one.wire().sends();
  CHECK_EQ(one.objects().import(one.wire(), one.device(), SharedObjectKind::memory, names[1], object_bytes,
                                GlExternalObjects::opaque_win32, one.handle()),
           GlExternalObjects::gl_out_of_memory);
  CHECK_EQ(one.wire().sends(), sends + 2u);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, names[1]), 0u);
  CHECK_EQ(one.objects().made(SharedObjectKind::memory, names[0]), host_name);
  CHECK(one.objects().invariants_hold()); }

