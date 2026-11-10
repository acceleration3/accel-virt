#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "icd/opengl/buffer_mapping.hpp"
#include "icd/opengl/buffer_shadows.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "icd/opengl/mapped_buffers.hpp"

#include "gl_protocol_ids.h"

#include <cstdint>
#include <cstring>

using av::GlContextOptions;
using av::GlContextRig;
using av::Status;
using av::glicd::BufferMapping;
using av::glicd::BufferShadows;
using av::glicd::GuestGlContext;
using av::glicd::MappedBuffers;
namespace ids = av::glproto;

namespace av::glicd {

struct MappedBuffersProbe {
    static void miscount(MappedBuffers &mappings) { mappings.persistent_writers_++; }
    static BufferMapping &mapping(MappedBuffers &mappings, std::uint32_t buffer)
    {
        return *mappings.mappings_.find(buffer, 0u);
    }
};

struct BufferMappingProbe {
    static void set_dirty(BufferMapping &mapping, std::uint64_t from, std::uint64_t to)
    {
        mapping.dirty_from_ = from;
        mapping.dirty_to_ = to;
    }
};

}

namespace {

constexpr std::uint32_t no_error = MappedBuffers::gl_no_error;
constexpr std::uint32_t invalid_value = MappedBuffers::gl_invalid_value;
constexpr std::uint32_t invalid_operation = MappedBuffers::gl_invalid_operation;
constexpr std::uint32_t out_of_memory = MappedBuffers::gl_out_of_memory;
constexpr std::uint32_t read = BufferMapping::map_read, write = BufferMapping::map_write;
constexpr std::uint32_t persistent = BufferMapping::map_persistent, explicit_flush = BufferMapping::map_flush_explicit;

struct Rig {
    GlContextRig   context_rig;
    GuestGlContext context{context_rig.connection(), context_rig.platform(), context_rig.send_lock(),
                           context_rig.groups(), context_rig.lines(), context_rig.extensions(), context_rig.handles(), 1,
                           GlContextOptions()};
    BufferShadows  shadows{context_rig.platform()};
    MappedBuffers  mappings{context_rig.platform(), shadows, context.slices(), context_rig.lines()};

    Rig()
    {
        context_rig.host().gl_made("renderer", "4.6");
        CHECK_STATUS(context.make(), Status::ok);
    }
    av::ScriptedHost    &host() { return context_rig.host(); }
    av::ClientTransport &wire() { return *context.wire(); }
    std::uint32_t        commands()
    {
        context.flush();
        return context_rig.kmt().commands();
    }
    std::uint32_t        id(std::uint32_t index) { return context_rig.kmt().command_id(index); }
    void                 size(std::int64_t bytes)
    {
        host().gl_bytes(reinterpret_cast<const std::uint8_t *>(&bytes), sizeof bytes);
    }
    void *map(std::uint32_t buffer, std::int64_t bytes, std::int64_t offset, std::int64_t length,
              std::uint32_t access, std::uint32_t expected)
    {
        std::uint64_t known = 0u;
        if (!shadows.size_of(buffer, known)) size(bytes);
        const std::uint64_t mapped = static_cast<std::uint64_t>(length == MappedBuffers::whole_buffer ? bytes - offset
                                                                                                        : length);
        const bool keeps = (access & (explicit_flush | BufferMapping::map_invalidate_range |
                                      BufferMapping::map_invalidate_buffer)) == 0u &&
                           !shadows.fresh(buffer) &&
                           shadows.covering(buffer, static_cast<std::uint64_t>(offset), mapped) == nullptr;
        if (expected == no_error && ((access & read) != 0u || keeps)) host().gl_bytes(nullptr, 0u);
        void *pointer = reinterpret_cast<void *>(0x1u);
        CHECK_EQ(mappings.map(wire(), buffer, offset, length, access, pointer), expected);
        CHECK(mappings.invariants_hold());
        return pointer;
    }
};

}

TEST(mapped_buffers, access_bits_and_arguments_are_judged_first)
{ Rig rig;
  constexpr struct {
      std::uint32_t buffer;
      std::int64_t  offset;
      std::int64_t  length;
      std::uint32_t access;
      std::uint32_t error;
  } refusals[] = {
      {1u, 0, 4, 0x0080u | read, invalid_value},
      {1u, 0, 4, 0x8000'0000u | write, invalid_value},
      {1u, 0, 4, 0u, invalid_operation},
      {1u, 0, 4, persistent, invalid_operation},
      {1u, 0, 4, read | BufferMapping::map_invalidate_range, invalid_operation},
      {1u, 0, 4, read | write | BufferMapping::map_invalidate_buffer, invalid_operation},
      {1u, 0, 4, read | BufferMapping::map_unsynchronized, invalid_operation},
      {1u, 0, 4, read | explicit_flush, invalid_operation},
      {1u, -1, 4, read, invalid_value},
      {1u, 0, 0, read, invalid_operation},
      {1u, 0, -2, write, invalid_value},
      {0u, 0, 4, read, invalid_operation},
  };
  const std::uint32_t before = rig.commands();
  for (const auto &row : refusals) {
    void *pointer = reinterpret_cast<void *>(0x1u);
    CHECK_EQ(rig.mappings.map(rig.wire(), row.buffer, row.offset, row.length, row.access, pointer), row.error);
    CHECK(pointer == nullptr);
  }
  CHECK_EQ(rig.commands(), before);
  CHECK_EQ(rig.mappings.held(), 0u);
  CHECK_EQ(rig.context_rig.log().lines(), 0u); }

TEST(mapped_buffers, a_range_past_the_buffer_or_a_buffer_the_host_does_not_size_is_refused)
{ Rig rig;
  std::uint32_t before = rig.commands();
  rig.host().gl_bytes(nullptr, 0u);
  void *pointer = nullptr;
  CHECK_EQ(rig.mappings.map(rig.wire(), 3u, 0, 4, read, pointer), invalid_operation);
  CHECK_EQ(rig.commands() - before, 1u);
  CHECK_EQ(rig.id(before), ids::kCmd_glGetNamedBufferParameteri64v);
  CHECK(rig.map(3u, -5, 0, 1, write, invalid_operation) == nullptr);
  CHECK(rig.map(10u, 16, 8, 9, write, invalid_value) == nullptr);
  CHECK(rig.map(11u, 16, 17, MappedBuffers::whole_buffer, write, invalid_value) == nullptr);
  CHECK(rig.map(12u, 16, 16, 1, write, invalid_value) == nullptr);
  CHECK(rig.map(13u, 0, 0, MappedBuffers::whole_buffer, write, invalid_value) == nullptr);
  const std::int64_t huge = static_cast<std::int64_t>(BufferMapping::max_length) + 1;
  CHECK(rig.map(14u, huge, 0, huge, write, out_of_memory) == nullptr);
  CHECK(rig.map(15u, INT64_MAX, 1, INT64_MAX, write, invalid_value) == nullptr);
  rig.context_rig.platform().fail_allocation_at(1u);
  CHECK(rig.map(16u, 16, 0, 4, write, out_of_memory) == nullptr);
  CHECK_EQ(rig.mappings.held(), 0u);
  before = rig.commands();
  CHECK(rig.map(3u, 16, 0, MappedBuffers::whole_buffer, write, no_error) != nullptr);
  CHECK_EQ(av::glicd::MappedBuffersProbe::mapping(rig.mappings, 3u).bytes(), 16u);
  CHECK(rig.map(4u, 16, 12, 4, write, no_error) != nullptr);
  CHECK_EQ(rig.commands() - before, 4u);
  CHECK_EQ(rig.mappings.held(), 2u);
  CHECK_EQ(rig.mappings.unmap(rig.wire(), 4u), no_error);
  before = rig.commands();
  CHECK(rig.map(4u, 16, 0, 17, write, invalid_value) == nullptr);
  CHECK_EQ(rig.commands(), before);
  CHECK(rig.map(4u, 16, 0, 4, write, no_error) != nullptr);
  CHECK_EQ(rig.commands() - before, 1u);
  CHECK_EQ(rig.id(before), ids::kCmd_glGetNamedBufferSubData); }

TEST(mapped_buffers, a_buffer_is_mapped_once_and_maps_go_past_the_old_bound)
{ Rig rig;
  constexpr std::uint32_t many = 128u;
  for (std::uint32_t buffer = 1u; buffer <= many; buffer++)
    CHECK(rig.map(buffer, 4, 0, 4, write, no_error) != nullptr);
  CHECK_EQ(rig.mappings.held(), many);
  for (std::uint32_t buffer = 1u; buffer <= many; buffer++) CHECK(rig.mappings.pointer_of(buffer) != nullptr);
  CHECK(rig.mappings.pointer_of(1u) != rig.mappings.pointer_of(2u));
  void *pointer = reinterpret_cast<void *>(0x1u);
  const std::uint32_t before = rig.commands();
  CHECK_EQ(rig.mappings.map(rig.wire(), 5u, 0, 4, write, pointer), invalid_operation);
  CHECK(pointer == nullptr);
  CHECK_EQ(rig.commands(), before);
  CHECK_EQ(rig.context_rig.log().holding("buffer maps are guest copies"), 1u);
  CHECK_EQ(rig.context_rig.log().lines(), 1u);
  CHECK(rig.mappings.invariants_hold());
  CHECK_EQ(rig.mappings.unmap(rig.wire(), 9u), no_error);
  CHECK_EQ(rig.mappings.held(), many - 1u); }

TEST(mapped_buffers, a_map_with_no_memory_to_hold_it_is_out_of_memory)
{ Rig rig;
  rig.context_rig.platform().fail_allocation_at(1u);
  CHECK(rig.map(1u, 4, 0, 4, write, out_of_memory) == nullptr);
  CHECK_EQ(rig.mappings.held(), 0u);
  CHECK(rig.mappings.invariants_hold());
  CHECK(rig.map(1u, 4, 0, 4, write, no_error) != nullptr);
  CHECK(rig.mappings.invariants_hold()); }

TEST(mapped_buffers, an_unmap_writes_back_what_was_mapped_and_gives_it_up)
{ Rig rig;
  auto *const bytes = static_cast<std::uint8_t *>(rig.map(6u, 8, 2, 4, write, no_error));
  CHECK(rig.mappings.pointer_of(6u) == bytes);
  CHECK(rig.mappings.pointer_of(0u) == nullptr);
  CHECK(rig.mappings.pointer_of(5u) == nullptr);
  bytes[3] = 0x44u;
  std::uint32_t before = rig.commands();
  CHECK_EQ(rig.mappings.unmap(rig.wire(), 5u), invalid_operation);
  CHECK_EQ(rig.mappings.unmap(rig.wire(), 0u), invalid_operation);
  CHECK_EQ(rig.commands(), before);
  CHECK_EQ(rig.mappings.unmap(rig.wire(), 6u), no_error);
  CHECK_EQ(rig.commands() - before, 1u);
  CHECK_EQ(rig.id(before), ids::kCmd_glNamedBufferSubData);
  CHECK(rig.mappings.pointer_of(6u) == nullptr);
  CHECK_EQ(rig.mappings.held(), 0u);
  CHECK_EQ(rig.mappings.unmap(rig.wire(), 6u), invalid_operation);
  CHECK(rig.map(6u, 8, 0, 8, read, no_error) != nullptr);
  before = rig.commands();
  CHECK_EQ(rig.mappings.unmap(rig.wire(), 6u), no_error);
  CHECK_EQ(rig.commands(), before);
  CHECK(rig.mappings.invariants_hold()); }

TEST(mapped_buffers, an_explicit_flush_is_judged_and_marks_the_range)
{ Rig rig;
  CHECK(rig.map(2u, 16, 0, 16, write | explicit_flush, no_error) != nullptr);
  CHECK(rig.map(3u, 16, 0, 16, write, no_error) != nullptr);
  constexpr struct {
      std::uint32_t buffer;
      std::int64_t  offset;
      std::int64_t  length;
      std::uint32_t error;
  } cases[] = {
      {2u, -1, 1, invalid_value}, {2u, 0, -1, invalid_value},  {9u, -1, 1, invalid_value},
      {9u, 0, 1, invalid_operation}, {0u, 0, 1, invalid_operation}, {3u, 0, 1, invalid_operation},
      {2u, 8, 9, invalid_value},  {2u, 17, 0, invalid_value},  {2u, 4, 2, no_error},
  };
  for (const auto &row : cases) CHECK_EQ(rig.mappings.mark(row.buffer, row.offset, row.length), row.error);
  const std::uint32_t before = rig.commands();
  CHECK_EQ(rig.mappings.unmap(rig.wire(), 2u), no_error);
  CHECK_EQ(rig.commands() - before, 1u);
  CHECK(rig.mappings.invariants_hold()); }

TEST(mapped_buffers, only_persistent_writers_are_flushed)
{ Rig rig;
  const std::uint32_t before = rig.commands();
  rig.mappings.flush_persistent(rig.wire());
  CHECK_EQ(rig.commands(), before);
  CHECK(rig.map(1u, 4, 0, 4, write, no_error) != nullptr);
  CHECK(rig.map(2u, 4, 0, 4, read | persistent, no_error) != nullptr);
  auto *const three = static_cast<std::uint8_t *>(rig.map(3u, 4, 0, 4, write | persistent, no_error));
  CHECK(three != nullptr);
  CHECK(rig.map(4u, 4, 0, 4, write | persistent | explicit_flush, no_error) != nullptr);
  const std::uint32_t flushed = rig.commands();
  rig.mappings.flush_persistent(rig.wire());
  CHECK_EQ(rig.commands(), flushed);
  three[0] = 0x31u;
  rig.mappings.flush_persistent(rig.wire());
  CHECK_EQ(rig.commands() - flushed, 1u);
  CHECK_EQ(rig.id(flushed), ids::kCmd_glNamedBufferSubData);
  CHECK_EQ(rig.mappings.mark(4u, 0, 1), no_error);
  rig.mappings.flush_persistent(rig.wire());
  CHECK_EQ(rig.commands() - flushed, 2u);
  three[1] = 0x32u;
  rig.mappings.flush_persistent(rig.wire());
  CHECK_EQ(rig.commands() - flushed, 3u);
  char            bytes[512];
  av::StateWriter writer(bytes, sizeof bytes);
  rig.mappings.report(writer);
  CHECK(std::strstr(writer.dump(), "mappings/0 held=4 persistent_writers=2\n") == writer.dump());
  CHECK(std::strstr(writer.dump(),
                    "mappings/0/mapping/2 buffer=3 offset=0 length=4 access=0x0000000000000042 dirty_from=0 "
                    "dirty_to=0") != nullptr);
  CHECK(rig.mappings.invariants_hold()); }

TEST(mapped_buffers, forget_and_release_give_up_ranges_without_writing)
{ Rig rig;
  CHECK(rig.map(1u, 4, 0, 4, write | persistent, no_error) != nullptr);
  CHECK(rig.map(2u, 4, 0, 4, write, no_error) != nullptr);
  auto *const three = static_cast<std::uint8_t *>(rig.map(3u, 4, 0, 4, write | persistent, no_error));
  CHECK(three != nullptr);
  const std::uint32_t before = rig.commands();
  rig.mappings.forget(1, nullptr);
  const std::uint32_t deleted[] = {0u, 9u, 1u, 2u};
  rig.mappings.forget(0, deleted);
  CHECK_EQ(rig.mappings.held(), 3u);
  rig.mappings.forget(4, deleted);
  CHECK_EQ(rig.mappings.held(), 1u);
  CHECK(rig.mappings.pointer_of(3u) != nullptr);
  CHECK(rig.mappings.invariants_hold());
  three[2] = 0x33u;
  rig.mappings.flush_persistent(rig.wire());
  CHECK_EQ(rig.commands() - before, 1u);
  rig.mappings.release();
  CHECK_EQ(rig.mappings.held(), 0u);
  CHECK(rig.mappings.invariants_hold());
  rig.mappings.flush_persistent(rig.wire());
  CHECK_EQ(rig.commands() - before, 1u); }

TEST(mapped_buffers_invariants, fail_for_a_miscount_or_a_broken_mapping)
{ Rig counted;
  CHECK(counted.map(1u, 4, 0, 4, write, no_error) != nullptr);
  av::glicd::MappedBuffersProbe::miscount(counted.mappings);
  CHECK(!counted.mappings.invariants_hold());
  Rig broken;
  CHECK(broken.map(1u, 4, 0, 4, write, no_error) != nullptr);
  av::glicd::BufferMappingProbe::set_dirty(av::glicd::MappedBuffersProbe::mapping(broken.mappings, 1u), 0u, 1u);
  CHECK(!broken.mappings.invariants_hold());
  Rig empty;
  CHECK(empty.mappings.invariants_hold()); }

TEST(mapped_buffers, a_map_that_writes_one_block_keeps_the_blocks_earlier_maps_wrote)
{ Rig                     rig;
  constexpr std::uint32_t unsynchronized = write | BufferMapping::map_unsynchronized;
  constexpr std::int64_t  size = 2496, block = 156;
  rig.shadows.respecify(5u, size, false);
  std::uint32_t before = rig.commands();
  auto *bytes = static_cast<std::uint8_t *>(rig.map(5u, size, 0, MappedBuffers::whole_buffer, unsynchronized,
                                                    no_error));
  CHECK_EQ(rig.commands(), before);
  for (std::int64_t at = 0; at < block; at++) bytes[at] = 0xA1u;
  CHECK_EQ(rig.mappings.unmap(rig.wire(), 5u), no_error);
  for (std::uint32_t index = 1u; index < 3u; index++) {
    before = rig.commands();
    bytes = static_cast<std::uint8_t *>(rig.map(5u, size, 0, MappedBuffers::whole_buffer, unsynchronized,
                                                no_error));
    CHECK_EQ(rig.commands(), before);
    for (std::uint32_t earlier = 0u; earlier < index; earlier++)
      CHECK_EQ(bytes[earlier * block + block - 1], 0xA1u + earlier);
    for (std::int64_t at = 0; at < block; at++) bytes[index * block + at] = static_cast<std::uint8_t>(0xA1u + index);
    before = rig.commands();
    CHECK_EQ(rig.mappings.unmap(rig.wire(), 5u), no_error);
    CHECK_EQ(rig.commands() - before, 1u);
    CHECK_EQ(rig.id(before), ids::kCmd_glNamedBufferSubData);
  }
  CHECK(rig.mappings.invariants_hold());
  CHECK(rig.shadows.invariants_hold()); }

TEST(mapped_buffers, map_state_answers_from_the_guests_map)
{ Rig rig;
  constexpr std::uint32_t mapped_name = MappedBuffers::gl_buffer_mapped, flags = MappedBuffers::gl_buffer_access_flags,
                          offset = MappedBuffers::gl_buffer_map_offset, length = MappedBuffers::gl_buffer_map_length,
                          access = MappedBuffers::gl_buffer_access;
  std::int64_t value = -1;
  CHECK(rig.mappings.map_state(6u, mapped_name, value));
  CHECK_EQ(value, 0);
  CHECK(rig.mappings.map_state(6u, flags, value));
  CHECK_EQ(value, 0);
  CHECK(rig.mappings.map_state(6u, length, value));
  CHECK_EQ(value, 0);
  CHECK(rig.mappings.map_state(0u, access, value));
  CHECK_EQ(value, MappedBuffers::gl_read_write);

  CHECK(rig.map(6u, 64, 16, 24, write | explicit_flush, no_error) != nullptr);
  CHECK(rig.mappings.map_state(6u, mapped_name, value));
  CHECK_EQ(value, 1);
  CHECK(rig.mappings.map_state(6u, flags, value));
  CHECK_EQ(value, static_cast<std::int64_t>(write | explicit_flush));
  CHECK(rig.mappings.map_state(6u, offset, value));
  CHECK_EQ(value, 16);
  CHECK(rig.mappings.map_state(6u, length, value));
  CHECK_EQ(value, 24);
  CHECK(rig.mappings.map_state(6u, access, value));
  CHECK_EQ(value, MappedBuffers::gl_write_only);
  CHECK(rig.mappings.map_state(7u, mapped_name, value));
  CHECK_EQ(value, 0);

  CHECK(rig.map(7u, 8, 0, 8, read, no_error) != nullptr);
  CHECK(rig.mappings.map_state(7u, access, value));
  CHECK_EQ(value, MappedBuffers::gl_read_only);
  CHECK(rig.map(8u, 8, 0, 8, read | write, no_error) != nullptr);
  CHECK(rig.mappings.map_state(8u, access, value));
  CHECK_EQ(value, MappedBuffers::gl_read_write);

  value = 42;
  CHECK(!rig.mappings.map_state(6u, MappedBuffers::gl_buffer_size, value));
  CHECK_EQ(value, 42);
  CHECK_EQ(rig.mappings.unmap(rig.wire(), 6u), no_error);
  CHECK(rig.mappings.map_state(6u, mapped_name, value));
  CHECK_EQ(value, 0); }

namespace {

constexpr std::uint32_t sliced_storage = persistent | write | read;

struct SlicedRig {
    GlContextRig   context_rig;
    GuestGlContext context{context_rig.connection(), context_rig.platform(), context_rig.send_lock(),
                           context_rig.groups(), context_rig.lines(), context_rig.extensions(), context_rig.handles(), 1,
                           GlContextOptions()};
    BufferShadows  shadows{context_rig.platform()};
    MappedBuffers  mappings{context_rig.platform(), shadows, context.slices(), context_rig.lines()};

    SlicedRig()
    {
        context_rig.host().gl_make("renderer", "4.6");
        context_rig.host().gl_slices(1u);
        CHECK_STATUS(context.make(), Status::ok);
        context_rig.host().blob_import(0u, 0x0000700000000100ull);
        context_rig.host().gl_slices(1u);
        CHECK_STATUS(context.slices().place(*context.wire(), 1u, 4096u, sliced_storage, nullptr), Status::ok);
    }
    av::ClientTransport &wire() { return *context.wire(); }
};

}

TEST(mapped_buffers, a_buffer_the_gpu_shares_with_the_guest_is_mapped_marked_and_unmapped_through_its_slice)
{ SlicedRig rig;
  void *pointer = nullptr;
  CHECK_EQ(rig.mappings.map(rig.wire(), 1u, 0, 128, write | persistent | explicit_flush, pointer), no_error);
  CHECK(pointer != nullptr);
  CHECK(rig.mappings.pointer_of(1u) == pointer);
  CHECK_EQ(rig.mappings.held(), 0u);
  CHECK_EQ(rig.mappings.mark(1u, 0, 16), no_error);
  CHECK_EQ(rig.mappings.mark(1u, 120, 16), invalid_value);
  CHECK_EQ(rig.mappings.unmap(rig.wire(), 1u), no_error);
  CHECK(rig.mappings.invariants_hold()); }

TEST(mapped_buffers, map_state_answers_a_sliced_buffers_storage_flags_and_its_map)
{ SlicedRig rig;
  constexpr std::uint32_t storage_flags = MappedBuffers::gl_buffer_storage_flags,
                          mapped_name = MappedBuffers::gl_buffer_mapped, offset = MappedBuffers::gl_buffer_map_offset,
                          length = MappedBuffers::gl_buffer_map_length;
  std::int64_t value = -1;
  CHECK(rig.mappings.map_state(1u, storage_flags, value));
  CHECK_EQ(value, static_cast<std::int64_t>(sliced_storage));
  value = -1;
  CHECK(rig.mappings.map_state(0u, storage_flags, value));
  CHECK_EQ(value, -1);
  value = -1;
  CHECK(!rig.mappings.map_state(9u, storage_flags, value));
  CHECK_EQ(value, -1);
  CHECK(rig.mappings.map_state(1u, mapped_name, value));
  CHECK_EQ(value, 0);
  void *pointer = nullptr;
  CHECK_EQ(rig.mappings.map(rig.wire(), 1u, 16, 64, write | persistent, pointer), no_error);
  CHECK(rig.mappings.map_state(1u, mapped_name, value));
  CHECK_EQ(value, 1);
  CHECK(rig.mappings.map_state(1u, offset, value));
  CHECK_EQ(value, 16);
  CHECK(rig.mappings.map_state(1u, length, value));
  CHECK_EQ(value, 64); }
