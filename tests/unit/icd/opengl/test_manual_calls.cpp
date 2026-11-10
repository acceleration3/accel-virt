#include "harness/framework/test_registry.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/scanout_target.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"
#include "proto/resources/surface_layout.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "harness/icd/gl_icd_rig.hpp"
#include "harness/icd/recording_thread_lock.hpp"
#include "harness/icd/scripted_thread_slot.hpp"
#include "icd/opengl/buffer_pieces.hpp"
#include "icd/opengl/buffer_shadows.hpp"
#include "icd/opengl/current_contexts.hpp"
#include "icd/opengl/gl_context_table.hpp"
#include "icd/opengl/gl_extension_policy.hpp"
#include "icd/opengl/gl_entry_points.hpp"
#include "icd/opengl/gl_external_objects.hpp"
#include "icd/opengl/gl_error_query.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "icd/opengl/pinned_buffers.hpp"
#include "icd/opengl/manual_calls.hpp"
#include "icd/opengl/sync_wait_probe.hpp"
#include "icd/opengl/sliced_buffers.hpp"
#include "icd/opengl/buffer_mapping.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"

#include "gl_icd_entries.h"
#include "gl_protocol_ids.h"

#include <cstdint>
#include <cstring>

using av::GlIcdRig;
using av::glicd::ManualCalls;
using av::glicd::SyncWaitProbe;
namespace gen = av::glicd::generated;
namespace ids = av::glproto;

namespace {

constexpr GLenum gl_invalid_enum = 0x0500u, gl_invalid_value = 0x0501u, gl_invalid_operation = 0x0502u;
constexpr GLenum array_buffer = 0x8892u, element_array_buffer = 0x8893u, pixel_pack_buffer = 0x88EBu;
constexpr GLenum gl_triangles = 0x0004u, gl_unsigned_short = 0x1403u, gl_float = 0x1406u;
constexpr GLenum gl_read_write = 0x88BAu, gl_buffer_map_pointer = 0x88BDu;

void answer_size(av::ScriptedHost &host, std::int64_t size)
{
    host.gl_bytes(reinterpret_cast<const std::uint8_t *>(&size), sizeof size);
}

struct Bare {
    av::GlContextRig               rig;
    av::ScriptedThreadSlot         slot;
    av::glicd::CurrentContexts     currents{slot};
    av::RecordingThreadLock        lock{rig.trace(), 'T'};
    av::RecordingThreadLock        slice_lock{rig.trace(), 'M'};
    av::glicd::GlContextTable      contexts;
    ManualCalls                    calls;

    explicit Bare(const av::glicd::GlExtensionPolicy &policy)
        : contexts(rig.platform(), rig.connection(), lock, rig.send_lock(), slice_lock, rig.lines(), policy,
                   rig.handles()),
          calls(currents, contexts, rig.kmt())
    {}
    Bare() : Bare(default_policy()) {}

private:
    static const av::glicd::GlExtensionPolicy &default_policy()
    {
        static const av::glicd::GlExtensionPolicy none(nullptr, 0u, nullptr, 0u);
        return none;
    }
};

constexpr std::uint64_t no_arena = 4096u;

class Current {
public:
    explicit Current(std::uint64_t window = av::MockKmt::rig_window_bytes)
    {
        if (window != no_arena) rig_.kmt().open_host_window(window);
        rig_.load_with_format();
        handle_ = window == no_arena ? current_without_arena() : rig_.current_context();
        CHECK(handle_ != nullptr);
        if (handle_ != nullptr) context().flush();
        commands_ = rig_.kmt().commands();
    }
    void *handle() const { return handle_; }
    GlIcdRig                  &rig() { return rig_; }
    av::glicd::GuestGlContext &context() { return *rig_.root().current(); }
    std::uint32_t sent_since(std::uint32_t index)
    {
        context().flush();
        return rig_.kmt().command_id(absolute(index));
    }
    std::uint64_t argument_since(std::uint32_t index, std::uint32_t nth)
    {
        context().flush();
        av::WireReader reader(rig_.kmt().command_bytes(absolute(index)) + av::CommandHeader::encoded_size,
                              av::MockKmt::journal_bytes - av::CommandHeader::encoded_size);
        std::uint64_t  value = 0u;
        for (std::uint32_t at = 0u; at <= nth; at++) CHECK(reader.read_varint(value));
        return value;
    }
    std::uint32_t sent()
    {
        context().flush();
        std::uint32_t count = 0u;
        for (std::uint32_t at = commands_; at < rig_.kmt().commands(); at++)
            count += rig_.kmt().command_id(at) == av::CommandHeader::channel_stats_id ? 0u : 1u;
        return count;
    }
    const std::uint8_t *command_since(std::uint32_t index)
    {
        context().flush();
        return rig_.kmt().command_bytes(absolute(index));
    }
    std::uint32_t error() { return av::glicd::entry::get_error(); }

private:
    std::uint32_t absolute(std::uint32_t index)
    {
        std::uint32_t seen = 0u;
        for (std::uint32_t at = commands_; at < rig_.kmt().commands(); at++) {
            if (rig_.kmt().command_id(at) == av::CommandHeader::channel_stats_id) continue;
            if (seen++ == index) return at;
        }
        return rig_.kmt().commands();
    }
    void *current_without_arena()
    {
        void *const context = rig_.opengl32().create_context(rig_.dc());
        if (context == nullptr) return nullptr;
        av::ScriptedHost &host = rig_.host();
        host.blob_import(1u, 0u);
        host.blob_import(1u, 0u);
        host.gl_string("av host vendor");
        host.gl_string("host gpu");
        host.gl_string("4.6.0 host");
        host.gl_string("4.60");
        for (int list = 0; list < 3; list++) host.gl_integer(0);
        host.gl_integer(4);
        host.gl_integer(6);
        host.gl_integer(2);
        host.gl_integer(0);
        host.gl_integer(0);
        host.gl_window(1u, 1u);
        return rig_.opengl32().make_current(rig_.dc(), context) ? context : nullptr;
    }

    GlIcdRig      rig_;
    void         *handle_ = nullptr;
    std::uint32_t commands_ = 0u;
};

std::uint64_t storage_flags(Current &current, std::uint32_t index)
{
    av::WireReader reader(current.command_since(index) + av::CommandHeader::encoded_size,
                          av::MockKmt::journal_bytes - av::CommandHeader::encoded_size);
    std::uint64_t  value = 0u;
    std::uint8_t   mode = 0u;
    std::uint8_t   bytes[8];
    CHECK(reader.read_varint(value));
    CHECK(reader.read_varint(value));
    CHECK(reader.read_pod(mode));
    CHECK_EQ(mode, 1u);
    CHECK(reader.read_varint(value));
    CHECK_EQ(value, 8u);
    CHECK(reader.read(bytes, sizeof bytes));
    CHECK(reader.read_varint(value));
    return value;
}

}

TEST(manual_calls, a_front_buffer_read_is_answered_and_every_other_is_sent)
{ Current      current;
  ManualCalls &calls = current.rig().root().manual();
  std::uint8_t pixels[64] = {};
  const std::uint32_t before = current.sent();
  current.rig().host().gl_bytes(nullptr, 0u);
  calls.read_pixels(0, 0, 1, 1, 0x80E1u , 0x1401u, pixels);
  CHECK_EQ(current.sent(), before + 1u);

  current.rig().host().gl_bytes(nullptr, 0u);
  CHECK(current.rig().opengl32().swap_buffers(current.rig().dc()));
  calls.read_buffer(0x0404u );
  const std::uint32_t asked = current.sent();
  calls.read_pixels(0, 0, 1, 1, 0x80E1u, 0x1401u, pixels);
  CHECK_EQ(current.sent(), asked);
  current.rig().host().gl_bytes(nullptr, 0u);
  calls.read_pixels(0, 0, 1, 1, 0x1907u , 0x1401u, pixels);
  CHECK(current.sent() > asked); }

TEST(manual_calls, a_read_into_a_pack_buffer_is_sent_as_an_offset)
{ Current      current;
  ManualCalls &calls = current.rig().root().manual();
  calls.bind_buffer(pixel_pack_buffer, 3u);
  const std::uint32_t before = current.sent();
  calls.read_pixels(0, 0, 1, 1, 0x80E1u, 0x1401u, nullptr);
  CHECK_EQ(current.sent(), before + 1u); }

TEST(manual_calls, a_read_with_nothing_current_is_not_sent)
{ GlIcdRig rig;
  rig.load_with_format();
  std::uint8_t pixels[16] = {};
  const std::uint32_t before = rig.kmt().commands();
  rig.root().manual().read_pixels(0, 0, 1, 1, 0x80E1u, 0x1401u, pixels);
  CHECK_EQ(rig.kmt().commands(), before); }

TEST(manual_calls, the_granted_profile_is_answered_without_the_host)
{ Current      current;
  ManualCalls &calls = current.rig().root().manual();
  const std::uint32_t before = current.sent();
  GLint mask = -1, major = -1, minor = -1;
  calls.integers(0x9126u , &mask);
  calls.integers(0x821Bu , &major);
  calls.integers(0x821Cu , &minor);
  CHECK_EQ(mask, 2);
  CHECK_EQ(major, 4);
  CHECK_EQ(minor, 6);
  CHECK_EQ(current.sent(), before);
  calls.integers(0x9126u, nullptr);
  CHECK_EQ(current.sent(), before); }

TEST(manual_calls, pixel_store_is_kept_and_sent)
{ Current current;
  gen::Wrap_glPixelStorei(0x0D05u , 1);
  gen::Wrap_glPixelStoref(0x0D02u , 2.6f);
  gen::Wrap_glPixelStoref(0x0D02u, -5.0f);
  gen::Wrap_glPixelStoref(0x0D02u, 3.0e10f);
  gen::Wrap_glPixelStoref(0x0D02u, -3.0e10f);
  CHECK_EQ(current.sent(), 5u);
  CHECK_EQ(current.sent_since(0u), ids::kCmd_glPixelStorei);
  CHECK_EQ(current.sent_since(1u), ids::kCmd_glPixelStoref);
  CHECK_EQ(current.context().arrays().pixel_bytes(0x1908u, 0x1401u, 1, 2, 1, true), 12u + 4u); }

TEST(manual_calls, buffer_binds_and_deletes_are_kept_and_sent)
{ Current current;
  gen::Wrap_glBindBuffer(pixel_pack_buffer, 5u);
  CHECK(current.context().arrays().pixel_buffer_bound(true));
  const GLuint deleted[] = {5u};
  gen::Wrap_glDeleteBuffers(1, deleted);
  CHECK(!current.context().arrays().pixel_buffer_bound(true));
  gen::Wrap_glBindVertexArray(3u);
  gen::Wrap_glBindBuffer(element_array_buffer, 9u);
  CHECK(current.context().arrays().indices_are_offset());
  const GLuint arrays[] = {3u};
  gen::Wrap_glDeleteVertexArrays(1, arrays);
  CHECK(!current.context().arrays().indices_are_offset());
  const std::uint32_t expected[] = {ids::kCmd_glBindBuffer, ids::kCmd_glDeleteBuffers, ids::kCmd_glBindVertexArray,
                                    ids::kCmd_glBindBuffer, ids::kCmd_glDeleteVertexArrays};
  CHECK_EQ(current.sent(), 5u);
  for (std::uint32_t index = 0u; index < 5u; index++) CHECK_EQ(current.sent_since(index), expected[index]); }

TEST(manual_calls, names_and_data_are_the_hosts)
{ Current current;
  const GLuint made[] = {11u, 12u};
  current.rig().host().gl_bytes(reinterpret_cast<const std::uint8_t *>(made), sizeof made);
  GLuint buffers[2] = {};
  gen::Wrap_glGenBuffers(2, buffers);
  CHECK_EQ(buffers[1], 12u);
  current.rig().host().gl_bytes(reinterpret_cast<const std::uint8_t *>(made), sizeof(GLuint));
  GLuint texture = 0u;
  gen::Wrap_glGenTextures(1, &texture);
  CHECK_EQ(texture, 11u);
  current.rig().host().gl_bytes(reinterpret_cast<const std::uint8_t *>(made), sizeof(GLuint));
  GLuint array = 0u;
  gen::Wrap_glGenVertexArrays(1, &array);
  CHECK_EQ(array, 11u);
  const std::uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  gen::Wrap_glBufferData(array_buffer, sizeof data, data, 0x88E4u);
  gen::Wrap_glBufferStorage(array_buffer, sizeof data, data, 0u);
  gen::Wrap_glNamedBufferData(11u, sizeof data, data, 0x88E4u);
  gen::Wrap_glNamedBufferStorage(11u, sizeof data, data, 0u);
  const std::uint32_t expected[] = {ids::kCmd_glGenBuffers,     ids::kCmd_glGenTextures,  ids::kCmd_glGenVertexArrays,
                                    ids::kCmd_glBufferData,     ids::kCmd_glBufferStorage, ids::kCmd_glNamedBufferData,
                                    ids::kCmd_glNamedBufferStorage};
  CHECK_EQ(current.sent(), 7u);
  for (std::uint32_t index = 0u; index < 7u; index++) CHECK_EQ(current.sent_since(index), expected[index]);
  CHECK_EQ(storage_flags(current, 4u), 0x0100u);
  CHECK_EQ(storage_flags(current, 6u), 0x0100u); }

TEST(manual_calls, a_client_side_array_is_kept_and_an_offset_is_sent)
{ Current current;
  gen::Wrap_glVertexAttribPointer(2u, 3, gl_float, 0u, 0, reinterpret_cast<const void *>(0x1000u));
  CHECK_EQ(current.sent(), 0u);
  gen::Wrap_glBindBuffer(array_buffer, 4u);
  gen::Wrap_glVertexAttribPointer(2u, 3, gl_float, 0u, 12, reinterpret_cast<const void *>(0x10u));
  gen::Wrap_glEnableVertexAttribArray(2u);
  gen::Wrap_glDisableVertexAttribArray(2u);
  CHECK_EQ(current.sent(), 4u);
  CHECK_EQ(current.sent_since(1u), ids::kCmd_glVertexAttribPointer);
  CHECK_EQ(current.sent_since(2u), ids::kCmd_glEnableVertexAttribArray);
  CHECK_EQ(current.sent_since(3u), ids::kCmd_glDisableVertexAttribArray);
  void *pointer = nullptr;
  gen::Wrap_glGetVertexAttribPointerv(2u, 0x8645u, &pointer);
  CHECK(pointer == reinterpret_cast<void *>(0x10u));
  gen::Wrap_glGetVertexAttribPointerv(2u, 0x1234u, &pointer);
  CHECK_EQ(current.error(), gl_invalid_enum);
  gen::Wrap_glGetVertexAttribPointerv(99u, 0x8645u, &pointer);
  CHECK_EQ(current.error(), gl_invalid_value);
  gen::Wrap_glGetVertexAttribPointerv(2u, 0x8645u, nullptr);
  CHECK_EQ(current.sent(), 4u); }

TEST(manual_calls, draws_are_sent)
{ Current current;
  gen::Wrap_glDrawArrays(gl_triangles, 0, 3);
  gen::Wrap_glDrawArraysInstanced(gl_triangles, 0, 3, 2);
  CHECK_EQ(current.sent(), 2u);
  gen::Wrap_glDrawElements(gl_triangles, 3, gl_unsigned_short, nullptr);
  gen::Wrap_glBindVertexArray(1u);
  gen::Wrap_glBindBuffer(element_array_buffer, 2u);
  gen::Wrap_glDrawElements(gl_triangles, 3, gl_unsigned_short, reinterpret_cast<const void *>(6u));
  gen::Wrap_glDrawElementsInstanced(gl_triangles, 3, gl_unsigned_short, nullptr, 4);
  gen::Wrap_glDrawRangeElements(gl_triangles, 0u, 2u, 3, gl_unsigned_short, nullptr);
  const std::uint32_t expected[] = {ids::kCmd_glDrawArrays,       ids::kCmd_glDrawArraysInstanced,
                                    ids::kCmd_glDrawElements,     ids::kCmd_glBindVertexArray,
                                    ids::kCmd_glBindBuffer,       ids::kCmd_glDrawElements,
                                    ids::kCmd_glDrawElementsInstanced, ids::kCmd_glDrawRangeElements};
  CHECK_EQ(current.sent(), 8u);
  for (std::uint32_t index = 0u; index < 8u; index++) CHECK_EQ(current.sent_since(index), expected[index]); }

TEST(manual_calls, flush_wait_and_program_calls_reach_the_host)
{ Current current;
  gen::Wrap_glFlush();
  current.rig().host().gl_varint(0x911Au );
  CHECK_EQ(gen::Wrap_glClientWaitSync(reinterpret_cast<GLsync>(0x9u), 1u, 1000u), 0x911Au);
  current.rig().host().gl_varint(3u);
  CHECK_EQ(gen::Wrap_glGetAttribLocation(5u, "position"), 3);
  gen::Wrap_glLinkProgram(5u);
  gen::Wrap_glDeleteProgram(5u);
  const std::uint32_t expected[] = {ids::kCmd_glFlush, ids::kCmd_glClientWaitSync, ids::kCmd_glGetAttribLocation,
                                    ids::kCmd_glLinkProgram, ids::kCmd_glDeleteProgram};
  CHECK_EQ(current.sent(), 5u);
  for (std::uint32_t index = 0u; index < 5u; index++) CHECK_EQ(current.sent_since(index), expected[index]); }

TEST(manual_calls, a_uniform_location_learns_its_shape)
{ Current current;
  current.rig().host().gl_varint(4u);
  current.rig().host().gl_varint(0u);
  current.rig().host().gl_resource_property(0x8B5C );
  CHECK_EQ(gen::Wrap_glGetUniformLocation(5u, "transform"), 4);
  CHECK_EQ(current.context().uniforms().values(5u, 4), 16u);
  const std::uint32_t expected[] = {ids::kCmd_glGetUniformLocation, ids::kCmd_glGetProgramResourceIndex,
                                    ids::kCmd_glGetProgramResourceiv};
  for (std::uint32_t index = 0u; index < 3u; index++) CHECK_EQ(current.sent_since(index), expected[index]);
  gen::Wrap_glLinkProgram(5u);
  CHECK_EQ(current.context().uniforms().values(5u, 4), 0u);
  current.rig().host().gl_varint(static_cast<std::uint64_t>(-1));
  CHECK_EQ(gen::Wrap_glGetUniformLocation(5u, "missing"), -1);
  current.rig().host().gl_varint(6u);
  current.rig().host().gl_varint(0xFFFFFFFFu);
  CHECK_EQ(gen::Wrap_glGetUniformLocation(5u, "hidden"), 6);
  CHECK_EQ(current.context().uniforms().values(5u, 6), 0u);
  current.rig().host().gl_varint(7u);
  current.rig().host().gl_varint(1u);
  const std::uint8_t nothing[1] = {0};
  current.rig().host().gl_bytes(nothing, 0u);
  CHECK_EQ(gen::Wrap_glGetUniformLocation(5u, "unshaped"), 7);
  CHECK_EQ(current.context().uniforms().values(5u, 7), 0u);
  current.rig().host().gl_varint(8u);
  current.rig().host().gl_varint(0u);
  current.rig().host().gl_resource_property(0x1406);
  CHECK_EQ(gen::Wrap_glGetUniformLocation(5u, "scale"), 8);
  gen::Wrap_glDeleteProgram(5u);
  CHECK_EQ(current.context().uniforms().values(5u, 8), 0u); }

TEST(manual_calls, a_uniform_shape_with_no_memory_to_keep_it_is_out_of_memory)
{ Current current;
  current.rig().host().gl_varint(4u);
  current.rig().host().gl_varint(0u);
  current.rig().host().gl_resource_property(0x8B5C );
  current.rig().platform().fail_allocation_at(1u);
  CHECK_EQ(gen::Wrap_glGetUniformLocation(5u, "transform"), 4);
  CHECK_EQ(current.context().uniforms().values(5u, 4), 0u);
  CHECK_EQ(current.context().take_error(), 0x0505u ); }

TEST(manual_calls, a_vertex_array_with_no_memory_to_keep_is_out_of_memory)
{ Current current;
  const std::uint32_t sent = current.sent();
  current.rig().platform().fail_allocation_at(1u);
  gen::Wrap_glBindVertexArray(9u);
  CHECK_EQ(current.context().take_error(), 0x0505u );
  CHECK_EQ(current.sent(), sent + 1u);
  CHECK_EQ(current.sent_since(sent), ids::kCmd_glBindVertexArray); }

TEST(manual_calls, strings_are_the_hosts_and_no_extension_is_listed)
{ Current current;
  const std::uint32_t sent = current.sent();
  CHECK(std::strcmp(reinterpret_cast<const char *>(gen::Wrap_glGetString(0x1F00u)), "av host vendor") == 0);
  CHECK(std::strcmp(reinterpret_cast<const char *>(gen::Wrap_glGetString(0x1F03u)), "") == 0);
  CHECK(gen::Wrap_glGetString(0x1234u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_enum);
  CHECK(gen::Wrap_glGetStringi(0x1F03u, 0u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_value);
  CHECK(gen::Wrap_glGetStringi(0x1F00u, 0u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_enum);
  CHECK_EQ(current.sent(), sent); }

TEST(manual_calls, a_target_map_is_the_hosts_bound_buffer_filled_and_written_back)
{ Current current;
  av::ScriptedHost &host = current.rig().host();
  const std::uint8_t data[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  gen::Wrap_glBindBuffer(array_buffer, 7u);
  answer_size(host, sizeof data);
  host.gl_bytes(data, sizeof data);
  auto *const mapped = static_cast<std::uint8_t *>(gen::Wrap_glMapBuffer(array_buffer, gl_read_write));
  CHECK(mapped != nullptr);
  CHECK(std::memcmp(mapped, data, sizeof data) == 0);
  CHECK_EQ(current.sent(), 3u);
  CHECK_EQ(current.sent_since(0u), ids::kCmd_glBindBuffer);
  CHECK_EQ(current.argument_since(0u, 0u), array_buffer);
  CHECK_EQ(current.sent_since(1u), ids::kCmd_glGetNamedBufferParameteri64v);
  CHECK_EQ(current.argument_since(1u, 0u), 7u);
  CHECK_EQ(current.argument_since(1u, 1u), 0x8764u);
  CHECK_EQ(current.sent_since(2u), ids::kCmd_glGetNamedBufferSubData);
  CHECK_EQ(current.argument_since(2u, 2u), sizeof data);
  CHECK_EQ(current.rig().log().holding("buffer maps are guest copies"), 1u);
  CHECK(current.context().mappings().invariants_hold());
  void *pointer = nullptr;
  gen::Wrap_glGetBufferPointerv(array_buffer, gl_buffer_map_pointer, &pointer);
  CHECK(pointer == mapped);
  pointer = nullptr;
  gen::Wrap_glGetNamedBufferPointerv(7u, gl_buffer_map_pointer, &pointer);
  CHECK(pointer == mapped);
  CHECK_EQ(current.sent(), 3u);
  mapped[0] = 0xAAu;
  CHECK(gen::Wrap_glUnmapBuffer(array_buffer) == 1u);
  CHECK_EQ(current.sent(), 4u);
  CHECK_EQ(current.sent_since(3u), ids::kCmd_glNamedBufferSubData);
  CHECK_EQ(current.argument_since(3u, 0u), 7u);
  CHECK_EQ(current.argument_since(3u, 1u), 0u);
  CHECK_EQ(current.argument_since(3u, 2u), 8u);
  CHECK_EQ(current.context().mappings().held(), 0u);
  CHECK_EQ(current.error(), 0u); }

TEST(manual_calls, an_explicit_range_writes_back_only_what_was_flushed)
{ Current current;
  av::ScriptedHost &host = current.rig().host();
  gen::Wrap_glBindBuffer(array_buffer, 7u);
  answer_size(host, 16u);
  const GLbitfield explicit_write = 0x0002u | 0x0010u;
  auto *const mapped = static_cast<std::uint8_t *>(gen::Wrap_glMapBufferRange(array_buffer, 4, 8, explicit_write));
  CHECK(mapped != nullptr);
  CHECK_EQ(mapped[7], 0u);
  CHECK_EQ(current.sent(), 2u);
  gen::Wrap_glFlushMappedBufferRange(array_buffer, 1, 2);
  gen::Wrap_glFlushMappedNamedBufferRange(7u, 6, 1);
  CHECK(gen::Wrap_glUnmapNamedBuffer(7u) == 1u);
  CHECK_EQ(current.sent(), 3u);
  CHECK_EQ(current.sent_since(2u), ids::kCmd_glNamedBufferSubData);
  CHECK_EQ(current.argument_since(2u, 1u), 5u);
  CHECK_EQ(current.argument_since(2u, 2u), 6u);
  CHECK_EQ(current.error(), 0u); }

TEST(manual_calls, a_persistent_write_map_is_sent_before_every_draw)
{ Current current;
  av::ScriptedHost &host = current.rig().host();
  answer_size(host, 8u);
  host.gl_bytes(nullptr, 0u);
  const GLbitfield persistent_write = 0x0002u | 0x0040u;
  auto *const mapped = static_cast<std::uint8_t *>(gen::Wrap_glMapNamedBufferRange(3u, 0, 8, persistent_write));
  CHECK(mapped != nullptr);
  mapped[0] = 1u;
  gen::Wrap_glDrawArrays(gl_triangles, 0, 3);
  mapped[1] = 2u;
  gen::Wrap_glDrawElements(gl_triangles, 3, gl_unsigned_short, nullptr);
  mapped[2] = 3u;
  gen::Wrap_glDrawRangeElements(gl_triangles, 0u, 2u, 3, gl_unsigned_short, nullptr);
  gen::Wrap_glDrawArrays(gl_triangles, 0, 3);
  const std::uint32_t expected[] = {ids::kCmd_glGetNamedBufferParameteri64v, ids::kCmd_glGetNamedBufferSubData,
                                    ids::kCmd_glNamedBufferSubData,          ids::kCmd_glDrawArrays,
                                    ids::kCmd_glNamedBufferSubData,          ids::kCmd_glDrawElements,
                                    ids::kCmd_glNamedBufferSubData,          ids::kCmd_glDrawRangeElements,
                                    ids::kCmd_glDrawArrays};
  CHECK_EQ(current.sent(), 9u);
  for (std::uint32_t index = 0u; index < 9u; index++) CHECK_EQ(current.sent_since(index), expected[index]);
  const GLuint deleted[] = {3u};
  gen::Wrap_glDeleteBuffers(1, deleted);
  gen::Wrap_glDrawArrays(gl_triangles, 0, 3);
  CHECK_EQ(current.sent(), 11u);
  CHECK_EQ(current.sent_since(10u), ids::kCmd_glDrawArrays);
  CHECK_EQ(current.context().mappings().held(), 0u); }

TEST(manual_calls, a_maps_state_is_answered_by_the_guest)
{ Current current;
  av::ScriptedHost &host = current.rig().host();
  constexpr GLenum map_length = 0x9120u, map_offset = 0x9121u, access_flags = 0x911Fu, mapped_name = 0x88BCu,
                   access = 0x88BBu, buffer_size = 0x8764u;
  gen::Wrap_glBindBuffer(array_buffer, 7u);
  answer_size(host, 64u);
  host.gl_bytes(nullptr, 0u);
  const GLbitfield unsynchronized_write = 0x0002u | 0x0020u;
  CHECK(gen::Wrap_glMapBufferRange(array_buffer, 8, 32, unsynchronized_write) != nullptr);
  const std::uint32_t sent = current.sent();
  GLint value = -1;
  gen::Wrap_glGetBufferParameteriv(array_buffer, map_length, &value);
  CHECK_EQ(value, 32);
  GLint64 wide = -1;
  gen::Wrap_glGetBufferParameteri64v(array_buffer, map_offset, &wide);
  CHECK_EQ(wide, 8);
  gen::Wrap_glGetNamedBufferParameteriv(7u, access_flags, &value);
  CHECK_EQ(value, static_cast<GLint>(unsynchronized_write));
  gen::Wrap_glGetNamedBufferParameteri64v(7u, mapped_name, &wide);
  CHECK_EQ(wide, 1);
  gen::Wrap_glGetNamedBufferParameterivEXT(7u, access, &value);
  CHECK_EQ(value, static_cast<GLint>(0x88B9u));
  CHECK_EQ(current.sent(), sent);
  const std::int32_t host_size = 64;
  host.gl_bytes(reinterpret_cast<const std::uint8_t *>(&host_size), sizeof host_size);
  gen::Wrap_glGetBufferParameteriv(array_buffer, buffer_size, &value);
  CHECK_EQ(value, 64);
  CHECK_EQ(current.sent() - sent, 1u);
  CHECK_EQ(current.sent_since(sent), ids::kCmd_glGetBufferParameteriv);
  host.gl_varint(0u);
  CHECK_EQ(current.error(), 0u);
  gen::Wrap_glBindBuffer(array_buffer, 0u);
  gen::Wrap_glGetBufferParameteriv(array_buffer, map_length, &value);
  CHECK_EQ(current.error(), gl_invalid_operation);
  gen::Wrap_glGetBufferParameteriv(0x1234u, map_length, &value);
  CHECK_EQ(current.error(), gl_invalid_enum);
  const std::uint32_t before_zero = current.sent();
  const std::int32_t  host_zero = 0;
  host.gl_bytes(reinterpret_cast<const std::uint8_t *>(&host_zero), sizeof host_zero);
  gen::Wrap_glGetNamedBufferParameteriv(0u, map_length, &value);
  CHECK_EQ(current.sent() - before_zero, 1u);
  CHECK_EQ(current.sent_since(before_zero), ids::kCmd_glGetNamedBufferParameteriv); }

TEST(manual_calls, map_refusals_are_recorded)
{ Current current;
  CHECK(gen::Wrap_glMapBuffer(0x1234u, gl_read_write) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_enum);
  CHECK(!gen::Wrap_glUnmapBuffer(0x1234u));
  gen::Wrap_glFlushMappedBufferRange(0x1234u, 0, 1);
  void *pointer = reinterpret_cast<void *>(0x1u);
  gen::Wrap_glGetBufferPointerv(0x1234u, gl_buffer_map_pointer, &pointer);
  CHECK(pointer == reinterpret_cast<void *>(0x1u));
  CHECK_EQ(current.error(), gl_invalid_enum);
  CHECK_EQ(current.sent(), 0u);
  gen::Wrap_glBindBuffer(array_buffer, 7u);
  CHECK(gen::Wrap_glMapBuffer(array_buffer, 0x1234u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_enum);
  CHECK(gen::Wrap_glMapNamedBuffer(7u, 0x1234u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_enum);
  CHECK(gen::Wrap_glMapBufferRange(array_buffer, 0, -1, 1u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_value);
  CHECK(gen::Wrap_glMapNamedBufferRange(7u, 0, -1, 1u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_value);
  CHECK_EQ(current.sent(), 1u);
  gen::Wrap_glBindBuffer(array_buffer, 0u);
  gen::Wrap_glGetBufferPointerv(array_buffer, gl_buffer_map_pointer, &pointer);
  CHECK_EQ(current.error(), gl_invalid_operation);
  gen::Wrap_glGetBufferPointerv(array_buffer, 0x1234u, &pointer);
  CHECK_EQ(current.error(), gl_invalid_enum);
  gen::Wrap_glGetBufferPointerv(array_buffer, gl_buffer_map_pointer, nullptr);
  gen::Wrap_glGetNamedBufferPointerv(7u, 0x1234u, &pointer);
  CHECK_EQ(current.error(), gl_invalid_enum);
  gen::Wrap_glGetNamedBufferPointerv(7u, gl_buffer_map_pointer, nullptr);
  pointer = reinterpret_cast<void *>(0x1u);
  gen::Wrap_glGetNamedBufferPointerv(7u, gl_buffer_map_pointer, &pointer);
  CHECK(pointer == nullptr);
  CHECK(!gen::Wrap_glUnmapNamedBuffer(7u));
  CHECK_EQ(current.error(), gl_invalid_operation);
  gen::Wrap_glFlushMappedNamedBufferRange(7u, 0, 1);
  CHECK_EQ(current.error(), gl_invalid_operation);
  CHECK_EQ(current.sent(), 2u);
  const GLuint several[] = {7u};
  gen::Wrap_glBindBuffersBase(0x8A11u, 0u, 1, several);
  CHECK(!gen::Wrap_glUnmapBuffer(0x8A11u));
  CHECK_EQ(current.error(), gl_invalid_operation);
  CHECK_EQ(current.sent(), 3u);
  CHECK_EQ(current.sent_since(2u), ids::kCmd_glBindBuffersBase); }

TEST(manual_calls, the_ext_direct_state_calls_are_refused_without_a_host_call)
{ Current current;
  CHECK(gen::Wrap_glMapNamedBufferEXT(1u, 0x88B8u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_operation);
  CHECK(gen::Wrap_glMapNamedBufferRangeEXT(1u, 0, 4, 1u) == nullptr);
  CHECK(!gen::Wrap_glUnmapNamedBufferEXT(1u));
  gen::Wrap_glFlushMappedNamedBufferRangeEXT(1u, 0, 4);
  void *pointer = nullptr;
  gen::Wrap_glGetNamedBufferPointervEXT(1u, 0x88BDu, &pointer);
  CHECK_EQ(current.error(), gl_invalid_operation);
  gen::Wrap_glGetPointerIndexedvEXT(0u, 0u, &pointer);
  CHECK_EQ(current.error(), gl_invalid_operation);
  gen::Wrap_glGetPointeri_vEXT(0u, 0u, &pointer);
  CHECK_EQ(current.error(), gl_invalid_operation);
  gen::Wrap_glGetVertexArrayPointervEXT(1u, 0u, &pointer);
  CHECK_EQ(current.error(), gl_invalid_operation);
  gen::Wrap_glGetVertexArrayPointeri_vEXT(1u, 0u, 0u, &pointer);
  CHECK_EQ(current.error(), gl_invalid_operation);
  CHECK_EQ(current.sent(), 0u); }

TEST(manual_calls, the_debug_callback_is_kept_for_the_pointer_query)
{ Current current;
  int user = 0;
  gen::Wrap_glDebugMessageCallback(reinterpret_cast<GLDEBUGPROC>(0x4000u), &user);
  void *answer = nullptr;
  gen::Wrap_glGetPointerv(0x8244u, &answer);
  CHECK(answer == reinterpret_cast<void *>(0x4000u));
  gen::Wrap_glGetPointerv(0x8245u, &answer);
  CHECK(answer == &user);
  gen::Wrap_glGetPointerv(0x1234u, &answer);
  CHECK_EQ(current.error(), gl_invalid_enum);
  gen::Wrap_glGetPointerv(0x8244u, nullptr);
  CHECK_EQ(current.sent(), 0u); }

TEST(manual_calls, legacy_client_arrays_are_kept_by_the_guest_and_sent_with_the_draw_that_reads_them)
{ Current current;
  const float positions[8] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
  gen::Wrap_glEnableClientState(0x8074u);
  gen::Wrap_glVertexPointer(2, gl_float, 0, positions);
  CHECK_EQ(current.sent(), 0u);
  const GLuint scratch = 7u;
  current.rig().host().gl_bytes(reinterpret_cast<const std::uint8_t *>(&scratch), sizeof scratch);
  gen::Wrap_glDrawArrays(gl_triangles, 1, 3);
  const std::uint32_t first[] = {ids::kCmd_glGenBuffers,    ids::kCmd_glBindBuffer,       ids::kCmd_glBufferData,
                                 ids::kCmd_glBufferSubData, ids::kCmd_glVertexPointer,    ids::kCmd_glEnableClientState,
                                 ids::kCmd_glBindBuffer,    ids::kCmd_glDrawArrays};
  CHECK_EQ(current.sent(), 8u);
  for (std::uint32_t index = 0u; index < 8u; index++) CHECK_EQ(current.sent_since(index), first[index]);
  CHECK_EQ(current.argument_since(1u, 1u), 7u);
  CHECK_EQ(current.argument_since(3u, 1u), 16u);
  CHECK_EQ(current.argument_since(3u, 2u), 24u);
  gen::Wrap_glDrawArrays(gl_triangles, 0, 4);
  CHECK_EQ(current.sent(), 14u);
  CHECK_EQ(current.sent_since(8u), ids::kCmd_glBindBuffer);
  CHECK_EQ(current.sent_since(12u), ids::kCmd_glBindBuffer);
  CHECK_EQ(current.sent_since(13u), ids::kCmd_glDrawArrays);
  gen::Wrap_glDisableClientState(0x8074u);
  CHECK_EQ(current.sent_since(14u), ids::kCmd_glDisableClientState);
  gen::Wrap_glDrawArrays(gl_triangles, 0, 4);
  CHECK_EQ(current.sent(), 16u);
  CHECK_EQ(current.sent_since(15u), ids::kCmd_glDrawArrays); }

TEST(manual_calls, client_indices_are_staged_with_the_arrays_and_the_element_binding_is_restored)
{ Current current;
  const float positions[8] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
  const std::uint16_t indices[] = {0, 1, 2, 2, 3, 0};
  gen::Wrap_glEnableClientState(0x8074u);
  gen::Wrap_glVertexPointer(2, gl_float, 0, positions);
  const GLuint scratch = 7u;
  current.rig().host().gl_bytes(reinterpret_cast<const std::uint8_t *>(&scratch), sizeof scratch);
  gen::Wrap_glDrawElements(gl_triangles, 6, gl_unsigned_short, indices);
  const std::uint32_t expected[] = {ids::kCmd_glGenBuffers,    ids::kCmd_glBindBuffer,    ids::kCmd_glBufferData,
                                    ids::kCmd_glBufferSubData, ids::kCmd_glVertexPointer, ids::kCmd_glEnableClientState,
                                    ids::kCmd_glBufferSubData, ids::kCmd_glBindBuffer,    ids::kCmd_glBindBuffer,
                                    ids::kCmd_glDrawElements,  ids::kCmd_glBindBuffer};
  CHECK_EQ(current.sent(), 11u);
  for (std::uint32_t index = 0u; index < 11u; index++) CHECK_EQ(current.sent_since(index), expected[index]);
  CHECK_EQ(current.argument_since(8u, 0u), element_array_buffer);
  CHECK_EQ(current.argument_since(8u, 1u), 7u);
  CHECK_EQ(current.argument_since(10u, 0u), element_array_buffer);
  CHECK_EQ(current.argument_since(10u, 1u), 0u);
  CHECK_EQ(current.error(), 0u); }

TEST(manual_calls, client_indices_without_client_arrays_are_staged_alone)
{ Current current;
  const std::uint16_t indices[] = {0, 1, 2};
  const GLuint        scratch = 7u;
  current.rig().host().gl_bytes(reinterpret_cast<const std::uint8_t *>(&scratch), sizeof scratch);
  gen::Wrap_glDrawElements(gl_triangles, 3, gl_unsigned_short, indices);
  const std::uint32_t expected[] = {ids::kCmd_glGenBuffers,    ids::kCmd_glBindBuffer,   ids::kCmd_glBufferData,
                                    ids::kCmd_glBufferSubData, ids::kCmd_glBindBuffer,   ids::kCmd_glBindBuffer,
                                    ids::kCmd_glDrawElements,  ids::kCmd_glBindBuffer};
  CHECK_EQ(current.sent(), 8u);
  for (std::uint32_t index = 0u; index < 8u; index++) CHECK_EQ(current.sent_since(index), expected[index]);
  CHECK_EQ(current.error(), 0u); }

TEST(manual_calls, an_array_in_a_buffer_is_sent_at_once_and_a_client_array_is_dropped_from_the_host_at_begin)
{ Current current;
  const float colors[12] = {};
  gen::Wrap_glBindBuffer(array_buffer, 4u);
  gen::Wrap_glColorPointer(3, gl_float, 0, reinterpret_cast<const void *>(0x20u));
  gen::Wrap_glEnableClientState(0x8076u);
  const std::uint32_t expected[] = {ids::kCmd_glBindBuffer, ids::kCmd_glColorPointer, ids::kCmd_glEnableClientState};
  CHECK_EQ(current.sent(), 3u);
  for (std::uint32_t index = 0u; index < 3u; index++) CHECK_EQ(current.sent_since(index), expected[index]);
  gen::Wrap_glBindBuffer(array_buffer, 0u);
  gen::Wrap_glColorPointer(3, gl_float, 0, colors);
  gen::Wrap_glEnableClientState(0x8078u);
  CHECK_EQ(current.sent(), 4u);
  gen::Wrap_glTexCoordPointer(2, gl_float, 0, colors);
  CHECK_EQ(current.sent(), 4u);
  const GLuint scratch = 7u;
  current.rig().host().gl_bytes(reinterpret_cast<const std::uint8_t *>(&scratch), sizeof scratch);
  gen::Wrap_glDrawArrays(gl_triangles, 0, 2);
  const std::uint32_t before = current.sent();
  gen::Wrap_glBegin(gl_triangles);
  CHECK_EQ(current.sent(), before + 3u);
  CHECK_EQ(current.sent_since(before), ids::kCmd_glDisableClientState);
  CHECK_EQ(current.sent_since(before + 2u), ids::kCmd_glBegin);
  gen::Wrap_glArrayElement(1);
  CHECK_EQ(current.sent(), before + 5u);
  CHECK_EQ(current.sent_since(before + 3u), ids::kCmd_glMultiTexCoord4fv);
  CHECK_EQ(current.sent_since(before + 4u), ids::kCmd_glColor4fv); }

TEST(manual_calls, client_array_errors_are_recorded_without_a_host_call)
{ Current current;
  const float data[4] = {};
  gen::Wrap_glVertexPointer(2, gl_float, -1, data);
  CHECK_EQ(current.error(), gl_invalid_value);
  gen::Wrap_glVertexPointer(1, gl_float, 0, data);
  CHECK_EQ(current.error(), gl_invalid_value);
  gen::Wrap_glVertexPointer(2, 0x1401u, 0, data);
  CHECK_EQ(current.error(), gl_invalid_enum);
  gen::Wrap_glEnableClientState(0x1234u);
  CHECK_EQ(current.error(), gl_invalid_enum);
  gen::Wrap_glClientActiveTexture(0x1234u);
  CHECK_EQ(current.error(), gl_invalid_enum);
  gen::Wrap_glInterleavedArrays(0x1234u, 0, data);
  CHECK_EQ(current.error(), gl_invalid_enum);
  gen::Wrap_glInterleavedArrays(0x2A20u, -1, data);
  CHECK_EQ(current.error(), gl_invalid_value);
  gen::Wrap_glDrawArraysIndirect(gl_triangles, nullptr);
  CHECK_EQ(current.sent(), 1u);
  gen::Wrap_glEnableClientState(0x8074u);
  gen::Wrap_glVertexPointer(2, gl_float, 0, data);
  gen::Wrap_glDrawArraysIndirect(gl_triangles, nullptr);
  CHECK_EQ(current.error(), gl_invalid_operation);
  CHECK_EQ(current.sent(), 1u); }

TEST(manual_calls, interleaved_arrays_set_every_array_of_the_format)
{ Current current;
  const float data[12] = {};
  const GLuint scratch = 7u;
  gen::Wrap_glInterleavedArrays(0x2A26u, 0, data);
  CHECK_EQ(current.sent(), 0u);
  current.rig().host().gl_bytes(reinterpret_cast<const std::uint8_t *>(&scratch), sizeof scratch);
  gen::Wrap_glDrawArrays(gl_triangles, 0, 1);
  std::uint32_t enables = 0u, pointers = 0u;
  const std::uint32_t total = current.sent();
  for (std::uint32_t index = 0u; index < total; index++) {
    const std::uint32_t id = current.sent_since(index);
    enables += id == ids::kCmd_glEnableClientState ? 1u : 0u;
    pointers += id == ids::kCmd_glVertexPointer || id == ids::kCmd_glColorPointer || id == ids::kCmd_glNormalPointer ? 1u : 0u;
  }
  CHECK_EQ(enables, 3u);
  CHECK_EQ(pointers, 3u); }

TEST(manual_calls, with_nothing_current_nothing_is_sent_or_answered)
{ GlIcdRig rig;
  rig.load_with_format();
  ManualCalls &calls = rig.root().manual();
  GLuint name = 0u;
  void  *pointer = reinterpret_cast<void *>(0x1u);
  GLint  value = 7;
  calls.pixel_store(0x0D05u, 1);
  calls.pixel_store(0x0D05u, 1.0f);
  calls.bind_buffer(array_buffer, 1u);
  calls.delete_buffers(1, &name);
  calls.gen_buffers(1, &name);
  calls.gen_textures(1, &name);
  calls.gen_vertex_arrays(1, &name);
  calls.bind_vertex_array(1u);
  calls.delete_vertex_arrays(1, &name);
  calls.buffer_data(array_buffer, 0, nullptr, 0u);
  calls.buffer_storage(array_buffer, 0, nullptr, 0u);
  calls.named_buffer_data(1u, 0, nullptr, 0u);
  calls.named_buffer_storage(1u, 0, nullptr, 0u);
  calls.vertex_attrib_pointer(0u, 1, gl_float, 0u, 0, nullptr);
  calls.enable_vertex_attrib_array(0u, true);
  calls.draw_arrays(gl_triangles, 0, 3, -1);
  calls.draw_elements(gl_triangles, 3, gl_unsigned_short, nullptr, -1);
  calls.draw_range_elements(gl_triangles, 0u, 2u, 3, gl_unsigned_short, nullptr);
  calls.flush();
  CHECK_EQ(calls.client_wait_sync(nullptr, 0u, 0u), 0x911Du);
  calls.debug_message_callback(nullptr, nullptr);
  calls.link_program(1u);
  calls.delete_program(1u);
  CHECK_EQ(calls.attrib_location(1u, "a"), -1);
  CHECK_EQ(calls.uniform_location(1u, "a"), -1);
  calls.integers(0x0BA2u, &value);
  CHECK_EQ(value, 7);
  CHECK(calls.string(0x1F00u) == nullptr);
  CHECK(calls.string_i(0x1F03u, 0u) == nullptr);
  CHECK(calls.map_buffer(array_buffer, 0x88B8u) == nullptr);
  CHECK(calls.map_buffer_range(array_buffer, 0, 1, 1u) == nullptr);
  CHECK(calls.map_named_buffer(1u, 0x88B8u) == nullptr);
  CHECK(calls.map_named_buffer_range(1u, 0, 1, 1u) == nullptr);
  CHECK(!calls.unmap_buffer(array_buffer));
  CHECK(!calls.unmap_named_buffer(1u));
  calls.flush_mapped_range(array_buffer, 0, 1);
  calls.flush_mapped_named_range(1u, 0, 1);
  calls.refuse();
  calls.buffer_pointer(array_buffer, 0x88BDu, &pointer);
  calls.named_buffer_pointer(1u, 0x88BDu, &pointer);
  std::int64_t state = 7;
  CHECK(!calls.buffer_map_state(array_buffer, 0x9120u, state));
  CHECK(!calls.named_buffer_map_state(1u, 0x9120u, state));
  CHECK_EQ(state, 7);
  calls.attribute_pointer(0u, 0x8645u, &pointer);
  calls.pointer(0x8244u, &pointer);
  CHECK(pointer == reinterpret_cast<void *>(0x1u));
  calls.buffer_sub_data(array_buffer, 0, 1, &value);
  calls.named_buffer_sub_data(1u, 0, 1, &value);
  calls.named_buffer_sub_data_ext(1u, 0, 1, &value);
  calls.get_buffer_sub_data(array_buffer, 0, 1, &value);
  calls.get_named_buffer_sub_data(1u, 0, 1, &value);
  calls.get_named_buffer_sub_data_ext(1u, 0, 1, &value);
  calls.named_buffer_data_ext(1u, 1, &value, 0x88E4u);
  calls.named_buffer_storage_ext(1u, 1, &value, 0u);
  CHECK(!calls.stage_upload(&value, std::size_t{1} << 20u));
  CHECK(!calls.stage_download(&value, std::size_t{1} << 20u));
  calls.finish_upload();
  calls.finish_download(&value, 1u);
  calls.host_writes_target(array_buffer);
  calls.host_writes_buffer(1u);
  calls.bind_buffer_indexed(0x90D2u, 1u);
  calls.bind_buffers_indexed(0x90D2u);
  calls.shaders_write_buffers();
  calls.client_attributes_popped();
  CHECK_EQ(rig.kmt().commands(), 0u); }

TEST(manual_calls, a_lost_context_sends_nothing_but_still_answers_what_it_kept)
{ Current current;
  current.rig().kmt().withhold_answer_at(1u);
  GLint value = 0;
  gen::Wrap_glGetIntegerv(0x0BA2u, &value);
  const std::uint32_t sent = current.sent();
  gen::Wrap_glDrawArrays(gl_triangles, 0, 3);
  gen::Wrap_glBindBuffer(array_buffer, 3u);
  CHECK_EQ(current.context().arrays().bound_buffer(array_buffer), 0u);
  gen::Wrap_glDebugMessageCallback(reinterpret_cast<GLDEBUGPROC>(0x4000u), nullptr);
  void *answer = nullptr;
  gen::Wrap_glGetPointerv(0x8244u, &answer);
  CHECK(answer == reinterpret_cast<void *>(0x4000u));
  CHECK(std::strcmp(reinterpret_cast<const char *>(gen::Wrap_glGetString(0x1F01u)), "host gpu") == 0);
  CHECK_EQ(current.sent(), sent); }

TEST(gl_error_query, asks_the_host_and_answers_no_error_when_the_call_fails)
{ Current current;
  av::ClientTransport &wire = *current.context().wire();
  current.rig().host().gl_varint(0x0505u);
  CHECK_EQ(av::glicd::GlErrorQuery(wire).ask(), 0x0505u);
  const std::uint8_t nothing[1] = {0};
  current.rig().kmt().queue_answer(nothing, 0u);
  CHECK_EQ(av::glicd::GlErrorQuery(wire).ask(), 0u);
  current.rig().kmt().withhold_answer_at(1u);
  CHECK_EQ(av::glicd::GlErrorQuery(wire).ask(), 0u);
  CHECK_EQ(av::glicd::GlErrorQuery(wire).ask(), 0u); }

TEST(manual_calls, the_extension_count_is_answered_without_the_host)
{ Current current;
  GLint count = 9;
  av::glicd::entry::get_integerv(0x821Du , &count);
  CHECK_EQ(count, 0);
  av::glicd::entry::get_integerv(0x821Du, nullptr);
  CHECK_EQ(current.sent(), 0u); }

TEST(manual_calls, the_extensions_answered_are_the_context_s_list)
{ static constexpr av::glicd::GlExtensionPolicy::Row rows[] = {
      {"GL_ARB_debug_output", 1u, "manual_calls.the_extensions_answered_are_the_context_s_list"}};
  const av::glicd::GlExtensionPolicy policy(rows, 1u, nullptr, 0u);
  Bare bare(policy);
  av::GlContextRig &rig = bare.rig;
  av::glicd::CurrentContexts &currents = bare.currents;
  ManualCalls &calls = bare.calls;
  av::glicd::GuestGlContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.groups(), rig.lines(),
                                    policy, rig.handles(), 1, av::GlContextOptions());
  CHECK(calls.string_i(0x1F03u, 0u) == nullptr);
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_integer(2);
  rig.host().gl_string("GL_EXT_host_only");
  rig.host().gl_string("GL_ARB_debug_output");
  rig.host().gl_slices();
  CHECK_STATUS(context.make(), av::Status::ok);
  CHECK(currents.make_current(context));
  const std::uint32_t commands = rig.kmt().commands();
  GLint count = 0;
  calls.integers(0x821Du, &count);
  CHECK_EQ(count, 1);
  CHECK(std::strcmp(reinterpret_cast<const char *>(calls.string(0x1F03u)), "GL_ARB_debug_output") == 0);
  CHECK(std::strcmp(reinterpret_cast<const char *>(calls.string_i(0x1F03u, 0u)), "GL_ARB_debug_output") == 0);
  CHECK_EQ(context.take_error(), 0u);
  CHECK_EQ(rig.kmt().commands(), commands);
  CHECK(calls.string_i(0x1F03u, 1u) == nullptr);
  CHECK_EQ(context.take_error(), gl_invalid_value);
  CHECK(calls.string_i(0x1F00u, 0u) == nullptr);
  CHECK_EQ(context.take_error(), gl_invalid_enum);
  currents.release(); }

TEST(manual_calls, the_window_is_translated_on_the_way_to_the_host_and_back)
{ Current current;
  constexpr GLenum gl_framebuffer = 0x8D40u, gl_draw_framebuffer = 0x8CA9u, gl_back = 0x0405u;
  constexpr GLenum gl_color_attachment0 = 0x8CE0u, gl_draw_framebuffer_binding = 0x8CA6u, gl_draw_buffer = 0x0C01u;
  gen::Wrap_glBindFramebuffer(gl_framebuffer, 0u);
  gen::Wrap_glDrawBuffer(gl_back);
  gen::Wrap_glReadBuffer(gl_back);
  gen::Wrap_glBindFramebuffer(gl_draw_framebuffer, 7u);
  gen::Wrap_glDrawBuffer(gl_back);
  gen::Wrap_glReadBuffer(gl_back);
  CHECK_EQ(current.sent(), 6u);
  CHECK_EQ(current.sent_since(0u), ids::kCmd_glBindFramebuffer);
  CHECK_EQ(current.argument_since(0u, 1u), 100u);
  CHECK_EQ(current.sent_since(1u), ids::kCmd_glDrawBuffer);
  CHECK_EQ(current.argument_since(1u, 0u), gl_color_attachment0);
  CHECK_EQ(current.sent_since(2u), ids::kCmd_glReadBuffer);
  CHECK_EQ(current.argument_since(2u, 0u), gl_color_attachment0);
  CHECK_EQ(current.argument_since(3u, 1u), 7u);
  CHECK_EQ(current.argument_since(4u, 0u), gl_back);
  CHECK_EQ(current.argument_since(5u, 0u), gl_color_attachment0);
  current.rig().host().gl_integer(7);
  GLint answer = -1;
  av::glicd::entry::get_integerv(gl_draw_framebuffer_binding, &answer);
  CHECK_EQ(answer, 7);
  gen::Wrap_glBindFramebuffer(gl_framebuffer, 0u);
  current.rig().host().gl_integer(100);
  av::glicd::entry::get_integerv(gl_draw_framebuffer_binding, &answer);
  CHECK_EQ(answer, 0);
  current.rig().host().gl_integer(static_cast<std::int32_t>(gl_color_attachment0));
  av::glicd::entry::get_integerv(gl_draw_buffer, &answer);
  CHECK_EQ(answer, static_cast<GLint>(gl_back));
  current.rig().host().gl_integer(0);
  av::glicd::entry::get_integerv(gl_draw_buffer, nullptr);
  CHECK_EQ(current.rig().kmt().queued_answers(), 0u);
  current.rig().host().gl_varint(0u);
  CHECK_EQ(current.error(), 0u); }

TEST(manual_calls, window_calls_with_nothing_current_send_nothing)
{ GlIcdRig rig;
  rig.load_with_format();
  gen::Wrap_glBindFramebuffer(0x8D40u, 0u);
  gen::Wrap_glDrawBuffer(0x0405u);
  gen::Wrap_glReadBuffer(0x0405u);
  CHECK_EQ(rig.kmt().commands(), 0u); }

TEST(manual_calls, a_level_download_is_sized_by_the_hosts_level)
{ Current current;
  av::ScriptedHost &host = current.rig().host();
  host.gl_integer(3);
  host.gl_integer(2);
  host.gl_integer(1);
  const std::uint32_t before = current.sent();
  std::uint8_t pixels[24] = {};
  host.gl_bytes(nullptr, 0u);
  gen::Wrap_glGetTexImage(0x0DE1u, 1, 0x1908u , 0x1401u, pixels);
  CHECK_EQ(current.sent() - before, 4u);
  CHECK_EQ(current.sent_since(before), ids::kCmd_glGetTexLevelParameteriv);
  CHECK_EQ(current.argument_since(before, 1u), 1u);
  CHECK_EQ(current.argument_since(before, 2u), 0x1000u);
  CHECK_EQ(current.argument_since(before + 1u, 2u), 0x1001u);
  CHECK_EQ(current.argument_since(before + 2u, 2u), 0x8071u);
  CHECK_EQ(current.sent_since(before + 3u), ids::kCmd_glGetTexImage);
  CHECK_EQ(current.context().arrays().pixel_bytes(0x1908u, 0x1401u, 3, 2, 1, true), 24u);
  host.gl_integer(4);
  host.gl_integer(-1);
  host.gl_integer(1);
  CHECK_EQ(current.rig().root().manual().level_bytes(0x0DE1u, 0, 0x1908u, 0x1401u), 0u);
  host.gl_integer(77);
  CHECK_EQ(current.rig().root().manual().compressed_level_bytes(0x0DE1u, 2), 77u);
  host.gl_integer(-5);
  CHECK_EQ(current.rig().root().manual().compressed_level_bytes(0x0DE1u, 2), 0u);
  host.gl_integer(16);
  host.gl_bytes(nullptr, 0u);
  gen::Wrap_glGetCompressedTexImage(0x0DE1u, 0, pixels);
  CHECK_EQ(current.sent_since(current.sent() - 1u), ids::kCmd_glGetCompressedTexImage);
  CHECK_EQ(current.error(), 0u); }

TEST(manual_calls, a_level_download_with_nothing_current_is_sized_nothing)
{ GlIcdRig rig;
  rig.load_with_format();
  CHECK_EQ(rig.root().manual().level_bytes(0x0DE1u, 0, 0x1908u, 0x1401u), 0u);
  CHECK_EQ(rig.root().manual().compressed_level_bytes(0x0DE1u, 0), 0u);
  CHECK_EQ(rig.kmt().commands(), 0u); }

TEST(manual_calls, every_map_access_enum_and_refusal_reaches_the_mapping)
{ Current current;
  av::ScriptedHost &host = current.rig().host();
  gen::Wrap_glBindBuffer(array_buffer, 7u);
  answer_size(host, 4);
  host.gl_bytes(nullptr, 0u);
  CHECK(gen::Wrap_glMapBuffer(array_buffer, 0x88B8u) != nullptr);
  CHECK_EQ(current.context().mappings().held(), 1u);
  answer_size(host, 4);
  host.gl_bytes(nullptr, 0u);
  CHECK(gen::Wrap_glMapNamedBuffer(8u, 0x88B9u) != nullptr);
  CHECK_EQ(current.sent(), 5u);
  CHECK_EQ(current.sent_since(3u), ids::kCmd_glGetNamedBufferParameteri64v);
  CHECK_EQ(current.sent_since(4u), ids::kCmd_glGetNamedBufferSubData);
  CHECK(gen::Wrap_glMapBufferRange(0x1234u, 0, 4, 1u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_enum);
  CHECK(gen::Wrap_glMapNamedBufferRange(9u, 0, 4, 0u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_operation);
  gen::Wrap_glBindBuffer(array_buffer, 10u);
  CHECK(!gen::Wrap_glUnmapBuffer(array_buffer));
  CHECK_EQ(current.error(), gl_invalid_operation);
  CHECK_EQ(current.sent(), 6u);
  CHECK_EQ(current.context().mappings().held(), 2u); }

TEST(manual_calls, the_generated_entries_hooks_follow_the_current_context)
{ { GlIcdRig rig;
    rig.load_with_format();
    CHECK_EQ(gen::Wrap_glCheckNamedFramebufferStatus(0u, 0x8D40u), 0u);
    const GLenum buffers[] = {0x8CE0u};
    gen::Wrap_glDrawBuffers(1, buffers);
    CHECK_EQ(rig.kmt().commands(), 0u); }
  Current current;
  av::ScriptedHost &host = current.rig().host();
  host.gl_varint(0x8CD5u);
  CHECK_EQ(gen::Wrap_glCheckNamedFramebufferStatus(0u, 0x8D40u), 0x8CD5u);
  CHECK_EQ(current.sent_since(0u), ids::kCmd_glCheckNamedFramebufferStatus);
  CHECK_EQ(current.argument_since(0u, 0u), 100u);
  answer_size(host, 4);
  host.gl_bytes(nullptr, 0u);
  auto *const mapped = static_cast<std::uint8_t *>(gen::Wrap_glMapNamedBufferRange(3u, 0, 4, 0x0002u | 0x0040u));
  CHECK(mapped != nullptr);
  mapped[0] = 1u;
  const GLenum buffers[] = {0x8CE0u};
  gen::Wrap_glDrawBuffers(1, buffers);
  CHECK_EQ(current.sent(), 5u);
  CHECK_EQ(current.sent_since(3u), ids::kCmd_glNamedBufferSubData);
  CHECK_EQ(current.sent_since(4u), ids::kCmd_glDrawBuffers);
  current.rig().kmt().withhold_answer_at(1u);
  GLint value = 0;
  gen::Wrap_glGetIntegerv(0x0BA2u, &value);
  const std::uint32_t lost = current.sent();
  gen::Wrap_glDrawBuffers(1, buffers);
  CHECK_EQ(current.sent(), lost); }

TEST(manual_calls, an_imported_objects_name_is_the_hosts_by_the_time_it_crosses)
{ const av::glicd::GlExtensionPolicy policy(nullptr, 0u, nullptr, 0u);
  Bare bare(policy);
  av::GlContextRig &rig = bare.rig;
  av::glicd::CurrentContexts &currents = bare.currents;
  ManualCalls &calls = bare.calls;
  av::glicd::GuestGlContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.groups(), rig.lines(),
                                    policy, rig.handles(), 1, av::GlContextOptions());
  rig.host().gl_made("host gpu", "4.6");
  CHECK_STATUS(context.make(), av::Status::ok);
  CHECK(currents.make_current(context));

  const std::uint32_t quiet = rig.kmt().commands();
  GLuint              memory = 0u;
  calls.create_memory_objects(1, &memory);
  CHECK(memory != 0u);
  CHECK_EQ(rig.kmt().commands(), quiet);
  CHECK_EQ(context.take_error(), 0u);

  calls.buffer_storage_mem(0x8892u, 4096, memory, 0u);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  CHECK_EQ(rig.kmt().commands(), quiet);

  calls.import_memory_win32(memory, 4096u, 0x9587u, reinterpret_cast<void *>(0xDEADu));
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  CHECK_EQ(rig.kmt().commands(), quiet);

  GLuint semaphore = 0u;
  calls.gen_semaphores(1, &semaphore);
  CHECK(semaphore != 0u);
  calls.wait_semaphore(semaphore, 0u, nullptr, 0u, nullptr, nullptr);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  calls.signal_semaphore(memory, 0u, nullptr, 0u, nullptr, nullptr);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  CHECK_EQ(rig.kmt().commands(), quiet);

  calls.delete_memory_objects(1, &memory);
  calls.delete_semaphores(1, &semaphore);
  CHECK_EQ(rig.kmt().commands(), quiet);
  CHECK_EQ(context.take_error(), 0u); }

TEST(manual_calls, the_external_object_entries_do_nothing_with_no_context)
{ Bare bare;
  ManualCalls &calls = bare.calls;
  GLuint names[1] = {9u};
  calls.create_memory_objects(1, names);
  CHECK_EQ(names[0], 9u);
  calls.gen_semaphores(1, names);
  CHECK_EQ(names[0], 9u);
  calls.import_memory_win32(1u, 4096u, 0x9587u, nullptr);
  calls.import_semaphore_win32(1u, 0x9587u, nullptr);
  calls.delete_memory_objects(1, names);
  calls.delete_semaphores(1, names);
  calls.buffer_storage_mem(0x8892u, 4096, 1u, 0u);
  calls.texture_storage_mem_2d(1u, 1, 0x8058u, 4, 4, 1u, 0u);
  calls.signal_semaphore(1u, 0u, nullptr, 0u, nullptr, nullptr);
  calls.wait_semaphore(1u, 0u, nullptr, 0u, nullptr, nullptr);
  CHECK(!calls.is_memory_object(1u));
  CHECK(!calls.is_semaphore(1u));
  const GLint value = 1;
  const GLuint64 wide = 1u;
  GLint read = 7;
  GLuint64 got = 7u;
  calls.memory_object_parameter(1u, av::glicd::GlExternalObjects::dedicated, &value);
  calls.get_memory_object_parameter(1u, av::glicd::GlExternalObjects::dedicated, &read);
  CHECK_EQ(read, 7);
  calls.semaphore_parameter(1u, av::glicd::GlExternalObjects::d3d12_fence_value, &wide);
  calls.get_semaphore_parameter(1u, av::glicd::GlExternalObjects::d3d12_fence_value, &got);
  CHECK_EQ(got, 7u);
  calls.import_memory_win32_name(1u, 4096u, 0x9587u, "shared");
  calls.import_semaphore_win32_name(1u, 0x9587u, "shared");
  calls.named_buffer_storage_mem(1u, 4096, 1u, 0u);
  calls.tex_storage_mem_1d(0x0DE0u, 1, 0x8058u, 4, 1u, 0u);
  calls.tex_storage_mem_2d(0x0DE1u, 1, 0x8058u, 4, 4, 1u, 0u);
  calls.tex_storage_mem_2d_multisample(0x9100u, 4, 0x8058u, 4, 4, true, 1u, 0u);
  calls.tex_storage_mem_3d(0x806Fu, 1, 0x8058u, 4, 4, 2, 1u, 0u);
  calls.tex_storage_mem_3d_multisample(0x9102u, 4, 0x8058u, 4, 4, 2, true, 1u, 0u);
  calls.texture_storage_mem_1d(1u, 1, 0x8058u, 4, 1u, 0u);
  calls.texture_storage_mem_2d_multisample(1u, 4, 0x8058u, 4, 4, false, 1u, 0u);
  calls.texture_storage_mem_3d(1u, 1, 0x8058u, 4, 4, 2, 1u, 0u);
  calls.texture_storage_mem_3d_multisample(1u, 4, 0x8058u, 4, 4, 2, false, 1u, 0u); }

TEST(manual_calls, an_import_puts_the_hosts_name_on_the_wire_and_a_delete_releases_the_object)
{ const av::glicd::GlExtensionPolicy policy(nullptr, 0u, nullptr, 0u);
  Bare bare(policy);
  av::GlContextRig &rig = bare.rig;
  av::glicd::CurrentContexts &currents = bare.currents;
  ManualCalls &calls = bare.calls;
  av::glicd::GuestGlContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.groups(), rig.lines(),
                                    policy, rig.handles(), 1, av::GlContextOptions());
  rig.host().gl_made("host gpu", "4.6");
  CHECK_STATUS(context.make(), av::Status::ok);
  CHECK(currents.make_current(context));

  std::uint32_t adapter = 0u;
  std::uint32_t found = 0u;
  CHECK_STATUS(rig.kmt().enumerate_adapters(&adapter, 1u, found), av::Status::ok);
  std::uint32_t elsewhere = 0u;
  CHECK_STATUS(rig.kmt().create_device(adapter, elsewhere), av::Status::ok);
  const av::AllocationDescriptor carrier(av::AllocationKind::client, true, av::SurfaceLayout(), 4096u,
                                         av::ScanoutTarget());
  std::uint8_t bytes[av::AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size] = {};
  CHECK_STATUS(carrier.encode(bytes, av::AllocationDescriptor::encoded_size), av::Status::ok);
  const av::SharedObjectRecord asking(0u, av::SharedObjectKind(av::SharedObjectKind::memory), 0u, 4096u);
  CHECK_STATUS(asking.encode(bytes + av::AllocationDescriptor::encoded_size,
                             av::SharedObjectRecord::encoded_size),
               av::Status::ok);
  std::uint32_t allocation = 0u;
  CHECK_STATUS(rig.kmt().create_shared_allocation(elsewhere, bytes, sizeof bytes, allocation), av::Status::ok);
  std::uint64_t sent = 0u;
  CHECK_STATUS(rig.kmt().share_object(elsewhere, allocation, sent), av::Status::ok);

  GLuint memory = 0u;
  calls.create_memory_objects(1, &memory);
  constexpr std::uint32_t host_name = 0x77u;
  rig.host().shared_object(0u, host_name);
  calls.import_memory_win32(memory, 4096u, 0x9587u, reinterpret_cast<void *>(static_cast<std::uintptr_t>(sent)));
  CHECK_EQ(context.take_error(), 0u);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), av::CommandHeader::open_shared_object_id);

  calls.buffer_storage_mem(0x8892u, 4096, memory, 0u);
  CHECK_EQ(context.take_error(), 0u);
  context.flush();
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u),
           static_cast<std::uint32_t>(ids::kCmd_glBufferStorageMemEXT));
  { av::WireReader body(rig.kmt().command_bytes(rig.kmt().commands() - 1u) + av::CommandHeader::encoded_size,
                        av::MockKmt::journal_bytes - av::CommandHeader::encoded_size);
    std::uint64_t target = 0u;
    std::uint64_t size = 0u;
    std::uint64_t named = 0u;
    CHECK(body.read_varint(target));
    CHECK(body.read_varint(size));
    CHECK(body.read_varint(named));
    CHECK_EQ(named, host_name);
    CHECK(named != memory); }

  rig.host().shared_object(0u, 0u);
  calls.delete_memory_objects(1, &memory);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), av::CommandHeader::release_shared_object_id);
  calls.buffer_storage_mem(0x8892u, 4096, memory, 0u);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation}); }

namespace {
void *openable_handle(av::GlContextRig &rig)
{
    std::uint32_t adapter = 0u;
    std::uint32_t found = 0u;
    CHECK_STATUS(rig.kmt().enumerate_adapters(&adapter, 1u, found), av::Status::ok);
    std::uint32_t device = 0u;
    CHECK_STATUS(rig.kmt().create_device(adapter, device), av::Status::ok);
    const av::AllocationDescriptor descriptor(av::AllocationKind::client, true, av::SurfaceLayout(),
                                              av::SharedObjectRecord::page_size, av::ScanoutTarget());
    std::uint8_t bytes[av::AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size] = {};
    CHECK_STATUS(descriptor.encode(bytes, av::AllocationDescriptor::encoded_size), av::Status::ok);
    const av::SharedObjectRecord asking(0u, av::SharedObjectKind(av::SharedObjectKind::memory), 0u, 4096u);
    CHECK_STATUS(asking.encode(bytes + av::AllocationDescriptor::encoded_size,
                               av::SharedObjectRecord::encoded_size),
                 av::Status::ok);
    std::uint32_t allocation = 0u;
    CHECK_STATUS(rig.kmt().create_shared_allocation(device, bytes, sizeof bytes, allocation), av::Status::ok);
    std::uint64_t named = 0u;
    CHECK_STATUS(rig.kmt().share_object(device, allocation, named), av::Status::ok);
    return reinterpret_cast<void *>(static_cast<std::uintptr_t>(named));
}

}

TEST(manual_calls, an_imported_name_crosses_as_the_hosts_own_on_every_command_that_takes_one)
{ const av::glicd::GlExtensionPolicy policy(nullptr, 0u, nullptr, 0u);
  Bare bare(policy);
  av::GlContextRig &rig = bare.rig;
  av::glicd::CurrentContexts &currents = bare.currents;
  ManualCalls &calls = bare.calls;
  av::glicd::GuestGlContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.groups(), rig.lines(),
                                    policy, rig.handles(), 1, av::GlContextOptions());
  rig.host().gl_made("host gpu", "4.6");
  CHECK_STATUS(context.make(), av::Status::ok);
  CHECK(currents.make_current(context));

  constexpr std::uint32_t host_memory = 0x4200u;
  constexpr std::uint32_t host_semaphore = 0x4300u;
  constexpr std::uint32_t opaque_win32 = av::glicd::GlExternalObjects::opaque_win32;

  GLuint memory = 0u;
  calls.create_memory_objects(1, &memory);
  CHECK(memory != 0u);
  rig.host().shared_object(0u, host_memory);
  calls.import_memory_win32(memory, 4096u, opaque_win32, openable_handle(rig));
  CHECK_EQ(context.take_error(), 0u);

  GLuint semaphore = 0u;
  calls.gen_semaphores(1, &semaphore);
  CHECK(semaphore != 0u);
  rig.host().shared_object(0u, host_semaphore);
  calls.import_semaphore_win32(semaphore, opaque_win32, openable_handle(rig));
  CHECK_EQ(context.take_error(), 0u);

  context.flush();
  std::uint32_t before = rig.kmt().commands();
  CHECK(context.shadows().trusted());
  calls.buffer_storage_mem(array_buffer, 4096, memory, 0u);
  CHECK_EQ(context.take_error(), 0u);
  CHECK(!context.shadows().trusted());
  context.flush();
  CHECK(rig.kmt().commands() > before);

  context.flush();
  before = rig.kmt().commands();
  calls.texture_storage_mem_2d(1u, 1, 0x8058u, 16, 16, memory, 0u);
  CHECK_EQ(context.take_error(), 0u);
  context.flush();
  CHECK(rig.kmt().commands() > before);

  context.flush();
  before = rig.kmt().commands();
  calls.wait_semaphore(semaphore, 0u, nullptr, 0u, nullptr, nullptr);
  CHECK_EQ(context.take_error(), 0u);
  context.flush();
  CHECK(rig.kmt().commands() > before);

  context.flush();
  before = rig.kmt().commands();
  calls.signal_semaphore(semaphore, 0u, nullptr, 0u, nullptr, nullptr);
  CHECK_EQ(context.take_error(), 0u);
  context.flush();
  CHECK(rig.kmt().commands() > before); }

TEST(manual_calls, every_storage_command_names_the_hosts_memory_object)
{ const av::glicd::GlExtensionPolicy policy(nullptr, 0u, nullptr, 0u);
  Bare bare(policy);
  av::GlContextRig &rig = bare.rig;
  av::glicd::CurrentContexts &currents = bare.currents;
  ManualCalls &calls = bare.calls;
  av::glicd::GuestGlContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.groups(), rig.lines(),
                                    policy, rig.handles(), 1, av::GlContextOptions());
  rig.host().gl_made("host gpu", "4.6");
  CHECK_STATUS(context.make(), av::Status::ok);
  CHECK(currents.make_current(context));
  const auto sent_id = [&](std::uint32_t at) {
      context.flush();
      return rig.kmt().command_id(at);
  };

  constexpr std::uint32_t host_memory = 0x4200u;
  GLuint memory = 0u;
  calls.create_memory_objects(1, &memory);
  rig.host().shared_object(0u, host_memory);
  calls.import_memory_win32(memory, 4096u, av::glicd::GlExternalObjects::opaque_win32,
                            openable_handle(rig));
  CHECK_EQ(context.take_error(), 0u);
  const GLuint stranger = memory + 0x1000u;

  struct Sent {
    const char *what;
    std::uint32_t id;
  };
  std::uint32_t before = 0u;
  const std::uint32_t named_at = rig.kmt().commands();
  CHECK(context.shadows().trusted());
  calls.named_buffer_storage_mem(1u, 4096, memory, 0u);
  CHECK_EQ(context.take_error(), 0u);
  CHECK(!context.shadows().trusted());
  CHECK_EQ(sent_id(named_at), static_cast<std::uint32_t>(ids::kCmd_glNamedBufferStorageMemEXT));

  before = rig.kmt().commands();
  calls.tex_storage_mem_1d(0x0DE0u, 1, 0x8058u, 16, memory, 0u);
  CHECK_EQ(sent_id(before), static_cast<std::uint32_t>(ids::kCmd_glTexStorageMem1DEXT));
  before = rig.kmt().commands();
  calls.tex_storage_mem_2d(0x0DE1u, 1, 0x8058u, 16, 16, memory, 0u);
  CHECK_EQ(sent_id(before), static_cast<std::uint32_t>(ids::kCmd_glTexStorageMem2DEXT));
  before = rig.kmt().commands();
  calls.tex_storage_mem_2d_multisample(0x9100u, 4, 0x8058u, 16, 16, true, memory, 0u);
  CHECK_EQ(sent_id(before),
           static_cast<std::uint32_t>(ids::kCmd_glTexStorageMem2DMultisampleEXT));
  before = rig.kmt().commands();
  calls.tex_storage_mem_3d(0x806Fu, 1, 0x8058u, 16, 16, 4, memory, 0u);
  CHECK_EQ(sent_id(before), static_cast<std::uint32_t>(ids::kCmd_glTexStorageMem3DEXT));
  before = rig.kmt().commands();
  calls.tex_storage_mem_3d_multisample(0x9102u, 4, 0x8058u, 16, 16, 4, true, memory, 0u);
  CHECK_EQ(sent_id(before),
           static_cast<std::uint32_t>(ids::kCmd_glTexStorageMem3DMultisampleEXT));
  before = rig.kmt().commands();
  calls.texture_storage_mem_1d(2u, 1, 0x8058u, 16, memory, 0u);
  CHECK_EQ(sent_id(before), static_cast<std::uint32_t>(ids::kCmd_glTextureStorageMem1DEXT));
  before = rig.kmt().commands();
  calls.texture_storage_mem_2d_multisample(3u, 4, 0x8058u, 16, 16, false, memory, 0u);
  CHECK_EQ(sent_id(before),
           static_cast<std::uint32_t>(ids::kCmd_glTextureStorageMem2DMultisampleEXT));
  before = rig.kmt().commands();
  calls.texture_storage_mem_3d(4u, 1, 0x8058u, 16, 16, 4, memory, 0u);
  CHECK_EQ(sent_id(before), static_cast<std::uint32_t>(ids::kCmd_glTextureStorageMem3DEXT));
  before = rig.kmt().commands();
  calls.texture_storage_mem_3d_multisample(5u, 4, 0x8058u, 16, 16, 4, false, memory, 0u);
  CHECK_EQ(sent_id(before),
           static_cast<std::uint32_t>(ids::kCmd_glTextureStorageMem3DMultisampleEXT));
  CHECK_EQ(context.take_error(), 0u);

  context.flush();
  { av::WireReader body(rig.kmt().command_bytes(named_at) + av::CommandHeader::encoded_size,
                        av::MockKmt::journal_bytes - av::CommandHeader::encoded_size);
    std::uint64_t buffer = 0u;
    std::uint64_t size = 0u;
    std::uint64_t named = 0u;
    CHECK(body.read_varint(buffer));
    CHECK(body.read_varint(size));
    CHECK(body.read_varint(named));
    CHECK_EQ(named, static_cast<std::uint64_t>(host_memory));
    CHECK(named != memory); }

  const std::uint32_t quiet = rig.kmt().commands();
  calls.named_buffer_storage_mem(1u, 4096, stranger, 0u);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  calls.tex_storage_mem_1d(0x0DE0u, 1, 0x8058u, 16, stranger, 0u);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  calls.tex_storage_mem_2d(0x0DE1u, 1, 0x8058u, 16, 16, stranger, 0u);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  calls.tex_storage_mem_2d_multisample(0x9100u, 4, 0x8058u, 16, 16, true, stranger, 0u);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  calls.tex_storage_mem_3d(0x806Fu, 1, 0x8058u, 16, 16, 4, stranger, 0u);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  calls.tex_storage_mem_3d_multisample(0x9102u, 4, 0x8058u, 16, 16, 4, true, stranger, 0u);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  calls.texture_storage_mem_1d(2u, 1, 0x8058u, 16, stranger, 0u);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  calls.texture_storage_mem_2d_multisample(3u, 4, 0x8058u, 16, 16, false, stranger, 0u);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  calls.texture_storage_mem_3d(4u, 1, 0x8058u, 16, 16, 4, stranger, 0u);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  calls.texture_storage_mem_3d_multisample(5u, 4, 0x8058u, 16, 16, 4, false, stranger, 0u);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  CHECK_EQ(rig.kmt().commands(), quiet); }

TEST(manual_calls, the_queries_and_parameters_are_answered_without_the_host)
{ const av::glicd::GlExtensionPolicy policy(nullptr, 0u, nullptr, 0u);
  Bare bare(policy);
  av::GlContextRig &rig = bare.rig;
  av::glicd::CurrentContexts &currents = bare.currents;
  ManualCalls &calls = bare.calls;
  av::glicd::GuestGlContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.groups(), rig.lines(),
                                    policy, rig.handles(), 1, av::GlContextOptions());
  rig.host().gl_made("host gpu", "4.6");
  CHECK_STATUS(context.make(), av::Status::ok);
  CHECK(currents.make_current(context));

  GLuint memory = 0u;
  GLuint semaphore = 0u;
  calls.create_memory_objects(1, &memory);
  calls.gen_semaphores(1, &semaphore);
  const std::uint32_t quiet = rig.kmt().commands();

  CHECK(calls.is_memory_object(memory));
  CHECK(calls.is_semaphore(semaphore));
  CHECK(!calls.is_memory_object(semaphore));
  CHECK(!calls.is_semaphore(memory));
  CHECK(!calls.is_memory_object(memory + 0x1000u));

  const GLint dedicated = 1;
  calls.memory_object_parameter(memory, av::glicd::GlExternalObjects::dedicated, &dedicated);
  CHECK_EQ(context.take_error(), 0u);
  GLint read = 0;
  calls.get_memory_object_parameter(memory, av::glicd::GlExternalObjects::dedicated, &read);
  CHECK_EQ(context.take_error(), 0u);
  CHECK_EQ(read, 1);
  calls.memory_object_parameter(memory, av::glicd::GlExternalObjects::protected_memory, &dedicated);
  CHECK_EQ(context.take_error(), 0u);
  calls.import_memory_win32(memory, 4096u, av::glicd::GlExternalObjects::opaque_win32,
                            openable_handle(rig));
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});

  const GLuint64 value = 7u;
  calls.semaphore_parameter(semaphore, av::glicd::GlExternalObjects::d3d12_fence_value, &value);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_enum});
  GLuint64 got = 0u;
  calls.get_semaphore_parameter(semaphore, av::glicd::GlExternalObjects::d3d12_fence_value, &got);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_enum});
  calls.get_semaphore_parameter(semaphore, av::glicd::GlExternalObjects::d3d12_fence_value, nullptr);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_value});
  calls.memory_object_parameter(memory, 0x1234u, &dedicated);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_enum});
  calls.get_memory_object_parameter(memory, av::glicd::GlExternalObjects::dedicated, nullptr);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_value});
  calls.memory_object_parameter(memory, av::glicd::GlExternalObjects::dedicated, nullptr);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_value});

  calls.import_memory_win32_name(memory, 4096u, av::glicd::GlExternalObjects::opaque_win32, "shared");
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  calls.import_semaphore_win32_name(semaphore, av::glicd::GlExternalObjects::opaque_win32, "shared");
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});

  CHECK_EQ(rig.kmt().commands(), quiet); }

TEST(manual_calls, a_count_no_call_can_have_is_kept_as_an_error_on_each_entry)
{ const av::glicd::GlExtensionPolicy policy(nullptr, 0u, nullptr, 0u);
  Bare bare(policy);
  av::GlContextRig &rig = bare.rig;
  av::glicd::CurrentContexts &currents = bare.currents;
  ManualCalls &calls = bare.calls;
  av::glicd::GuestGlContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.groups(), rig.lines(),
                                    policy, rig.handles(), 1, av::GlContextOptions());
  rig.host().gl_made("host gpu", "4.6");
  CHECK_STATUS(context.make(), av::Status::ok);
  CHECK(currents.make_current(context));

  const std::uint32_t quiet = rig.kmt().commands();
  GLuint              names[1] = {};
  calls.create_memory_objects(-1, names);
  CHECK_EQ(context.take_error(), std::uint32_t{gl_invalid_value});
  calls.gen_semaphores(-1, names);
  CHECK_EQ(context.take_error(), std::uint32_t{gl_invalid_value});
  calls.delete_memory_objects(-1, names);
  CHECK_EQ(context.take_error(), std::uint32_t{gl_invalid_value});
  calls.delete_semaphores(-1, names);
  CHECK_EQ(context.take_error(), std::uint32_t{gl_invalid_value});
  calls.import_semaphore_win32(1u, av::glicd::GlExternalObjects::opaque_win32,
                               reinterpret_cast<void *>(0xDEADu));
  CHECK_EQ(context.take_error(), std::uint32_t{gl_invalid_value});
  CHECK_EQ(rig.kmt().commands(), quiet); }

TEST(manual_calls, a_command_the_host_runs_forgets_the_copy_of_the_buffer_it_writes)
{ Current                   current;
  ManualCalls              &calls = current.rig().root().manual();
  av::glicd::BufferShadows &shadows = current.context().shadows();
  constexpr GLenum          copy_read = 0x8F36u, copy_write = 0x8F37u, uniform = 0x8A11u;
  constexpr GLenum          rgba8 = 0x8058u, rgba = 0x1908u, ubyte = 0x1401u;
  std::uint8_t              bytes[16] = {};
  for (std::uint32_t buffer = 1u; buffer <= 9u; buffer++) shadows.keep(buffer, 0u, sizeof bytes, bytes);
  const auto kept = [&](std::uint32_t buffer) { return shadows.covering(buffer, 0u, 1u) != nullptr; };
  calls.bind_buffer(copy_write, 2u);
  gen::Wrap_glCopyBufferSubData(copy_read, copy_write, 0, 0, 4);
  CHECK(!kept(2u));
  CHECK(kept(1u));
  gen::Wrap_glCopyNamedBufferSubData(1u, 3u, 0, 0, 4);
  CHECK(!kept(3u));
  CHECK(kept(1u));
  gen::Wrap_glClearNamedBufferData(4u, rgba8, rgba, ubyte, nullptr);
  CHECK(!kept(4u));
  gen::Wrap_glInvalidateBufferData(5u);
  CHECK(!kept(5u));
  gen::Wrap_glGetQueryBufferObjectiv(1u, 6u, 0x8866u , 0);
  CHECK(!kept(6u));
  calls.bind_buffer(pixel_pack_buffer, 7u);
  current.rig().host().gl_bytes(nullptr, 0u);
  calls.read_pixels(0, 0, 1, 1, rgba, ubyte, nullptr);
  CHECK(!kept(7u));
  shadows.keep(7u, 0u, sizeof bytes, bytes);
  current.rig().host().gl_bytes(nullptr, 0u);
  gen::Wrap_glGetCompressedTexImage(0x0DE1u, 0, nullptr);
  CHECK(!kept(7u));
  calls.bind_buffer(0x9192u , 9u);
  current.rig().host().gl_bytes(nullptr, 0u);
  gen::Wrap_glGetQueryObjectiv(1u, 0x8866u, nullptr);
  CHECK(!kept(9u));
  CHECK(kept(1u));
  CHECK(kept(8u));
  const GLuint several[] = {9u};
  gen::Wrap_glBindBuffersBase(uniform, 0u, 1, several);
  CHECK(shadows.trusted());
  gen::Wrap_glClearBufferData(uniform, rgba8, rgba, ubyte, nullptr);
  CHECK_EQ(shadows.held(), 0u);
  CHECK(shadows.trusted()); }

TEST(manual_calls, buffer_sub_data_goes_into_the_copy_of_the_buffer_bound_or_named)
{ Current                   current;
  ManualCalls              &calls = current.rig().root().manual();
  av::glicd::BufferShadows &shadows = current.context().shadows();
  constexpr GLenum          uniform = 0x8A11u;
  std::uint8_t              zeros[16] = {};
  const std::uint8_t        data[4] = {1u, 2u, 3u, 4u};
  for (std::uint32_t buffer = 3u; buffer <= 5u; buffer++) shadows.keep(buffer, 0u, sizeof zeros, zeros);
  calls.bind_buffer(uniform, 3u);
  gen::Wrap_glBufferSubData(uniform, 4, sizeof data, data);
  CHECK(std::memcmp(shadows.covering(3u, 4u, 4u), data, sizeof data) == 0);
  gen::Wrap_glNamedBufferSubData(4u, 8, sizeof data, data);
  CHECK(std::memcmp(shadows.covering(4u, 8u, 4u), data, sizeof data) == 0);
  gen::Wrap_glBufferSubData(uniform, -1, sizeof data, data);
  gen::Wrap_glBufferSubData(uniform, 0, -1, data);
  gen::Wrap_glNamedBufferSubData(4u, -1, sizeof data, data);
  gen::Wrap_glNamedBufferSubData(4u, 0, -1, data);
  CHECK_EQ(shadows.covering(3u, 0u, 1u)[0], 0u);
  CHECK_EQ(shadows.covering(4u, 0u, 1u)[0], 0u);
  gen::Wrap_glBindBufferRange(uniform, 0u, 5u, 0, 16);
  CHECK_EQ(current.context().arrays().bound_buffer(uniform), 5u);
  gen::Wrap_glBufferSubData(uniform, 0, sizeof data, data);
  CHECK(std::memcmp(shadows.covering(5u, 0u, 4u), data, sizeof data) == 0);
  CHECK_EQ(shadows.covering(3u, 0u, 1u)[0], 0u);
  const GLuint     several[] = {3u};
  const GLintptr   offsets[] = {0};
  const GLsizeiptr sizes[] = {16};
  gen::Wrap_glBindBuffersRange(uniform, 0u, 1, several, offsets, sizes);
  gen::Wrap_glBufferSubData(uniform, 0, sizeof data, data);
  CHECK_EQ(shadows.held(), 0u); }

TEST(manual_calls, a_binding_a_shader_writes_through_ends_the_copies)
{ constexpr GLenum storage = 0x90D2u, atomic = 0x92C0u, feedback = 0x8C8Eu, uniform = 0x8A11u;
  {
    Current      current;
    const GLuint none[] = {0u};
    gen::Wrap_glBindBufferBase(uniform, 0u, 5u);
    gen::Wrap_glBindBufferBase(storage, 0u, 0u);
    gen::Wrap_glBindBuffersBase(uniform, 0u, 1, none);
    CHECK(current.context().shadows().trusted());
    gen::Wrap_glBindBufferRange(atomic, 0u, 5u, 0, 4);
    CHECK(!current.context().shadows().trusted());
  }
  {
    Current      current;
    const GLuint some[] = {5u};
    gen::Wrap_glBindBuffersBase(feedback, 0u, 1, some);
    CHECK(!current.context().shadows().trusted());
  }
  {
    Current current;
    gen::Wrap_glBindImageTexture(0u, 1u, 0, 0u, 0, 0x88B9u , 0x8058u);
    CHECK(!current.context().shadows().trusted());
  }
  {
    Current current;
    gen::Wrap_glBeginTransformFeedback(0x0000u );
    CHECK(!current.context().shadows().trusted());
  }
  {
    Current current;
    gen::Wrap_glTransformFeedbackBufferBase(1u, 0u, 5u);
    CHECK(!current.context().shadows().trusted());
  } }

TEST(manual_calls, new_storage_is_fresh_and_a_deleted_buffer_has_no_copy)
{ Current                   current;
  ManualCalls              &calls = current.rig().root().manual();
  av::glicd::BufferShadows &shadows = current.context().shadows();
  constexpr GLenum          uniform = 0x8A11u, stream_draw = 0x88E0u;
  std::uint8_t              data[16] = {};
  calls.bind_buffer(array_buffer, 3u);
  shadows.keep(3u, 0u, sizeof data, data);
  calls.buffer_data(array_buffer, sizeof data, nullptr, stream_draw);
  CHECK(shadows.covering(3u, 0u, 1u) == nullptr);
  CHECK(shadows.fresh(3u));
  calls.buffer_data(array_buffer, sizeof data, data, stream_draw);
  CHECK(!shadows.fresh(3u));
  calls.buffer_storage(array_buffer, sizeof data, nullptr, 0u);
  CHECK(shadows.fresh(3u));
  calls.named_buffer_data(4u, sizeof data, nullptr, stream_draw);
  calls.named_buffer_storage(5u, sizeof data, nullptr, 0u);
  CHECK(shadows.fresh(4u));
  CHECK(shadows.fresh(5u));
  calls.named_buffer_storage(5u, sizeof data, data, 0u);
  CHECK(!shadows.fresh(5u));
  const GLuint several[] = {6u};
  gen::Wrap_glBindBuffersBase(uniform, 0u, 1, several);
  calls.buffer_data(uniform, sizeof data, nullptr, stream_draw);
  CHECK(!shadows.fresh(3u));
  CHECK(!shadows.fresh(4u));
  const GLuint deleted[] = {7u};
  shadows.keep(7u, 0u, sizeof data, data);
  calls.delete_buffers(1, deleted);
  CHECK(shadows.covering(7u, 0u, 1u) == nullptr);
  shadows.keep(8u, 0u, sizeof data, data);
  calls.client_attributes_popped();
  CHECK_EQ(shadows.held(), 0u);
  CHECK(shadows.trusted()); }

namespace {

using av::glicd::BufferPieces;

std::uint8_t bulk[std::size_t{1} << 20u];

bool sent_as(Current &current, std::uint32_t first, const std::uint32_t *expected, std::uint32_t count)
{
    if (current.sent() != first + count) return false;
    for (std::uint32_t index = 0u; index < count; index++)
        if (current.sent_since(first + index) != expected[index]) return false;
    return true;
}

}

TEST(manual_calls, without_an_arena_a_buffers_bytes_cross_in_pieces_each_way)
{ Current current(no_arena);
  CHECK(!current.context().wire()->blobs().ready());
  ManualCalls        &calls = current.rig().root().manual();
  const std::size_t   write_room = current.context().wire()->room().inline_write();
  const std::size_t   read_room = current.context().wire()->room().inline_read();
  const intptr_t      over = static_cast<intptr_t>(write_room) + 1;
  calls.buffer_sub_data(array_buffer, 0, over, bulk);
  calls.named_buffer_sub_data(4u, 0, over, bulk);
  calls.named_buffer_sub_data_ext(4u, 0, over, bulk);
  const std::uint32_t writes[] = {ids::kCmd_glBufferSubData,           ids::kCmd_glBufferSubData,
                                  ids::kCmd_glNamedBufferSubData,      ids::kCmd_glNamedBufferSubData,
                                  ids::kCmd_glNamedBufferSubDataEXT,   ids::kCmd_glNamedBufferSubDataEXT};
  CHECK(sent_as(current, 0u, writes, 6u));
  CHECK_EQ(current.argument_since(1u, 1u), write_room);
  CHECK_EQ(current.argument_since(1u, 2u), 1u);
  calls.buffer_data(array_buffer, over, bulk, 0x88E4u);
  calls.buffer_storage(array_buffer, over, bulk, 0u);
  calls.named_buffer_data(4u, over, bulk, 0x88E4u);
  calls.named_buffer_storage(4u, over, bulk, 0u);
  calls.named_buffer_data_ext(4u, over, bulk, 0x88E4u);
  calls.named_buffer_storage_ext(4u, over, bulk, 0u);
  const std::uint32_t storage[] = {
      ids::kCmd_glBufferData,            ids::kCmd_glBufferSubData,         ids::kCmd_glBufferSubData,
      ids::kCmd_glBufferStorage,         ids::kCmd_glBufferSubData,         ids::kCmd_glBufferSubData,
      ids::kCmd_glNamedBufferData,       ids::kCmd_glNamedBufferSubData,    ids::kCmd_glNamedBufferSubData,
      ids::kCmd_glNamedBufferStorage,    ids::kCmd_glNamedBufferSubData,    ids::kCmd_glNamedBufferSubData,
      ids::kCmd_glNamedBufferDataEXT,    ids::kCmd_glNamedBufferSubDataEXT, ids::kCmd_glNamedBufferSubDataEXT,
      ids::kCmd_glNamedBufferStorageEXT, ids::kCmd_glNamedBufferSubDataEXT, ids::kCmd_glNamedBufferSubDataEXT};
  CHECK(sent_as(current, 6u, storage, 18u));
  const intptr_t read = 2 * static_cast<intptr_t>(read_room) + 1;
  for (std::uint32_t answer = 0u; answer < 9u; answer++) current.rig().host().gl_bytes(nullptr, 0u);
  calls.get_buffer_sub_data(array_buffer, 8, read, bulk);
  calls.get_named_buffer_sub_data(4u, 0, read, bulk);
  calls.get_named_buffer_sub_data_ext(4u, 0, read, bulk);
  CHECK_EQ(current.sent(), 33u);
  CHECK_EQ(current.sent_since(24u), ids::kCmd_glGetBufferSubData);
  CHECK_EQ(current.sent_since(26u), ids::kCmd_glGetBufferSubData);
  CHECK_EQ(current.argument_since(26u, 1u), 8u + 2u * read_room);
  CHECK_EQ(current.sent_since(27u), ids::kCmd_glGetNamedBufferSubData);
  CHECK_EQ(current.sent_since(32u), ids::kCmd_glGetNamedBufferSubDataEXT);
  current.rig().host().gl_bytes(nullptr, 0u);
  calls.get_buffer_sub_data(array_buffer, 0, static_cast<intptr_t>(read_room), bulk);
  calls.buffer_sub_data(array_buffer, 0, -1, bulk);
  CHECK_EQ(current.sent(), 34u); }

TEST(manual_calls, with_an_arena_a_large_write_is_lent_whole)
{ Current current;
  CHECK(current.context().wire()->blobs().ready());
  const std::size_t write_room = current.context().wire()->room().inline_write();
  current.rig().root().manual().buffer_sub_data(array_buffer, 0, static_cast<intptr_t>(write_room) + 1, bulk);
  CHECK_EQ(current.sent(), 1u); }

TEST(manual_calls, without_an_arena_a_large_read_pixels_goes_through_the_staging_buffer)
{ Current current(no_arena);
  ManualCalls &calls = current.rig().root().manual();
  const GLuint staged = 21u;
  current.rig().host().gl_bytes(reinterpret_cast<const std::uint8_t *>(&staged), sizeof staged);
  for (std::uint32_t answer = 0u; answer < 3u; answer++) current.rig().host().gl_bytes(nullptr, 0u);
  calls.read_pixels(0, 0, 200, 100, 0x1908u, 0x1401u, bulk);
  const std::uint32_t read[] = {ids::kCmd_glGenBuffers,       ids::kCmd_glBindBuffer,       ids::kCmd_glBufferData,
                                ids::kCmd_glReadPixels,       ids::kCmd_glGetBufferSubData, ids::kCmd_glGetBufferSubData,
                                ids::kCmd_glBindBuffer};
  CHECK(sent_as(current, 0u, read, 7u));
  CHECK_EQ(current.argument_since(1u, 0u), pixel_pack_buffer);
  CHECK_EQ(current.argument_since(1u, 1u), staged);
  CHECK_EQ(current.argument_since(6u, 1u), 0u);
  CHECK_EQ(current.context().staging().name(), staged);
  CHECK(!current.context().arrays().pixel_buffer_bound(true));
  CHECK(current.context().invariants_hold());
  CHECK(current.rig().opengl32().make_current(nullptr, nullptr));
  CHECK(current.rig().opengl32().delete_context(current.handle()));
  bool deleted = false;
  for (std::uint32_t index = 7u; index < current.rig().kmt().commands() - 0u; index++)
      deleted = deleted || current.rig().kmt().command_id(index) == ids::kCmd_glDeleteBuffers;
  CHECK(deleted); }

TEST(manual_calls, without_an_arena_a_large_generated_upload_is_staged)
{ Current current(no_arena);
  const GLuint staged = 22u;
  current.rig().host().gl_bytes(reinterpret_cast<const std::uint8_t *>(&staged), sizeof staged);
  gen::Wrap_glTexSubImage2D(0x0DE1u, 0, 0, 0, 512, 256, 0x1908u, 0x1401u, bulk);
  const std::uint32_t upload[] = {ids::kCmd_glGenBuffers,    ids::kCmd_glBindBuffer,    ids::kCmd_glBufferData,
                                  ids::kCmd_glBufferSubData, ids::kCmd_glBufferSubData, ids::kCmd_glBufferSubData,
                                  ids::kCmd_glTexSubImage2D, ids::kCmd_glBindBuffer};
  CHECK(sent_as(current, 0u, upload, 8u));
  CHECK_EQ(current.argument_since(1u, 0u), 0x88ECu);
  gen::Wrap_glTexSubImage2D(0x0DE1u, 0, 0, 0, 16, 16, 0x1908u, 0x1401u, bulk);
  CHECK_EQ(current.sent(), 9u);
  CHECK_EQ(current.sent_since(8u), ids::kCmd_glTexSubImage2D);
  CHECK(current.context().invariants_hold()); }

namespace {

constexpr GLenum pixel_unpack_buffer = 0x88ECu, transform_feedback_buffer = 0x8C8Eu;
constexpr std::uint32_t sliced_buffer = 5u, plain_buffer = 6u, sliced_bytes = 4096u;

class SlicedCurrent {
public:
    SlicedCurrent()
    {
        rig_.kmt().open_host_window(av::MockKmt::rig_window_bytes);
        rig_.load_with_format();
        void *const handle = rig_.opengl32().create_context(rig_.dc());
        CHECK(handle != nullptr);
        rig_.host().gl_make("host gpu", "4.6");
        rig_.host().gl_integer(0);
        rig_.host().gl_slices(1u);
        rig_.host().gl_window(1u, 1u);
        CHECK(rig_.opengl32().make_current(rig_.dc(), handle));
        av::glicd::GuestGlContext &made = context();
        rig_.host().blob_import(0u, 0x0000700000000100ull);
        rig_.host().gl_slices(1u);
        CHECK_STATUS(made.slices().place(*made.wire(), sliced_buffer, sliced_bytes,
                                         av::glicd::BufferMapping::map_persistent | av::glicd::BufferMapping::map_write,
                                         nullptr),
                     av::Status::ok);
        made.flush();
        commands_ = rig_.kmt().commands();
    }

    GlIcdRig                  &rig() { return rig_; }
    std::uint32_t              error() { return context().take_error(); }
    ManualCalls               &calls() { return rig_.root().manual(); }
    av::glicd::GuestGlContext &context() { return *rig_.root().current(); }
    std::uint32_t              sent()
    {
        context().flush();
        std::uint32_t count = 0u;
        for (std::uint32_t at = commands_; at < rig_.kmt().commands(); at++)
            count += rig_.kmt().command_id(at) == av::CommandHeader::channel_stats_id ? 0u : 1u;
        return count;
    }
    std::uint32_t id(std::uint32_t index)
    {
        context().flush();
        std::uint32_t seen = 0u;
        for (std::uint32_t at = commands_; at < rig_.kmt().commands(); at++) {
            if (rig_.kmt().command_id(at) == av::CommandHeader::channel_stats_id) continue;
            if (seen++ == index) return rig_.kmt().command_id(at);
        }
        return 0u;
    }
    std::uint64_t argument(std::uint32_t index, std::uint32_t nth)
    {
        context().flush();
        std::uint32_t seen = 0u;
        for (std::uint32_t at = commands_; at < rig_.kmt().commands(); at++) {
            if (rig_.kmt().command_id(at) == av::CommandHeader::channel_stats_id) continue;
            if (seen++ != index) continue;
            av::WireReader reader(rig_.kmt().command_bytes(at) + av::CommandHeader::encoded_size,
                                  av::MockKmt::journal_bytes - av::CommandHeader::encoded_size);
            std::uint64_t  value = 0u;
            for (std::uint32_t arg = 0u; arg <= nth; arg++) CHECK(reader.read_varint(value));
            return value;
        }
        return 0u;
    }

private:
    GlIcdRig      rig_;
    std::uint32_t commands_ = 0u;
};

}

TEST(manual_calls_slices, a_pixel_buffer_binding_is_sliced_only_when_a_slice_is_bound_to_it)
{ SlicedCurrent current;
  ManualCalls  &calls = current.calls();
  CHECK(!calls.unpack_slice_bound());
  CHECK(!calls.pack_slice_bound());
  calls.bind_buffer(pixel_unpack_buffer, sliced_buffer);
  CHECK(calls.unpack_slice_bound());
  CHECK(!calls.pack_slice_bound());
  calls.bind_buffer(pixel_unpack_buffer, plain_buffer);
  CHECK(!calls.unpack_slice_bound());
  calls.bind_buffer(pixel_pack_buffer, sliced_buffer);
  CHECK(calls.pack_slice_bound());
  CHECK(!calls.unpack_slice_bound());
  calls.bind_buffer(pixel_pack_buffer, 0u);
  CHECK(!calls.pack_slice_bound()); }

TEST(manual_calls_slices, no_context_current_has_no_sliced_binding_and_refuses_nothing)
{ GlIcdRig rig;
  rig.load_with_format();
  ManualCalls &calls = rig.root().manual();
  const void  *pixels = reinterpret_cast<const void *>(8u);
  void        *target = reinterpret_cast<void *>(8u);
  CHECK(!calls.unpack_slice_bound());
  CHECK(!calls.pack_slice_bound());
  CHECK(!calls.refuse_sliced_unpack());
  CHECK(!calls.refuse_sliced_pack());
  CHECK(!calls.slice_unpack_pixels(pixels, 4u));
  CHECK(!calls.slice_pack_pixels(target, 4u));
  calls.unpack_slice_done();
  calls.pack_slice_done();
  CHECK(!calls.invalidate_named_buffer(sliced_buffer));
  CHECK(!calls.invalidate_named_buffer_range(sliced_buffer, 0, 4));
  CHECK_EQ(rig.kmt().commands(), 0u); }

TEST(manual_calls_slices, an_unpack_from_a_slice_reads_the_slices_memory_with_the_host_binding_dropped_around_it)
{ SlicedCurrent current;
  ManualCalls  &calls = current.calls();
  calls.bind_buffer(pixel_unpack_buffer, sliced_buffer);
  const std::uint32_t before = current.sent();
  const void         *pixels = reinterpret_cast<const void *>(64u);
  CHECK(calls.slice_unpack_pixels(pixels, 32u));
  const std::uint8_t *expected = nullptr;
  CHECK_EQ(current.context().slices().unpack_source(sliced_buffer, 64u, 32u, expected),
           av::glicd::SlicedBuffers::gl_no_error);
  CHECK(pixels == expected);
  CHECK_EQ(current.sent() - before, 1u);
  CHECK_EQ(current.id(before), ids::kCmd_glBindBuffer);
  CHECK_EQ(current.argument(before, 0u), pixel_unpack_buffer);
  CHECK_EQ(current.argument(before, 1u), 0u);
  calls.unpack_slice_done();
  CHECK_EQ(current.sent() - before, 2u);
  CHECK_EQ(current.id(before + 1u), ids::kCmd_glBindBuffer);
  CHECK_EQ(current.argument(before + 1u, 0u), pixel_unpack_buffer);
  CHECK_EQ(current.argument(before + 1u, 1u), sliced_buffer);
  CHECK_EQ(current.error(), 0u); }

TEST(manual_calls_slices, an_unpack_past_the_slice_is_refused_with_an_error_and_sends_nothing)
{ SlicedCurrent current;
  ManualCalls  &calls = current.calls();
  calls.bind_buffer(pixel_unpack_buffer, sliced_buffer);
  const std::uint32_t before = current.sent();
  const void         *pixels = reinterpret_cast<const void *>(sliced_bytes - 8u);
  CHECK(!calls.slice_unpack_pixels(pixels, 32u));
  CHECK(pixels == reinterpret_cast<const void *>(sliced_bytes - 8u));
  CHECK_EQ(current.sent(), before);
  CHECK_EQ(current.error(), gl_invalid_operation); }

TEST(manual_calls_slices, a_pack_into_a_slice_writes_the_slices_memory_with_the_host_binding_dropped_around_it)
{ SlicedCurrent current;
  ManualCalls  &calls = current.calls();
  calls.bind_buffer(pixel_pack_buffer, sliced_buffer);
  const std::uint32_t before = current.sent();
  void               *pixels = reinterpret_cast<void *>(128u);
  CHECK(calls.slice_pack_pixels(pixels, 16u));
  std::uint8_t *expected = nullptr;
  CHECK_EQ(current.context().slices().pack_target(sliced_buffer, 128u, 16u, expected),
           av::glicd::SlicedBuffers::gl_no_error);
  CHECK(pixels == expected);
  static_cast<std::uint8_t *>(pixels)[0] = 0x66u;
  std::uint8_t seen = 0u;
  CHECK_EQ(current.context().slices().read(sliced_buffer, 128, 1, &seen), av::glicd::SlicedBuffers::gl_no_error);
  CHECK_EQ(seen, 0x66u);
  CHECK_EQ(current.id(before), ids::kCmd_glBindBuffer);
  CHECK_EQ(current.argument(before, 0u), pixel_pack_buffer);
  CHECK_EQ(current.argument(before, 1u), 0u);
  calls.pack_slice_done();
  CHECK_EQ(current.argument(before + 1u, 0u), pixel_pack_buffer);
  CHECK_EQ(current.argument(before + 1u, 1u), sliced_buffer);
  void *beyond = reinterpret_cast<void *>(sliced_bytes);
  CHECK(!calls.slice_pack_pixels(beyond, 1u));
  CHECK_EQ(current.error(), gl_invalid_operation); }

TEST(manual_calls_slices, a_sliced_pixel_binding_is_refused_where_the_call_cannot_use_it)
{ SlicedCurrent current;
  ManualCalls  &calls = current.calls();
  CHECK(!calls.refuse_sliced_unpack());
  CHECK(!calls.refuse_sliced_pack());
  CHECK_EQ(current.error(), 0u);
  calls.bind_buffer(pixel_unpack_buffer, sliced_buffer);
  CHECK(calls.refuse_sliced_unpack());
  CHECK_EQ(current.error(), gl_invalid_operation);
  CHECK(!calls.refuse_sliced_pack());
  CHECK_EQ(current.error(), 0u);
  calls.bind_buffer(pixel_pack_buffer, sliced_buffer);
  CHECK(calls.refuse_sliced_pack());
  CHECK_EQ(current.error(), gl_invalid_operation);
  calls.bind_buffer(pixel_pack_buffer, plain_buffer);
  CHECK(!calls.refuse_sliced_pack());
  CHECK_EQ(current.error(), 0u); }

TEST(manual_calls_slices, invalidating_a_sliced_buffer_is_answered_here_and_any_other_is_left_to_the_host)
{ SlicedCurrent current;
  ManualCalls  &calls = current.calls();
  const std::uint32_t before = current.sent();
  CHECK(calls.invalidate_named_buffer(sliced_buffer));
  CHECK(!calls.invalidate_named_buffer(plain_buffer));
  CHECK(calls.invalidate_named_buffer_range(sliced_buffer, 16, 64));
  CHECK_EQ(current.error(), 0u);
  CHECK(!calls.invalidate_named_buffer_range(plain_buffer, 16, 64));
  CHECK(calls.invalidate_named_buffer_range(sliced_buffer, sliced_bytes - 4, 64));
  CHECK_EQ(current.error(), gl_invalid_value);
  CHECK(calls.invalidate_named_buffer_range(sliced_buffer, -1, 64));
  CHECK_EQ(current.error(), gl_invalid_value);
  CHECK_EQ(current.sent(), before); }

TEST(manual_calls_slices, deleting_transform_feedbacks_forgets_what_was_bound_to_their_target)
{ SlicedCurrent current;
  ManualCalls  &calls = current.calls();
  const std::uint32_t ids_deleted[] = {3u, 4u};
  calls.bind_buffer(transform_feedback_buffer, 9u);
  CHECK_EQ(current.context().arrays().bound_buffer(transform_feedback_buffer), 9u);
  calls.delete_transform_feedbacks(0, ids_deleted);
  calls.delete_transform_feedbacks(2, nullptr);
  CHECK_EQ(current.context().arrays().bound_buffer(transform_feedback_buffer), 9u);
  calls.delete_transform_feedbacks(2, ids_deleted);
  CHECK_EQ(current.context().arrays().bound_buffer(transform_feedback_buffer),
           av::glicd::ClientArrayState::unknown);
  CHECK_EQ(current.id(current.sent() - 1u), ids::kCmd_glDeleteTransformFeedbacks); }

TEST(manual_calls_window, the_window_framebuffer_and_buffers_are_translated_to_the_hosts_when_the_window_is_bound)
{ Current current;
  constexpr GLenum gl_framebuffer = 0x8D40u, gl_back = 0x0405u, gl_color_attachment0 = 0x8CE0u;
  ManualCalls     &calls = current.rig().root().manual();
  calls.bind_framebuffer(gl_framebuffer, 0u);
  calls.draw_buffer(gl_back);
  CHECK_EQ(current.sent_since(0u), ids::kCmd_glBindFramebuffer);
  CHECK_EQ(current.argument_since(0u, 1u), 100u);
  CHECK_EQ(current.sent_since(1u), ids::kCmd_glDrawBuffer);
  CHECK_EQ(current.argument_since(1u, 0u), gl_color_attachment0);
  calls.bind_framebuffer(gl_framebuffer, 7u);
  calls.draw_buffer(gl_back);
  CHECK_EQ(current.argument_since(2u, 1u), 7u);
  CHECK_EQ(current.argument_since(3u, 0u), gl_back);
  CHECK(current.context().invariants_hold()); }

namespace {

struct WaitSent {
    std::uint64_t sync = 0u;
    std::uint64_t flags = 0u;
    std::uint64_t timeout = 0u;
};

WaitSent wait_sent(Current &current, std::uint32_t index)
{
    WaitSent sent;
    CHECK_EQ(current.sent_since(index), ids::kCmd_glClientWaitSync);
    av::WireReader reader(current.command_since(index) + av::CommandHeader::encoded_size,
                          av::MockKmt::journal_bytes - av::CommandHeader::encoded_size);
    CHECK(reader.read_pod(sent.sync));
    CHECK(reader.read_varint(sent.flags));
    CHECK(reader.read_varint(sent.timeout));
    return sent;
}

}

TEST(sync_wait_probe, a_wait_the_host_answers_signaled_is_done_with_that_answer)
{ Current current;
  current.rig().host().gl_varint(SyncWaitProbe::gl_already_signaled);
  SyncWaitProbe probe(*current.context().wire(), 0x77u, SyncWaitProbe::gl_sync_flush_commands_bit);
  CHECK(probe.ask(1000u));
  CHECK_EQ(probe.result(), SyncWaitProbe::gl_already_signaled);
  const WaitSent sent = wait_sent(current, 0u);
  CHECK_EQ(sent.sync, 0x77u);
  CHECK_EQ(sent.flags, SyncWaitProbe::gl_sync_flush_commands_bit);
  CHECK_EQ(sent.timeout, 1000u); }

TEST(sync_wait_probe, a_wait_that_timed_out_asks_again_without_flushing_and_a_signal_after_it_is_a_satisfied_condition)
{ Current current;
  av::ScriptedHost &host = current.rig().host();
  SyncWaitProbe     probe(*current.context().wire(), 0x77u, SyncWaitProbe::gl_sync_flush_commands_bit | 0x4u);
  CHECK_EQ(probe.result(), SyncWaitProbe::gl_timeout_expired);
  host.gl_varint(SyncWaitProbe::gl_timeout_expired);
  CHECK(!probe.ask(500u));
  CHECK_EQ(probe.result(), SyncWaitProbe::gl_timeout_expired);
  host.gl_varint(SyncWaitProbe::gl_already_signaled);
  CHECK(probe.ask(500u));
  CHECK_EQ(probe.result(), SyncWaitProbe::gl_condition_satisfied);
  CHECK_EQ(wait_sent(current, 0u).flags, SyncWaitProbe::gl_sync_flush_commands_bit | 0x4u);
  CHECK_EQ(wait_sent(current, 1u).flags, 0x4u);
  probe.pause(500u);
  CHECK_EQ(probe.result(), SyncWaitProbe::gl_condition_satisfied); }

TEST(sync_wait_probe, a_condition_the_host_satisfies_is_reported_as_it_came)
{ Current current;
  current.rig().host().gl_varint(SyncWaitProbe::gl_condition_satisfied);
  SyncWaitProbe probe(*current.context().wire(), 0x77u, 0u);
  CHECK(probe.ask(0u));
  CHECK_EQ(probe.result(), SyncWaitProbe::gl_condition_satisfied);
  CHECK_EQ(wait_sent(current, 0u).flags, 0u); }

TEST(sync_wait_probe, an_answer_after_a_timeout_other_than_signaled_is_reported_as_it_came)
{ Current current;
  av::ScriptedHost &host = current.rig().host();
  SyncWaitProbe     probe(*current.context().wire(), 0x77u, 0u);
  host.gl_varint(SyncWaitProbe::gl_timeout_expired);
  CHECK(!probe.ask(500u));
  host.gl_varint(0x911Du);
  CHECK(probe.ask(500u));
  CHECK_EQ(probe.result(), 0x911Du); }

TEST(manual_calls_slices, writes_into_a_sliced_buffer_are_refused_and_reads_come_from_its_memory)
{ SlicedCurrent current;
  ManualCalls  &calls = current.calls();
  calls.bind_buffer(array_buffer, sliced_buffer);
  std::uint8_t *target = nullptr;
  CHECK_EQ(current.context().slices().pack_target(sliced_buffer, 0u, 4u, target), av::glicd::SlicedBuffers::gl_no_error);
  const std::uint8_t kept[4] = {9u, 8u, 7u, 6u};
  std::memcpy(target, kept, sizeof kept);
  const std::uint32_t before = current.sent();
  const std::uint8_t  data[4] = {1u, 2u, 3u, 4u};
  calls.buffer_sub_data(array_buffer, 0, sizeof data, data);
  CHECK_EQ(current.error(), gl_invalid_operation);
  calls.named_buffer_sub_data(sliced_buffer, 0, sizeof data, data);
  CHECK_EQ(current.error(), gl_invalid_operation);
  calls.named_buffer_sub_data_ext(sliced_buffer, 0, sizeof data, data);
  CHECK_EQ(current.error(), gl_invalid_operation);
  std::uint8_t seen[4] = {};
  calls.get_buffer_sub_data(array_buffer, 0, sizeof seen, seen);
  CHECK(std::memcmp(seen, kept, sizeof kept) == 0);
  std::memset(seen, 0, sizeof seen);
  calls.get_named_buffer_sub_data(sliced_buffer, 0, sizeof seen, seen);
  CHECK(std::memcmp(seen, kept, sizeof kept) == 0);
  std::memset(seen, 0, sizeof seen);
  calls.get_named_buffer_sub_data_ext(sliced_buffer, 0, sizeof seen, seen);
  CHECK(std::memcmp(seen, kept, sizeof kept) == 0);
  CHECK_EQ(current.error(), 0u);
  CHECK_EQ(current.sent(), before); }

TEST(manual_calls_slices, a_sliced_buffer_is_cleared_in_place_and_any_other_is_left_to_the_host)
{ SlicedCurrent current;
  ManualCalls  &calls = current.calls();
  constexpr std::uint32_t gl_r8 = 0x8229u, gl_red = 0x1903u, gl_unsigned_byte = 0x1401u;
  const std::uint8_t      fill = 0x5Au;
  calls.bind_buffer(array_buffer, sliced_buffer);
  const std::uint32_t before = current.sent();
  CHECK(calls.clear_bound_buffer(array_buffer, gl_r8, gl_red, gl_unsigned_byte, &fill));
  std::uint8_t seen[4] = {};
  calls.get_named_buffer_sub_data(sliced_buffer, 0, sizeof seen, seen);
  CHECK_EQ(seen[0], 0x5Au);
  CHECK_EQ(seen[3], 0x5Au);
  CHECK(calls.clear_bound_buffer_range(array_buffer, gl_r8, 1, 2, gl_red, gl_unsigned_byte, &fill));
  CHECK(calls.clear_named_buffer(sliced_buffer, gl_r8, gl_red, gl_unsigned_byte, &fill));
  CHECK(calls.clear_named_buffer_range(sliced_buffer, gl_r8, 0, 4, gl_red, gl_unsigned_byte, &fill));
  CHECK_EQ(current.error(), 0u);
  CHECK(calls.clear_named_buffer(sliced_buffer, 0u, gl_red, gl_unsigned_byte, &fill));
  CHECK_EQ(current.error(), 0x0500u);
  CHECK(calls.clear_named_buffer_range(sliced_buffer, gl_r8, -1, 2, gl_red, gl_unsigned_byte, &fill));
  CHECK_EQ(current.error(), gl_invalid_value);
  CHECK(!calls.clear_named_buffer(plain_buffer, gl_r8, gl_red, gl_unsigned_byte, &fill));
  CHECK(!calls.clear_named_buffer_range(plain_buffer, gl_r8, 0, 4, gl_red, gl_unsigned_byte, &fill));
  calls.bind_buffer(array_buffer, plain_buffer);
  CHECK(!calls.clear_bound_buffer(array_buffer, gl_r8, gl_red, gl_unsigned_byte, &fill));
  CHECK(!calls.clear_bound_buffer_range(array_buffer, gl_r8, 0, 4, gl_red, gl_unsigned_byte, &fill));
  CHECK(!calls.clear_bound_buffer(0x1234u, gl_r8, gl_red, gl_unsigned_byte, &fill));
  CHECK_EQ(current.error(), gl_invalid_enum);
  CHECK(!calls.clear_bound_buffer_range(0x1234u, gl_r8, 0, 4, gl_red, gl_unsigned_byte, &fill));
  CHECK_EQ(current.error(), gl_invalid_enum);
  CHECK_EQ(current.sent(), before + 1u); }

TEST(manual_calls, the_pinned_buffer_target_binds_a_name_here_and_pins_the_memory_a_data_call_gives)
{ Current      current;
  ManualCalls &calls = current.rig().root().manual();
  constexpr GLenum pinned_target = av::glicd::PinnedBuffers::target;
  alignas(4096) static std::uint8_t pages[2u * 4096u];
  const std::uint32_t before = current.sent();
  calls.buffer_data(pinned_target, 64, pages, 0x88E4u);
  CHECK_EQ(current.error(), gl_invalid_operation);
  calls.bind_buffer(pinned_target, 7u);
  CHECK_EQ(current.context().pinned().bound(), 7u);
  CHECK_EQ(current.sent(), before);
  calls.buffer_data(pinned_target, 0, pages, 0x88E4u);
  CHECK_EQ(current.error(), gl_invalid_value);
  calls.buffer_data(pinned_target, 64, nullptr, 0x88E4u);
  CHECK_EQ(current.error(), gl_invalid_value);
  CHECK_EQ(current.sent(), before);
  current.rig().host().blob_import(0u, 0x0000700000000300ull);
  const std::uint32_t pinned_at = current.sent();
  calls.buffer_data(pinned_target, 64, pages + 100, 0x88E4u);
  CHECK_EQ(current.sent_since(pinned_at + 1u), ids::kCmd_glBindBuffer);
  CHECK_EQ(current.argument_since(pinned_at + 1u, 1u), 7u);
  CHECK_EQ(current.sent_since(pinned_at + 2u), ids::kCmd_glBufferStorageMemEXT);
  CHECK_EQ(current.sent_since(pinned_at + 3u), ids::kCmd_glBindBuffer);
  CHECK_EQ(current.argument_since(pinned_at + 3u, 1u), 0u);
  const std::uint32_t pinned = current.sent();
  calls.buffer_data(pinned_target, 64, pages + 100, 0x88E4u);
  CHECK_EQ(current.sent(), pinned);
  CHECK_EQ(current.error(), gl_invalid_operation); }

TEST(manual_calls, an_atomic_counter_binding_distrusts_the_copies_of_buffers_a_shader_may_write)
{ Current                   current;
  ManualCalls              &calls = current.rig().root().manual();
  constexpr GLenum          atomic = 0x92C0u;
  av::glicd::BufferShadows &shadows = current.context().shadows();
  CHECK(shadows.trusted());
  calls.bind_buffer_indexed(atomic, 0u);
  CHECK(shadows.trusted());
  calls.bind_buffer_indexed(atomic, 3u);
  CHECK(!shadows.trusted()); }

TEST(manual_calls, the_client_attribute_stack_and_vertex_array_elements_follow_the_calls_of_the_current_context)
{ Current                    current;
  ManualCalls               &calls = current.rig().root().manual();
  av::glicd::ClientArrayState &arrays = current.context().arrays();
  constexpr std::uint32_t     vertex_bit = av::glicd::ClientArrayState::client_vertex_array_bit;
  calls.bind_buffer(array_buffer, 5u);
  calls.client_attributes_pushed(vertex_bit);
  calls.bind_buffer(array_buffer, 9u);
  calls.client_attributes_popped();
  CHECK_EQ(arrays.bound_buffer(array_buffer), 5u);
  calls.client_attributes_pushed(vertex_bit);
  calls.client_attributes_reset(av::glicd::ClientArrayState::client_pixel_store_bit);
  calls.bind_buffer(pixel_pack_buffer, 4u);
  calls.client_attributes_reset(av::glicd::ClientArrayState::client_pixel_store_bit);
  CHECK_EQ(arrays.bound_buffer(pixel_pack_buffer), 0u);
  calls.bind_vertex_array(3u);
  calls.vertex_array_elements(3u, 8u);
  CHECK_EQ(arrays.bound_buffer(element_array_buffer), 8u);
  CHECK(current.context().invariants_hold()); }

TEST(manual_calls, with_nothing_current_a_clear_a_staging_a_finish_and_the_attribute_stack_do_nothing)
{ GlIcdRig rig;
  rig.load_with_format();
  ManualCalls &calls = rig.root().manual();
  constexpr std::uint32_t gl_r8 = 0x8229u, gl_red = 0x1903u, gl_unsigned_byte = 0x1401u;
  const std::uint8_t      fill = 1u;
  std::uint8_t            pixels[4] = {};
  const std::uint32_t     ids_deleted[] = {3u};
  CHECK(!calls.clear_bound_buffer(array_buffer, gl_r8, gl_red, gl_unsigned_byte, &fill));
  CHECK(!calls.clear_bound_buffer_range(array_buffer, gl_r8, 0, 4, gl_red, gl_unsigned_byte, &fill));
  CHECK(!calls.clear_named_buffer(1u, gl_r8, gl_red, gl_unsigned_byte, &fill));
  CHECK(!calls.clear_named_buffer_range(1u, gl_r8, 0, 4, gl_red, gl_unsigned_byte, &fill));
  CHECK(!calls.stage_download(pixels, std::size_t{1} << 20u));
  calls.finish_download(pixels, sizeof pixels);
  calls.client_attributes_pushed(1u);
  calls.client_attributes_reset(1u);
  calls.vertex_array_elements(1u, 2u);
  calls.delete_transform_feedbacks(1, ids_deleted);
  calls.finish();
  CHECK_EQ(rig.kmt().commands(), 0u); }

TEST(manual_calls, without_an_arena_a_large_download_is_staged_and_finished_through_the_wrappers)
{ Current current(no_arena);
  ManualCalls &calls = current.rig().root().manual();
  const GLuint staged = 23u;
  current.rig().host().gl_bytes(reinterpret_cast<const std::uint8_t *>(&staged), sizeof staged);
  CHECK(calls.stage_download(bulk, 200u * 100u * 4u));
  CHECK_EQ(current.context().staging().name(), staged);
  for (std::uint32_t answer = 0u; answer < 2u; answer++) current.rig().host().gl_bytes(nullptr, 0u);
  calls.finish_download(bulk, 200u * 100u * 4u);
  CHECK_EQ(current.sent_since(current.sent() - 1u), ids::kCmd_glBindBuffer);
  CHECK(current.context().invariants_hold()); }

TEST(manual_calls, the_ext_buffer_calls_send_small_data_whole_and_keep_what_they_can_of_it)
{ Current                   current;
  ManualCalls              &calls = current.rig().root().manual();
  av::glicd::BufferShadows &shadows = current.context().shadows();
  std::uint8_t              zeros[16] = {};
  shadows.keep(4u, 0u, sizeof zeros, zeros);
  const std::uint8_t data[4] = {1u, 2u, 3u, 4u};
  const std::uint32_t before = current.sent();
  calls.named_buffer_sub_data_ext(4u, 4, sizeof data, data);
  CHECK(std::memcmp(shadows.covering(4u, 4u, 4u), data, sizeof data) == 0);
  calls.named_buffer_sub_data_ext(4u, -1, sizeof data, data);
  calls.named_buffer_sub_data_ext(4u, 0, -1, data);
  CHECK_EQ(shadows.covering(4u, 0u, 1u)[0], 0u);
  calls.named_buffer_data_ext(4u, sizeof data, data, 0x88E4u);
  calls.named_buffer_storage_ext(4u, sizeof data, data, 0u);
  const std::uint32_t expected[] = {ids::kCmd_glNamedBufferSubDataEXT, ids::kCmd_glNamedBufferSubDataEXT,
                                    ids::kCmd_glNamedBufferSubDataEXT, ids::kCmd_glNamedBufferDataEXT,
                                    ids::kCmd_glNamedBufferStorageEXT};
  CHECK_EQ(current.sent(), before + 5u);
  for (std::uint32_t index = 0u; index < 5u; index++) CHECK_EQ(current.sent_since(before + index), expected[index]);
  std::uint8_t seen[4] = {};
  current.rig().host().gl_bytes(data, sizeof data);
  calls.get_named_buffer_sub_data(4u, 0, sizeof seen, seen);
  CHECK(std::memcmp(seen, data, sizeof data) == 0);
  std::memset(seen, 0, sizeof seen);
  current.rig().host().gl_bytes(data, sizeof data);
  calls.get_named_buffer_sub_data_ext(4u, 0, sizeof seen, seen);
  CHECK(std::memcmp(seen, data, sizeof data) == 0);
  CHECK_EQ(current.sent_since(before + 5u), ids::kCmd_glGetNamedBufferSubData);
  CHECK_EQ(current.sent_since(before + 6u), ids::kCmd_glGetNamedBufferSubDataEXT); }

TEST(manual_calls_slices, a_slice_is_placed_by_storage_calls_and_a_plain_storage_call_leaves_the_slice_alone)
{ SlicedCurrent current;
  ManualCalls  &calls = current.calls();
  constexpr std::uint32_t storage_flags = av::glicd::BufferMapping::map_persistent | av::glicd::BufferMapping::map_write;
  constexpr std::uint32_t new_buffer = 8u, ext_buffer = 9u, named_buffer = 10u;
  calls.bind_buffer(array_buffer, new_buffer);
  current.rig().host().gl_slices(1u);
  calls.buffer_storage(array_buffer, 4096, nullptr, storage_flags);
  CHECK(current.context().slices().holds(new_buffer));
  current.rig().host().gl_slices(1u);
  calls.named_buffer_storage(named_buffer, 4096, nullptr, storage_flags);
  CHECK(current.context().slices().holds(named_buffer));
  current.rig().host().gl_slices(1u);
  calls.named_buffer_storage_ext(ext_buffer, 4096, nullptr, storage_flags);
  CHECK(current.context().slices().holds(ext_buffer));
  const std::uint32_t before = current.sent();
  calls.bind_buffer(array_buffer, plain_buffer);
  calls.buffer_storage(array_buffer, 4096, nullptr, 0u);
  CHECK(!current.context().slices().holds(plain_buffer));
  CHECK_EQ(current.sent(), before + 2u);
  calls.bind_buffer(0x1234u, 11u);
  calls.buffer_storage(0x1234u, 4096, nullptr, storage_flags);
  CHECK(!current.context().slices().holds(11u));
  CHECK_EQ(current.error(), gl_invalid_enum);
  CHECK(current.context().invariants_hold()); }

TEST(manual_calls, vertex_buffer_offsets_cross_as_eight_byte_elements)
{ Current current;
  const GLuint   buffers[2] = {4u, 5u};
  const GLintptr offsets[2] = {16, 32};
  const GLsizei  strides[2] = {12, 12};
  gen::Wrap_glBindVertexBuffers(0u, 2, buffers, offsets, strides);
  gen::Wrap_glBindBuffersRange(array_buffer, 0u, 2, buffers, offsets, offsets);
  gen::Wrap_glVertexArrayVertexBuffers(3u, 0u, 2, buffers, offsets, strides);
  CHECK_EQ(current.sent(), 3u);
  CHECK_EQ(current.sent_since(0u), ids::kCmd_glBindVertexBuffers);
  CHECK_EQ(current.sent_since(1u), ids::kCmd_glBindBuffersRange);
  CHECK_EQ(current.sent_since(2u), ids::kCmd_glVertexArrayVertexBuffers); }

TEST(manual_calls, a_negative_buffer_size_is_an_error_without_a_host_call)
{ Current current;
  const std::uint8_t data[4] = {};
  const std::uint32_t before = current.sent();
  gen::Wrap_glBufferData(array_buffer, -1, data, 0x88E4u);
  CHECK_EQ(current.error(), gl_invalid_value);
  gen::Wrap_glNamedBufferData(3u, -8, data, 0x88E4u);
  CHECK_EQ(current.error(), gl_invalid_value);
  gen::Wrap_glNamedBufferStorage(3u, -8, data, 0u);
  CHECK_EQ(current.error(), gl_invalid_value);
  gen::Wrap_glBufferStorage(array_buffer, -8, data, 0u);
  CHECK_EQ(current.error(), gl_invalid_value);
  CHECK_EQ(current.sent(), before); }
