/* One context's mapped ranges (data-model.md): the checks OpenGL makes of a
 * map, an unmap and an explicit flush - access bits, ranges against the size
 * the host answers, a buffer mapped twice, the 65th map, no memory - what goes
 * back to the host and when, and the one line a context logs. */
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "icd/opengl/buffer_mapping.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "icd/opengl/mapped_buffers.hpp"

#include "gl_protocol_ids.h"

#include <cstdint>
#include <cstring>

using av::GlContextOptions;
using av::GlContextRig;
using av::Status;
using av::glicd::BufferMapping;
using av::glicd::GuestGlContext;
using av::glicd::MappedBuffers;
namespace ids = av::glproto;

namespace av::glicd {

struct MappedBuffersProbe {
    static void miscount(MappedBuffers &mappings) { mappings.persistent_writers_++; }
    static BufferMapping &mapping(MappedBuffers &mappings, std::uint32_t slot)
    {
        return mappings.mappings_[slot].object();
    }
    /* A slot holding a mapping of nothing. */
    static void empty_slot(MappedBuffers &mappings, BlockSource &memory)
    {
        static_cast<void>(mappings.mappings_[5].construct(memory));
    }
    /* A second slot claiming the first slot's buffer. */
    static void duplicate(MappedBuffers &mappings, ClientTransport &wire, BlockSource &memory)
    {
        static_cast<void>(mappings.mappings_[1].construct(memory));
        static_cast<void>(mappings.mappings_[1].object().map(wire, mappings.mappings_[0].object().buffer(), 0u, 1u,
                                                               BufferMapping::map_write));
    }
};

struct BufferMappingProbe {
    static void set_dirty(BufferMapping &mapping, std::uint64_t from, std::uint64_t to)
    {
        mapping.dirty_from_ = from;
        mapping.dirty_to_ = to;
    }
};

}  // namespace av::glicd

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
                           context_rig.log(), context_rig.extensions(), 1, GlContextOptions()};
    MappedBuffers  mappings{context_rig.platform(), context_rig.log()};

    Rig()
    {
        context_rig.host().gl_make("renderer", "4.6");
        CHECK_STATUS(context.make(), Status::ok);
    }
    av::ScriptedHost    &host() { return context_rig.host(); }
    av::ClientTransport &wire() { return *context.wire(); }
    std::uint32_t        commands() { return context_rig.kmt().commands(); }
    std::uint32_t        id(std::uint32_t index) { return context_rig.kmt().command_id(index); }
    void                 size(std::int64_t bytes)
    {
        host().gl_bytes(reinterpret_cast<const std::uint8_t *>(&bytes), sizeof bytes);
    }
    /* A map of a buffer the host says has `bytes`; the pointer, or null. */
    void *map(std::uint32_t buffer, std::int64_t bytes, std::int64_t offset, std::int64_t length,
              std::uint32_t access, std::uint32_t expected)
    {
        size(bytes);
        /* A read map's fill, answered empty. */
        if ((access & read) != 0u && expected == no_error) host().gl_bytes(nullptr, 0u);
        void *pointer = reinterpret_cast<void *>(0x1u);
        CHECK_EQ(mappings.map(wire(), buffer, offset, length, access, pointer), expected);
        CHECK(mappings.invariants_hold());
        return pointer;
    }
};

}  // namespace

/* Refused before the host is asked anything. */
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
      {1u, 0, 0, read, invalid_value},
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

/* The range is judged against the size the host answers, asked every map. */
TEST(mapped_buffers, a_range_past_the_buffer_or_a_buffer_the_host_does_not_size_is_refused)
{ Rig rig;
  std::uint32_t before = rig.commands();
  rig.host().gl_bytes(nullptr, 0u);
  void *pointer = nullptr;
  CHECK_EQ(rig.mappings.map(rig.wire(), 3u, 0, 4, read, pointer), invalid_operation);
  CHECK_EQ(rig.commands() - before, 1u);
  CHECK_EQ(rig.id(before), ids::kCmd_glGetNamedBufferParameteri64v);
  CHECK(rig.map(3u, -5, 0, 1, write, invalid_operation) == nullptr);
  CHECK(rig.map(3u, 16, 8, 9, write, invalid_value) == nullptr);
  CHECK(rig.map(3u, 16, 17, MappedBuffers::whole_buffer, write, invalid_value) == nullptr);
  CHECK(rig.map(3u, 16, 16, 1, write, invalid_value) == nullptr);
  CHECK(rig.map(3u, 0, 0, MappedBuffers::whole_buffer, write, invalid_value) == nullptr);
  const std::int64_t huge = static_cast<std::int64_t>(BufferMapping::max_length) + 1;
  CHECK(rig.map(3u, huge, 0, huge, write, out_of_memory) == nullptr);
  CHECK(rig.map(3u, INT64_MAX, 1, INT64_MAX, write, invalid_value) == nullptr);
  rig.context_rig.platform().fail_allocation_at(1u);
  CHECK(rig.map(3u, 16, 0, 4, write, out_of_memory) == nullptr);
  CHECK_EQ(rig.mappings.held(), 0u);
  /* The whole buffer, and a range ending at its end. */
  before = rig.commands();
  CHECK(rig.map(3u, 16, 0, MappedBuffers::whole_buffer, write, no_error) != nullptr);
  CHECK_EQ(av::glicd::MappedBuffersProbe::mapping(rig.mappings, 0u).length(), 16u);
  CHECK(rig.map(4u, 16, 12, 4, write, no_error) != nullptr);
  CHECK_EQ(rig.commands() - before, 2u);
  CHECK_EQ(rig.mappings.held(), 2u); }

TEST(mapped_buffers, a_buffer_is_mapped_once_and_the_65th_map_is_out_of_memory)
{ Rig rig;
  for (std::uint32_t buffer = 1u; buffer <= MappedBuffers::max_mappings; buffer++)
    CHECK(rig.map(buffer, 4, 0, 4, write, no_error) != nullptr);
  CHECK_EQ(rig.mappings.held(), MappedBuffers::max_mappings);
  void *pointer = reinterpret_cast<void *>(0x1u);
  const std::uint32_t before = rig.commands();
  CHECK_EQ(rig.mappings.map(rig.wire(), 5u, 0, 4, write, pointer), invalid_operation);
  CHECK(pointer == nullptr);
  CHECK_EQ(rig.commands(), before);
  CHECK(rig.map(65u, 4, 0, 4, write, out_of_memory) == nullptr);
  CHECK_EQ(rig.context_rig.log().holding("buffer maps are guest copies"), 1u);
  CHECK_EQ(rig.context_rig.log().lines(), 1u);
  /* An unmap makes room. */
  CHECK_EQ(rig.mappings.unmap(rig.wire(), 9u), no_error);
  CHECK(rig.map(65u, 4, 0, 4, write, no_error) != nullptr);
  CHECK_EQ(rig.mappings.held(), MappedBuffers::max_mappings); }

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
  /* A read map sends nothing back. */
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
  CHECK(rig.map(3u, 4, 0, 4, write | persistent, no_error) != nullptr);
  CHECK(rig.map(4u, 4, 0, 4, write | persistent | explicit_flush, no_error) != nullptr);
  const std::uint32_t flushed = rig.commands();
  rig.mappings.flush_persistent(rig.wire());
  /* Buffer 3's whole range; buffer 4 marked nothing. */
  CHECK_EQ(rig.commands() - flushed, 1u);
  CHECK_EQ(rig.id(flushed), ids::kCmd_glNamedBufferSubData);
  CHECK_EQ(rig.mappings.mark(4u, 0, 1), no_error);
  rig.mappings.flush_persistent(rig.wire());
  CHECK_EQ(rig.commands() - flushed, 3u);
  rig.mappings.flush_persistent(rig.wire());
  CHECK_EQ(rig.commands() - flushed, 4u);
  char            bytes[512];
  av::StateWriter writer(bytes, sizeof bytes);
  rig.mappings.report(writer);
  CHECK(std::strstr(writer.dump(), "mappings/0 held=4 persistent_writers=2\n") == writer.dump());
  CHECK(std::strstr(writer.dump(),
                    "mappings/0/mapping/2 buffer=3 offset=0 length=4 access=0x0000000000000042 dirty_from=0 "
                    "dirty_to=0") != nullptr);
  CHECK(rig.mappings.invariants_hold()); }

/* A deleted buffer and a going context unmap with nothing written. */
TEST(mapped_buffers, forget_and_release_give_up_ranges_without_writing)
{ Rig rig;
  CHECK(rig.map(1u, 4, 0, 4, write | persistent, no_error) != nullptr);
  CHECK(rig.map(2u, 4, 0, 4, write, no_error) != nullptr);
  CHECK(rig.map(3u, 4, 0, 4, write | persistent, no_error) != nullptr);
  const std::uint32_t before = rig.commands();
  rig.mappings.forget(1, nullptr);
  const std::uint32_t deleted[] = {0u, 9u, 1u, 2u};
  rig.mappings.forget(0, deleted);
  CHECK_EQ(rig.mappings.held(), 3u);
  rig.mappings.forget(4, deleted);
  CHECK_EQ(rig.mappings.held(), 1u);
  CHECK(rig.mappings.pointer_of(3u) != nullptr);
  CHECK(rig.mappings.invariants_hold());
  rig.mappings.flush_persistent(rig.wire());
  CHECK_EQ(rig.commands() - before, 1u);
  rig.mappings.release();
  CHECK_EQ(rig.mappings.held(), 0u);
  CHECK(rig.mappings.invariants_hold());
  rig.mappings.flush_persistent(rig.wire());
  CHECK_EQ(rig.commands() - before, 1u); }

TEST(mapped_buffers_invariants, fail_for_a_miscount_a_buffer_mapped_twice_or_a_broken_mapping)
{ Rig counted;
  CHECK(counted.map(1u, 4, 0, 4, write, no_error) != nullptr);
  av::glicd::MappedBuffersProbe::miscount(counted.mappings);
  CHECK(!counted.mappings.invariants_hold());
  Rig twice;
  CHECK(twice.map(1u, 4, 0, 4, write, no_error) != nullptr);
  av::glicd::MappedBuffersProbe::duplicate(twice.mappings, twice.wire(), twice.context_rig.platform());
  CHECK(!twice.mappings.invariants_hold());
  Rig broken;
  CHECK(broken.map(1u, 4, 0, 4, write, no_error) != nullptr);
  av::glicd::BufferMappingProbe::set_dirty(av::glicd::MappedBuffersProbe::mapping(broken.mappings, 0u), 0u, 1u);
  CHECK(!broken.mappings.invariants_hold());
  Rig empty;
  av::glicd::MappedBuffersProbe::empty_slot(empty.mappings, empty.context_rig.platform());
  CHECK(!empty.mappings.invariants_hold()); }
