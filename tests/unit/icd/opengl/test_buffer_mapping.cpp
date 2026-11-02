#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "icd/opengl/buffer_mapping.hpp"
#include "icd/opengl/buffer_shadows.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"

#include "gl_protocol_ids.h"

#include <cstdint>
#include <cstring>

using av::GlContextOptions;
using av::GlContextRig;
using av::Status;
using av::glicd::BufferMapping;
using av::glicd::BufferShadows;
using av::glicd::GuestGlContext;
namespace ids = av::glproto;

namespace av::glicd {

struct BufferMappingProbe {
    static void drop_block(BufferMapping &mapping) { mapping.block_ = nullptr; }
    static void set_block(BufferMapping &mapping, std::uint8_t *block) { mapping.block_ = block; }
    static void drop_buffer(BufferMapping &mapping) { mapping.buffer_ = 0u; }
    static void set_length(BufferMapping &mapping, std::uint64_t length) { mapping.length_ = length; }
    static void set_dirty(BufferMapping &mapping, std::uint64_t from, std::uint64_t to)
    {
        mapping.dirty_from_ = from;
        mapping.dirty_to_ = to;
    }
};

}

namespace {

using Probe = av::glicd::BufferMappingProbe;

constexpr std::uint32_t read_only = BufferMapping::map_read;
constexpr std::uint32_t write_only = BufferMapping::map_write;
constexpr std::uint32_t read_write = BufferMapping::map_read | BufferMapping::map_write;
constexpr std::uint32_t explicit_write = BufferMapping::map_write | BufferMapping::map_flush_explicit;
constexpr std::uint32_t invalidating = BufferMapping::map_write | BufferMapping::map_invalidate_range;
constexpr std::uint32_t buffer = 7u;
constexpr std::uint8_t  inline_mode = 1u, blob_mode = 3u;

struct Write {
    std::uint64_t buffer = 0u;
    std::uint64_t offset = 0u;
    std::uint64_t size = 0u;
    std::uint8_t  mode = 0u;
    std::uint64_t lease = 0u;
    std::uint8_t  first = 0u;
};

struct Rig {
    GlContextRig   context_rig;
    GuestGlContext context{context_rig.connection(), context_rig.platform(), context_rig.send_lock(),
                           context_rig.log(), context_rig.extensions(), context_rig.handles(), 1, GlContextOptions()};
    BufferShadows  shadows{context_rig.platform()};
    BufferMapping  mapping{context_rig.platform(), shadows};

    Rig()
    {
        context_rig.host().gl_make("renderer", "4.6");
        CHECK_STATUS(context.make(), Status::ok);
    }
    av::ScriptedHost    &host() { return context_rig.host(); }
    av::MockKmt         &kmt() { return context_rig.kmt(); }
    av::ClientTransport &wire() { return *context.wire(); }
    std::uint32_t        commands()
    {
        context.flush();
        return kmt().commands();
    }
    std::uint32_t        id(std::uint32_t index)
    {
        context.flush();
        return kmt().command_id(index);
    }
    av::WireReader       reader(std::uint32_t index)
    {
        context.flush();
        const std::uint8_t *const bytes = kmt().command_bytes(index);
        CHECK(bytes != nullptr);
        if (bytes == nullptr) return av::WireReader(nullptr, 0u);
        return av::WireReader(bytes + av::CommandHeader::encoded_size,
                              av::MockKmt::journal_bytes - av::CommandHeader::encoded_size);
    }
    void read_at(std::uint32_t index, std::uint64_t offset, std::uint64_t size)
    {
        CHECK_EQ(id(index), ids::kCmd_glGetNamedBufferSubData);
        av::WireReader reader_ = reader(index);
        std::uint64_t  value = 0u;
        CHECK(reader_.read_varint(value));
        CHECK_EQ(value, buffer);
        CHECK(reader_.read_varint(value));
        CHECK_EQ(value, offset);
        CHECK(reader_.read_varint(value));
        CHECK_EQ(value, size);
    }
    Write write_at(std::uint32_t index)
    {
        CHECK_EQ(id(index), ids::kCmd_glNamedBufferSubData);
        av::WireReader reader_ = reader(index);
        Write          sent;
        std::uint64_t  want = 0u;
        CHECK(reader_.read_varint(sent.buffer));
        CHECK(reader_.read_varint(sent.offset));
        CHECK(reader_.read_varint(sent.size));
        CHECK(reader_.read_pod(sent.mode));
        CHECK(reader_.read_varint(want));
        CHECK_EQ(want, sent.size);
        if (sent.mode == blob_mode) CHECK(reader_.read_varint(sent.lease));
        if (sent.mode == inline_mode) CHECK(reader_.read(&sent.first, 1u));
        return sent;
    }
    std::uint8_t *bytes() { return static_cast<std::uint8_t *>(mapping.pointer()); }
};

void pattern(std::uint8_t *bytes, std::size_t size, std::uint8_t seed)
{
    for (std::size_t at = 0u; at < size; at++) bytes[at] = static_cast<std::uint8_t>(at * 7u + seed);
}

}

TEST(buffer_mapping, a_read_map_is_the_hosts_bytes_read_in_chunks_a_reply_holds)
{ Rig rig;
  CHECK(rig.mapping.invariants_hold());
  const std::uint64_t length = BufferMapping::read_bytes + 8u;
  std::uint8_t        first[16];
  std::uint8_t        second[8];
  pattern(first, sizeof first, 1u);
  pattern(second, sizeof second, 90u);
  rig.host().gl_bytes(first, sizeof first);
  rig.host().gl_bytes(second, sizeof second);
  const std::uint32_t before = rig.commands();
  CHECK_STATUS(rig.mapping.map(rig.wire(), buffer, 32u, length, read_only), Status::ok);
  CHECK(rig.mapping.invariants_hold());
  CHECK_EQ(rig.commands() - before, 2u);
  CHECK_EQ(rig.shadows.held(), 0u);
  rig.read_at(before, 32u, BufferMapping::read_bytes);
  rig.read_at(before + 1u, 32u + BufferMapping::read_bytes, 8u);
  CHECK(std::memcmp(rig.bytes(), first, sizeof first) == 0);
  CHECK_EQ(rig.bytes()[sizeof first], 0u);
  CHECK_EQ(rig.bytes()[BufferMapping::read_bytes - 1u], 0u);
  CHECK(std::memcmp(rig.bytes() + BufferMapping::read_bytes, second, sizeof second) == 0);
  CHECK_EQ(rig.mapping.buffer(), buffer);
  CHECK_EQ(rig.mapping.offset(), 32u);
  CHECK_EQ(rig.mapping.length(), length);
  CHECK_EQ(rig.mapping.access(), read_only);
  CHECK(!rig.mapping.persistent_writes());
  rig.bytes()[0] = 0xEEu;
  rig.mapping.write_back(rig.wire());
  CHECK_EQ(rig.commands() - before, 2u);
  CHECK_STATUS(rig.mapping.mark(0u, 1u), Status::invalid_state);
  CHECK_STATUS(rig.mapping.map(rig.wire(), buffer, 0u, 1u, read_only), Status::invalid_state);
  CHECK(rig.mapping.invariants_hold()); }

TEST(buffer_mapping, a_read_write_map_sends_back_only_the_words_changed_since_the_last_write_back)
{ Rig rig;
  std::uint8_t data[24];
  pattern(data, sizeof data, 3u);
  rig.host().gl_bytes(data, sizeof data);
  CHECK_STATUS(rig.mapping.map(rig.wire(), buffer, 4u, sizeof data, read_write), Status::ok);
  CHECK_EQ(rig.shadows.held(), 1u);
  const std::uint32_t before = rig.commands();
  rig.mapping.write_back(rig.wire());
  CHECK_EQ(rig.commands(), before);
  rig.bytes()[17] = 0xABu;
  rig.mapping.write_back(rig.wire());
  rig.mapping.write_back(rig.wire());
  CHECK_EQ(rig.commands() - before, 1u);
  const Write sent = rig.write_at(before);
  CHECK_EQ(sent.buffer, buffer);
  CHECK_EQ(sent.offset, 4u + 16u);
  CHECK_EQ(sent.size, 8u);
  CHECK_EQ(sent.mode, inline_mode);
  CHECK_EQ(sent.first, data[16]);
  CHECK_EQ(rig.shadows.covering(buffer, 4u + 17u, 1u)[0], 0xABu);
  CHECK(!rig.mapping.persistent_writes());
  CHECK(rig.mapping.invariants_hold()); }

TEST(buffer_mapping, a_write_only_map_starts_from_the_hosts_bytes_then_from_the_guests_copy)
{ Rig          rig;
  std::uint8_t data[64];
  pattern(data, sizeof data, 11u);
  rig.host().gl_bytes(data, sizeof data);
  std::uint32_t before = rig.commands();
  CHECK_STATUS(rig.mapping.map(rig.wire(), buffer, 8u, sizeof data, write_only), Status::ok);
  CHECK_EQ(rig.commands() - before, 1u);
  rig.read_at(before, 8u, sizeof data);
  CHECK(std::memcmp(rig.bytes(), data, sizeof data) == 0);
  CHECK_EQ(rig.shadows.held(), 1u);
  rig.bytes()[40] = 0xEEu;
  before = rig.commands();
  rig.mapping.write_back(rig.wire());
  CHECK_EQ(rig.commands() - before, 1u);
  CHECK_EQ(rig.write_at(before).offset, 8u + 40u);
  CHECK_EQ(rig.write_at(before).size, 8u);
  BufferMapping next{rig.context_rig.platform(), rig.shadows};
  before = rig.commands();
  CHECK_STATUS(next.map(rig.wire(), buffer, 16u, 40u, write_only), Status::ok);
  CHECK_EQ(rig.commands(), before);
  const std::uint8_t *const seen = static_cast<const std::uint8_t *>(next.pointer());
  CHECK(std::memcmp(seen, data + 8u, 32u) == 0);
  CHECK_EQ(seen[32], 0xEEu);
  CHECK(std::memcmp(seen + 33u, data + 41u, 7u) == 0);
  next.write_back(rig.wire());
  CHECK_EQ(rig.commands(), before);
  CHECK(next.invariants_hold()); }

TEST(buffer_mapping, a_map_with_nothing_to_keep_asks_the_host_nothing)
{ constexpr std::uint32_t accesses[] = {
      invalidating, BufferMapping::map_write | BufferMapping::map_invalidate_buffer, explicit_write, write_only};
  for (const std::uint32_t access : accesses) {
    Rig rig;
    if (access == write_only) rig.shadows.respecify(buffer, 64, false);
    const std::uint32_t before = rig.commands();
    CHECK_STATUS(rig.mapping.map(rig.wire(), buffer, 0u, 64u, access), Status::ok);
    CHECK_EQ(rig.commands(), before);
    for (std::uint32_t at = 0u; at < 64u; at++) CHECK_EQ(rig.bytes()[at], 0u);
    CHECK_EQ(rig.shadows.held(), 0u);
    CHECK(rig.mapping.invariants_hold());
  } }

TEST(buffer_mapping, a_whole_write_back_becomes_the_copy)
{ Rig rig;
  rig.shadows.respecify(buffer, 64, false);
  CHECK_STATUS(rig.mapping.map(rig.wire(), buffer, 0u, 16u, write_only), Status::ok);
  pattern(rig.bytes(), 16u, 2u);
  const std::uint32_t before = rig.commands();
  rig.mapping.write_back(rig.wire());
  CHECK_EQ(rig.commands() - before, 1u);
  CHECK_EQ(rig.write_at(before).size, 16u);
  CHECK(!rig.shadows.fresh(buffer));
  CHECK(std::memcmp(rig.shadows.covering(buffer, 0u, 16u), rig.bytes(), 16u) == 0);
  rig.mapping.write_back(rig.wire());
  CHECK_EQ(rig.commands() - before, 1u); }

TEST(buffer_mapping, changes_within_the_merge_gap_go_as_one_write)
{ constexpr std::uint64_t length = 1029u;
  struct Span {
      std::uint64_t offset;
      std::uint64_t size;
  };
  constexpr struct {
      std::uint64_t changes[3];
      std::uint32_t count;
      Span          writes[2];
      std::uint32_t written;
  } cases[] = {
      {{0u, 200u, 1027u}, 3u, {{0u, 208u}, {1024u, 5u}}, 2u},
      {{1016u, 1028u, 0u}, 2u, {{1016u, 13u}, {0u, 0u}}, 1u},
      {{1016u, 0u, 0u}, 1u, {{1016u, 8u}, {0u, 0u}}, 1u},
      {{0u, 256u, 0u}, 2u, {{0u, 264u}, {0u, 0u}}, 1u},
      {{0u, 264u, 0u}, 2u, {{0u, 8u}, {264u, 8u}}, 2u},
  };
  static_assert(BufferMapping::merge_gap == 256u, "the cases are drawn for this gap");
  for (const auto &row : cases) {
    Rig          rig;
    std::uint8_t zeros[length] = {};
    rig.shadows.keep(buffer, 0u, length, zeros);
    CHECK_STATUS(rig.mapping.map(rig.wire(), buffer, 0u, length, write_only), Status::ok);
    for (std::uint32_t change = 0u; change < row.count; change++) rig.bytes()[row.changes[change]] = 0x77u;
    const std::uint32_t before = rig.commands();
    rig.mapping.write_back(rig.wire());
    CHECK_EQ(rig.commands() - before, row.written);
    for (std::uint32_t write = 0u; write < row.written; write++) {
      const Write sent = rig.write_at(before + write);
      CHECK_EQ(sent.offset, row.writes[write].offset);
      CHECK_EQ(sent.size, row.writes[write].size);
    }
    rig.mapping.write_back(rig.wire());
    CHECK_EQ(rig.commands() - before, row.written);
  } }

TEST(buffer_mapping, an_explicit_write_back_puts_what_it_sent_into_the_copy)
{ Rig          rig;
  std::uint8_t zeros[64] = {};
  rig.shadows.keep(buffer, 100u, sizeof zeros, zeros);
  CHECK_STATUS(rig.mapping.map(rig.wire(), buffer, 100u, 64u, explicit_write), Status::ok);
  rig.bytes()[10] = 0x5Au;
  rig.bytes()[40] = 0x6Bu;
  CHECK_STATUS(rig.mapping.mark(8u, 4u), Status::ok);
  rig.mapping.write_back(rig.wire());
  const std::uint8_t *const copy = rig.shadows.covering(buffer, 100u, 64u);
  CHECK_EQ(copy[10], 0x5Au);
  CHECK_EQ(copy[40], 0u); }

TEST(buffer_mapping, an_explicit_map_writes_back_the_span_of_what_was_marked_once)
{ Rig rig;
  CHECK_STATUS(rig.mapping.map(rig.wire(), buffer, 100u, 64u, explicit_write), Status::ok);
  const std::uint32_t before = rig.commands();
  rig.mapping.write_back(rig.wire());
  CHECK_EQ(rig.commands(), before);
  CHECK_STATUS(rig.mapping.mark(0u, 0u), Status::ok);
  CHECK_STATUS(rig.mapping.mark(65u, 0u), Status::invalid_parameter);
  CHECK_STATUS(rig.mapping.mark(60u, 5u), Status::invalid_parameter);
  CHECK_STATUS(rig.mapping.mark(1u, ~std::uint64_t{0}), Status::invalid_parameter);
  CHECK_STATUS(rig.mapping.mark(20u, 4u), Status::ok);
  CHECK_STATUS(rig.mapping.mark(10u, 2u), Status::ok);
  CHECK_STATUS(rig.mapping.mark(12u, 1u), Status::ok);
  CHECK_STATUS(rig.mapping.mark(60u, 4u), Status::ok);
  CHECK_STATUS(rig.mapping.mark(30u, 1u), Status::ok);
  CHECK(rig.mapping.invariants_hold());
  rig.bytes()[10] = 0x5Au;
  rig.mapping.write_back(rig.wire());
  rig.mapping.write_back(rig.wire());
  CHECK_EQ(rig.commands() - before, 1u);
  const Write sent = rig.write_at(before);
  CHECK_EQ(sent.offset, 110u);
  CHECK_EQ(sent.size, 54u);
  CHECK_EQ(sent.first, 0x5Au);
  CHECK_STATUS(rig.mapping.mark(8u, 1u), Status::ok);
  CHECK_STATUS(rig.mapping.mark(2u, 1u), Status::ok);
  rig.mapping.write_back(rig.wire());
  CHECK_EQ(rig.write_at(before + 1u).offset, 102u);
  CHECK_EQ(rig.write_at(before + 1u).size, 7u);
  CHECK(rig.mapping.invariants_hold()); }

TEST(buffer_mapping, a_persistent_map_is_a_persistent_writer_only_when_it_writes)
{ constexpr struct {
      std::uint32_t access;
      bool          writes;
  } cases[] = {
      {BufferMapping::map_persistent | BufferMapping::map_write, true},
      {BufferMapping::map_persistent | read_write, true},
      {BufferMapping::map_persistent | write_only | BufferMapping::map_flush_explicit, true},
      {BufferMapping::map_persistent | read_only, false},
      {write_only, false},
  };
  for (const auto &row : cases) {
    Rig rig;
    if ((row.access & BufferMapping::map_flush_explicit) == 0u) rig.host().gl_bytes(nullptr, 0u);
    CHECK_STATUS(rig.mapping.map(rig.wire(), buffer, 0u, 4u, row.access), Status::ok);
    CHECK_EQ(rig.mapping.persistent_writes(), row.writes);
  } }

TEST(buffer_mapping, a_range_larger_than_the_arena_crosses_in_leases_with_a_retire_between)
{ Rig rig;
  const std::uint64_t mib = std::uint64_t{1} << 20u;
  const std::uint64_t length = 17u * mib;
  CHECK(rig.wire().blobs().set_aside(rig.wire().blobs().room() - 16u * mib).taken());
  CHECK_EQ(rig.wire().blobs().room(), 16u * mib);
  CHECK_STATUS(rig.mapping.map(rig.wire(), buffer, 0u, length, invalidating), Status::ok);
  pattern(rig.bytes(), static_cast<std::size_t>(length), 5u);
  rig.host().gl_varint(1u);
  const std::uint32_t before = rig.commands();
  rig.mapping.write_back(rig.wire());
  CHECK_EQ(rig.commands() - before, 6u);
  for (std::uint32_t chunk = 0u; chunk < 4u; chunk++) {
    const Write sent = rig.write_at(before + chunk);
    CHECK_EQ(sent.mode, blob_mode);
    CHECK_EQ(sent.offset, chunk * BufferMapping::blob_write_bytes);
    CHECK_EQ(sent.size, BufferMapping::blob_write_bytes);
    CHECK_EQ(sent.lease, chunk * BufferMapping::blob_write_bytes);
  }
  CHECK_EQ(rig.id(before + 4u), ids::kCmd_glIsBuffer);
  const Write last = rig.write_at(before + 5u);
  CHECK_EQ(last.mode, blob_mode);
  CHECK_EQ(last.offset, 16u * mib);
  CHECK_EQ(last.size, mib);
  CHECK_EQ(last.lease, 0u);
  av::ClientBlobArena  &blobs = rig.wire().blobs();
  const av::ClientBlobLease top = blobs.take(1u);
  CHECK_EQ(top.offset(), mib);
  const auto *const base = static_cast<const std::uint8_t *>(top.data()) - top.offset();
  CHECK(std::memcmp(base, rig.bytes() + 16u * mib, static_cast<std::size_t>(mib)) == 0);
  blobs.give_back(top); }

TEST(buffer_mapping, without_room_after_a_retire_or_an_arena_the_bytes_go_inline)
{ Rig rig;
  const std::size_t length = BufferMapping::inline_write_bytes + 8u;
  CHECK_STATUS(rig.mapping.map(rig.wire(), buffer, 0u, length, invalidating), Status::ok);
  pattern(rig.bytes(), length, 9u);
  std::uint8_t small[64];
  rig.wire().blobs().lend(small, sizeof small);
  rig.host().gl_varint(1u);
  std::uint32_t before = rig.commands();
  rig.mapping.write_back(rig.wire());
  CHECK_EQ(rig.commands() - before, 3u);
  CHECK_EQ(rig.id(before), ids::kCmd_glIsBuffer);
  Write sent = rig.write_at(before + 1u);
  CHECK_EQ(sent.mode, inline_mode);
  CHECK_EQ(sent.size, BufferMapping::inline_write_bytes);
  CHECK_EQ(sent.first, 9u);
  sent = rig.write_at(before + 2u);
  CHECK_EQ(sent.mode, inline_mode);
  CHECK_EQ(sent.offset, BufferMapping::inline_write_bytes);
  CHECK_EQ(sent.size, 8u);
  CHECK_EQ(sent.first, rig.bytes()[BufferMapping::inline_write_bytes]);
  rig.wire().blobs().lend(nullptr, 0u);
  pattern(rig.bytes(), length, 10u);
  before = rig.commands();
  rig.mapping.write_back(rig.wire());
  CHECK_EQ(rig.commands() - before, 2u);
  CHECK_EQ(rig.write_at(before).size, BufferMapping::inline_write_bytes);
  CHECK_EQ(rig.write_at(before + 1u).size, 8u); }

TEST(buffer_mapping, refuses_no_buffer_no_bytes_too_many_bytes_and_no_memory)
{ Rig rig;
  const std::uint32_t before = rig.commands();
  CHECK_STATUS(rig.mapping.map(rig.wire(), 0u, 0u, 4u, read_only), Status::invalid_parameter);
  CHECK_STATUS(rig.mapping.map(rig.wire(), buffer, 0u, 0u, read_only), Status::invalid_parameter);
  CHECK_STATUS(rig.mapping.map(rig.wire(), buffer, 0u, BufferMapping::max_length + 1u, write_only),
               Status::invalid_parameter);
  rig.context_rig.platform().fail_allocation_at(1u);
  CHECK_STATUS(rig.mapping.map(rig.wire(), buffer, 0u, 4u, read_only), Status::no_memory);
  CHECK_EQ(rig.commands(), before);
  CHECK(rig.mapping.pointer() == nullptr);
  CHECK_EQ(rig.mapping.buffer(), 0u);
  CHECK(rig.mapping.invariants_hold()); }

TEST(buffer_mapping, reports_its_range_access_and_marks)
{ Rig rig;
  CHECK_STATUS(rig.mapping.map(rig.wire(), buffer, 12u, 32u, explicit_write), Status::ok);
  CHECK_STATUS(rig.mapping.mark(4u, 4u), Status::ok);
  char            bytes[256];
  av::StateWriter writer(bytes, sizeof bytes);
  rig.mapping.report(writer, 5u);
  CHECK(std::strcmp(writer.dump(),
                    "mapping/5 buffer=7 offset=12 length=32 access=0x0000000000000012 dirty_from=4 dirty_to=8") == 0); }

TEST(buffer_mapping_invariants, fail_for_a_half_held_range_or_marks_outside_it_or_on_a_map_without_them)
{ Rig block;
  CHECK_STATUS(block.mapping.map(block.wire(), buffer, 0u, 8u, explicit_write), Status::ok);
  std::uint8_t *const held = block.bytes();
  Probe::drop_block(block.mapping);
  CHECK(!block.mapping.invariants_hold());
  Probe::set_block(block.mapping, held);
  Probe::set_length(block.mapping, 0u);
  CHECK(!block.mapping.invariants_hold());
  Probe::set_length(block.mapping, 8u);
  Probe::drop_buffer(block.mapping);
  CHECK(!block.mapping.invariants_hold());
  Rig marks;
  CHECK_STATUS(marks.mapping.map(marks.wire(), buffer, 0u, 8u, explicit_write), Status::ok);
  Probe::set_dirty(marks.mapping, 4u, 2u);
  CHECK(!marks.mapping.invariants_hold());
  Probe::set_dirty(marks.mapping, 0u, 9u);
  CHECK(!marks.mapping.invariants_hold());
  Probe::set_dirty(marks.mapping, 0u, 8u);
  CHECK(marks.mapping.invariants_hold());
  Rig plain;
  plain.host().gl_bytes(nullptr, 0u);
  CHECK_STATUS(plain.mapping.map(plain.wire(), buffer, 0u, 8u, write_only), Status::ok);
  Probe::set_dirty(plain.mapping, 0u, 1u);
  CHECK(!plain.mapping.invariants_hold());
  Rig empty;
  std::uint8_t stray = 0u;
  Probe::set_block(empty.mapping, &stray);
  CHECK(!empty.mapping.invariants_hold());
  Probe::set_block(empty.mapping, nullptr);
  CHECK(empty.mapping.invariants_hold()); }
