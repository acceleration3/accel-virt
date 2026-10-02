/* The OpenGL entries the ICD writes itself, called as an application calls
 * them - by the addresses opengl32 hands out - on a context made current over
 * MockKmt: what each sends, what each keeps, and what each refuses with no
 * host call. */
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
#include "harness/icd/scripted_thread_slot.hpp"
#include "icd/opengl/buffer_shadows.hpp"
#include "icd/opengl/current_contexts.hpp"
#include "icd/opengl/gl_extension_policy.hpp"
#include "icd/opengl/gl_entry_points.hpp"
#include "icd/opengl/gl_external_objects.hpp"
#include "icd/opengl/gl_error_query.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "icd/opengl/manual_calls.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/wire/wire_reader.hpp"

#include "gl_icd_entries.h"
#include "gl_protocol_ids.h"

#include <cstdint>
#include <cstring>

using av::GlIcdRig;
using av::glicd::ManualCalls;
namespace gen = av::glicd::generated;
namespace ids = av::glproto;

namespace {

constexpr GLenum gl_invalid_enum = 0x0500u, gl_invalid_value = 0x0501u, gl_invalid_operation = 0x0502u;
constexpr GLenum array_buffer = 0x8892u, element_array_buffer = 0x8893u, pixel_pack_buffer = 0x88EBu;
constexpr GLenum gl_triangles = 0x0004u, gl_unsigned_short = 0x1403u, gl_float = 0x1406u;
constexpr GLenum gl_read_write = 0x88BAu, gl_buffer_map_pointer = 0x88BDu;

/* GL_BUFFER_SIZE's answer. */
void answer_size(av::ScriptedHost &host, std::int64_t size)
{
    host.gl_bytes(reinterpret_cast<const std::uint8_t *>(&size), sizeof size);
}

class Current {
public:
    Current()
    {
        rig_.load_with_format();
        CHECK(rig_.current_context() != nullptr);
        if (rig_.current_context() != nullptr) context().flush();
        commands_ = rig_.kmt().commands();
    }
    GlIcdRig                  &rig() { return rig_; }
    av::glicd::GuestGlContext &context() { return *rig_.root().current(); }
    /* The ids sent since the context was made, in order, as one comparable number list. */
    std::uint32_t sent_since(std::uint32_t index)
    {
        context().flush();
        return rig_.kmt().command_id(commands_ + index);
    }
    /* The `nth` varint argument of the command sent `index`-th since the make. */
    std::uint64_t argument_since(std::uint32_t index, std::uint32_t nth)
    {
        context().flush();
        av::WireReader reader(rig_.kmt().command_bytes(commands_ + index) + av::CommandHeader::encoded_size,
                              av::MockKmt::journal_bytes - av::CommandHeader::encoded_size);
        std::uint64_t  value = 0u;
        for (std::uint32_t at = 0u; at <= nth; at++) CHECK(reader.read_varint(value));
        return value;
    }
    /* The context holds its calls in a batch until a flush: what reached the
     * host is read with the held ones sent. */
    std::uint32_t sent()
    {
        context().flush();
        return rig_.kmt().commands() - commands_;
    }
    const std::uint8_t *command_since(std::uint32_t index)
    {
        context().flush();
        return rig_.kmt().command_bytes(commands_ + index);
    }
    /* glGetError as opengl32's table holds it: the ICD's own entry. */
    std::uint32_t error() { return av::glicd::entry::get_error(); }

private:
    GlIcdRig      rig_;
    std::uint32_t commands_ = 0u;
};

/* The flags a glBufferStorage (a target) or glNamedBufferStorage (a name)
 * with 8 inline bytes sent. */
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

}  // namespace

/* Feature 051: the profile and version a context was granted are learned at
 * the make, so a query for them is answered from what the host already said -
 * one fewer round trip, and one answer whatever a driver does with a repeated
 * query. */
/* Feature 051: a read of the window's front buffer is answered from the frame
 * the swap put there; every other read goes to the host, and one with nothing
 * current goes nowhere. */
TEST(manual_calls, a_front_buffer_read_is_answered_and_every_other_is_sent)
{ Current      current;
  ManualCalls &calls = current.rig().root().manual();
  std::uint8_t pixels[64] = {};
  const std::uint32_t before = current.sent();
  /* Nothing presented and the read buffer is the back one: the host's. */
  current.rig().host().gl_bytes(nullptr, 0u);
  calls.read_pixels(0, 0, 1, 1, 0x80E1u /* GL_BGRA */, 0x1401u, pixels);
  CHECK_EQ(current.sent(), before + 1u);

  /* A frame on the window, and the front buffer named. */
  current.rig().host().gl_bytes(nullptr, 0u);
  CHECK(current.rig().opengl32().swap_buffers(current.rig().dc()));
  calls.read_buffer(0x0404u /* GL_FRONT */);
  const std::uint32_t asked = current.sent();
  calls.read_pixels(0, 0, 1, 1, 0x80E1u, 0x1401u, pixels);
  CHECK_EQ(current.sent(), asked);
  /* A format no present produced is still the host's. */
  current.rig().host().gl_bytes(nullptr, 0u);
  calls.read_pixels(0, 0, 1, 1, 0x1907u /* GL_RGB */, 0x1401u, pixels);
  CHECK(current.sent() > asked); }

/* With a pixel-pack buffer bound the read is an offset into it, and the room
 * it carries is the buffer's, not a count of bytes. */
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
  calls.integers(0x9126u /* GL_CONTEXT_PROFILE_MASK */, &mask);
  calls.integers(0x821Bu /* GL_MAJOR_VERSION */, &major);
  calls.integers(0x821Cu /* GL_MINOR_VERSION */, &minor);
  CHECK_EQ(mask, 2);
  CHECK_EQ(major, 4);
  CHECK_EQ(minor, 6);
  CHECK_EQ(current.sent(), before);
  /* A null pointer is a query nobody can be answered into. */
  calls.integers(0x9126u, nullptr);
  CHECK_EQ(current.sent(), before); }

TEST(manual_calls, pixel_store_is_kept_and_sent)
{ Current current;
  gen::Wrap_glPixelStorei(0x0D05u /* GL_PACK_ALIGNMENT */, 1);
  gen::Wrap_glPixelStoref(0x0D02u /* GL_PACK_ROW_LENGTH */, 2.6f);
  gen::Wrap_glPixelStoref(0x0D02u, -5.0f);
  gen::Wrap_glPixelStoref(0x0D02u, 3.0e10f);
  gen::Wrap_glPixelStoref(0x0D02u, -3.0e10f);
  CHECK_EQ(current.sent(), 5u);
  CHECK_EQ(current.sent_since(0u), ids::kCmd_glPixelStorei);
  CHECK_EQ(current.sent_since(1u), ids::kCmd_glPixelStoref);
  /* Rounded to 3, the -5 and the huge value not kept: a row of 3 pixels. */
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
  /* Immutable storage is dynamic on the host, where a map's bytes are written back. */
  CHECK_EQ(storage_flags(current, 4u), 0x0100u);
  CHECK_EQ(storage_flags(current, 6u), 0x0100u); }

TEST(manual_calls, a_client_side_array_is_invalid_operation_and_an_offset_is_sent)
{ Current current;
  gen::Wrap_glVertexAttribPointer(2u, 3, gl_float, 0u, 0, reinterpret_cast<const void *>(0x1000u));
  CHECK_EQ(current.sent(), 0u);
  CHECK_EQ(current.error(), gl_invalid_operation);
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

TEST(manual_calls, draws_send_and_client_indices_are_refused)
{ Current current;
  gen::Wrap_glDrawArrays(gl_triangles, 0, 3);
  gen::Wrap_glDrawArraysInstanced(gl_triangles, 0, 3, 2);
  const std::uint16_t indices[] = {0, 1, 2};
  gen::Wrap_glDrawElements(gl_triangles, 3, gl_unsigned_short, indices);
  gen::Wrap_glDrawElementsInstanced(gl_triangles, 3, gl_unsigned_short, indices, 2);
  gen::Wrap_glDrawRangeElements(gl_triangles, 0u, 2u, 3, gl_unsigned_short, indices);
  CHECK_EQ(current.sent(), 2u);
  CHECK_EQ(current.error(), gl_invalid_operation);
  gen::Wrap_glDrawElements(gl_triangles, 3, gl_unsigned_short, nullptr);
  gen::Wrap_glBindVertexArray(1u);
  gen::Wrap_glBindBuffer(element_array_buffer, 2u);
  gen::Wrap_glDrawElements(gl_triangles, 3, gl_unsigned_short, reinterpret_cast<const void *>(6u));
  gen::Wrap_glDrawElementsInstanced(gl_triangles, 3, gl_unsigned_short, nullptr, 4);
  gen::Wrap_glDrawRangeElements(gl_triangles, 0u, 2u, 3, gl_unsigned_short, nullptr);
  /* The refusal was answered locally: glGetError sent nothing. */
  const std::uint32_t expected[] = {ids::kCmd_glDrawArrays,       ids::kCmd_glDrawArraysInstanced,
                                    ids::kCmd_glDrawElements,     ids::kCmd_glBindVertexArray,
                                    ids::kCmd_glBindBuffer,       ids::kCmd_glDrawElements,
                                    ids::kCmd_glDrawElementsInstanced, ids::kCmd_glDrawRangeElements};
  CHECK_EQ(current.sent(), 8u);
  for (std::uint32_t index = 0u; index < 8u; index++) CHECK_EQ(current.sent_since(index), expected[index]); }

TEST(manual_calls, flush_wait_and_program_calls_reach_the_host)
{ Current current;
  gen::Wrap_glFlush();
  current.rig().host().gl_varint(0x911Au /* GL_CONDITION_SATISFIED */);
  CHECK_EQ(gen::Wrap_glClientWaitSync(reinterpret_cast<GLsync>(0x9u), 1u, 1000u), 0x911Au);
  current.rig().host().gl_varint(3u);
  CHECK_EQ(gen::Wrap_glGetAttribLocation(5u, "position"), 3);
  gen::Wrap_glLinkProgram(5u);
  gen::Wrap_glDeleteProgram(5u);
  const std::uint32_t expected[] = {ids::kCmd_glFlush, ids::kCmd_glClientWaitSync, ids::kCmd_glGetAttribLocation,
                                    ids::kCmd_glLinkProgram, ids::kCmd_glDeleteProgram};
  CHECK_EQ(current.sent(), 5u);
  for (std::uint32_t index = 0u; index < 5u; index++) CHECK_EQ(current.sent_since(index), expected[index]); }

/* A location's type is asked at once, so a later glGetUniform sizes its
 * answer; relinking or deleting the program forgets it. */
TEST(manual_calls, a_uniform_location_learns_its_shape)
{ Current current;
  current.rig().host().gl_varint(4u);
  current.rig().host().gl_varint(0u);
  current.rig().host().gl_resource_property(0x8B5C /* GL_FLOAT_MAT4 */);
  CHECK_EQ(gen::Wrap_glGetUniformLocation(5u, "transform"), 4);
  CHECK_EQ(current.context().uniforms().values(5u, 4), 16u);
  const std::uint32_t expected[] = {ids::kCmd_glGetUniformLocation, ids::kCmd_glGetProgramResourceIndex,
                                    ids::kCmd_glGetProgramResourceiv};
  for (std::uint32_t index = 0u; index < 3u; index++) CHECK_EQ(current.sent_since(index), expected[index]);
  gen::Wrap_glLinkProgram(5u);
  CHECK_EQ(current.context().uniforms().values(5u, 4), 0u);
  /* No location, an index the host does not have, and a property not written. */
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

/* A target names the buffer the host has bound there; the map is a copy the
 * host filled, and the unmap sends it back. */
TEST(manual_calls, a_target_map_is_the_hosts_bound_buffer_filled_and_written_back)
{ Current current;
  av::ScriptedHost &host = current.rig().host();
  const std::uint8_t data[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  /* The guest saw the bind, so the host is not asked what is bound. */
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
  /* Only the word that changed from the fill goes back. */
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

/* What a persistent map wrote reaches the host before each hand-written draw;
 * a draw with nothing new written sends nothing ahead of it. */
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
  /* Deleting the buffer unmaps it with nothing written. */
  const GLuint deleted[] = {3u};
  gen::Wrap_glDeleteBuffers(1, deleted);
  gen::Wrap_glDrawArrays(gl_triangles, 0, 3);
  CHECK_EQ(current.sent(), 11u);
  CHECK_EQ(current.sent_since(10u), ids::kCmd_glDrawArrays);
  CHECK_EQ(current.context().mappings().held(), 0u); }

/* A tracer learns what an application wrote by asking, at unmap, how the
 * buffer is mapped: the guest answers, since the host never saw the map. A
 * name that is not map state still goes to the host, and a target is resolved
 * only for a map-state name. */
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
  /* None of them needed a round trip. */
  CHECK_EQ(current.sent(), sent);
  /* Any other name is the host's, and needs no binding resolved first. */
  const std::int32_t host_size = 64;
  host.gl_bytes(reinterpret_cast<const std::uint8_t *>(&host_size), sizeof host_size);
  gen::Wrap_glGetBufferParameteriv(array_buffer, buffer_size, &value);
  CHECK_EQ(value, 64);
  CHECK_EQ(current.sent() - sent, 1u);
  CHECK_EQ(current.sent_since(sent), ids::kCmd_glGetBufferParameteriv);
  /* No error of the guest's: the host is asked for its own. */
  host.gl_varint(0u);
  CHECK_EQ(current.error(), 0u);
  /* A target binding nothing, or no target at all, is refused here. */
  gen::Wrap_glBindBuffer(array_buffer, 0u);
  gen::Wrap_glGetBufferParameteriv(array_buffer, map_length, &value);
  CHECK_EQ(current.error(), gl_invalid_operation);
  gen::Wrap_glGetBufferParameteriv(0x1234u, map_length, &value);
  CHECK_EQ(current.error(), gl_invalid_enum);
  /* Buffer 0 by name is the host's to refuse. */
  const std::uint32_t before_zero = current.sent();
  const std::int32_t  host_zero = 0;
  host.gl_bytes(reinterpret_cast<const std::uint8_t *>(&host_zero), sizeof host_zero);
  gen::Wrap_glGetNamedBufferParameteriv(0u, map_length, &value);
  CHECK_EQ(current.sent() - before_zero, 1u);
  CHECK_EQ(current.sent_since(before_zero), ids::kCmd_glGetNamedBufferParameteriv); }

TEST(manual_calls, map_refusals_are_recorded)
{ Current current;
  av::ScriptedHost &host = current.rig().host();
  /* A target that binds no buffer: no host call. */
  CHECK(gen::Wrap_glMapBuffer(0x1234u, gl_read_write) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_enum);
  CHECK(!gen::Wrap_glUnmapBuffer(0x1234u));
  gen::Wrap_glFlushMappedBufferRange(0x1234u, 0, 1);
  void *pointer = reinterpret_cast<void *>(0x1u);
  gen::Wrap_glGetBufferPointerv(0x1234u, gl_buffer_map_pointer, &pointer);
  CHECK(pointer == reinterpret_cast<void *>(0x1u));
  CHECK_EQ(current.error(), gl_invalid_enum);
  CHECK_EQ(current.sent(), 0u);
  /* An access that is no enum, with a buffer bound. */
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
  /* Nothing bound, nothing mapped. */
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
  /* Bound several at once, the uniform target is not the guest's to know:
   * the host is asked. */
  const GLuint several[] = {7u};
  gen::Wrap_glBindBuffersBase(0x8A11u, 0u, 1, several);
  host.gl_integer(7);
  CHECK(!gen::Wrap_glUnmapBuffer(0x8A11u));
  CHECK_EQ(current.error(), gl_invalid_operation);
  CHECK_EQ(current.sent(), 4u);
  CHECK_EQ(current.sent_since(3u), ids::kCmd_glGetIntegerv);
  CHECK_EQ(current.argument_since(3u, 0u), 0x8A28u); }

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

TEST(manual_calls, legacy_client_arrays_are_invalid_operation_without_a_host_call)
{ Current current;
  const float data[4] = {};
  gen::Wrap_glVertexPointer(2, gl_float, 0, data);
  CHECK_EQ(current.error(), gl_invalid_operation);
  gen::Wrap_glColorPointer(4, gl_float, 0, data);
  gen::Wrap_glNormalPointer(gl_float, 0, data);
  gen::Wrap_glTexCoordPointer(2, gl_float, 0, data);
  gen::Wrap_glIndexPointer(gl_float, 0, data);
  gen::Wrap_glEdgeFlagPointer(0, data);
  gen::Wrap_glFogCoordPointer(gl_float, 0, data);
  gen::Wrap_glSecondaryColorPointer(3, gl_float, 0, data);
  gen::Wrap_glInterleavedArrays(0x2A20u, 0, data);
  gen::Wrap_glArrayElement(0);
  gen::Wrap_glEnableClientState(0x8074u);
  gen::Wrap_glDisableClientState(0x8074u);
  gen::Wrap_glClientActiveTexture(0x84C0u);
  CHECK_EQ(current.error(), gl_invalid_operation);
  CHECK_EQ(current.sent(), 0u); }

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
  /* No context, no map state to answer: the host would be asked, and there
   * is no host either. */
  std::int64_t state = 7;
  CHECK(!calls.buffer_map_state(array_buffer, 0x9120u, state));
  CHECK(!calls.named_buffer_map_state(1u, 0x9120u, state));
  CHECK_EQ(state, 7);
  calls.attribute_pointer(0u, 0x8645u, &pointer);
  calls.pointer(0x8244u, &pointer);
  CHECK(pointer == reinterpret_cast<void *>(0x1u));
  calls.wrote_buffer(array_buffer, 0, 1, &value);
  calls.wrote_named_buffer(1u, 0, 1, &value);
  calls.host_writes_target(array_buffer);
  calls.host_writes_buffer(1u);
  calls.bind_buffer_indexed(0x90D2u, 1u);
  calls.bind_buffers_indexed(0x90D2u);
  calls.shaders_write_buffers();
  calls.bindings_restored();
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
  av::glicd::entry::get_integerv(0x821Du /* GL_NUM_EXTENSIONS */, &count);
  CHECK_EQ(count, 0);
  av::glicd::entry::get_integerv(0x821Du, nullptr);
  CHECK_EQ(current.sent(), 0u); }

/* A context whose host lists a reported extension answers it by count, by
 * index and joined, all without the host; an index past it and any other
 * indexed name are the GL's errors. */
TEST(manual_calls, the_extensions_answered_are_the_context_s_list)
{ static constexpr av::glicd::GlExtensionPolicy::Row rows[] = {
      {"GL_ARB_debug_output", 1u, "manual_calls.the_extensions_answered_are_the_context_s_list"}};
  av::GlContextRig rig;
  const av::glicd::GlExtensionPolicy policy(rows, 1u, nullptr, 0u);
  av::ScriptedThreadSlot slot;
  av::glicd::CurrentContexts currents(slot);
  ManualCalls calls(currents);
  av::glicd::GuestGlContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.log(), policy, rig.handles(), 1,
                                    av::GlContextOptions());
  CHECK(calls.string_i(0x1F03u, 0u) == nullptr);
  rig.host().gl_make("host gpu", "4.6");
  /* This context's own policy has rows, and the host lists two. */
  rig.host().gl_policy(2);
  rig.host().gl_string("GL_EXT_host_only");
  rig.host().gl_string("GL_ARB_debug_output");
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

/* bind_framebuffer sends framebuffer 0 as the window's framebuffer object (100
 * in GlIcdRig); draw_buffer and read_buffer send GL_BACK as its colour
 * attachment while the window is bound; a query answers as the window would. */
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
  /* A query with nowhere to answer is still the host's to answer. */
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

/* A whole level's download is sized from the host's answer for the level:
 * level_bytes asks its width, height and depth, compressed_level_bytes its
 * compressed size. */
TEST(manual_calls, a_level_download_is_sized_by_the_hosts_level)
{ Current current;
  av::ScriptedHost &host = current.rig().host();
  host.gl_integer(3);
  host.gl_integer(2);
  host.gl_integer(1);
  const std::uint32_t before = current.sent();
  std::uint8_t pixels[24] = {};
  host.gl_bytes(nullptr, 0u);
  gen::Wrap_glGetTexImage(0x0DE1u, 1, 0x1908u /* GL_RGBA */, 0x1401u, pixels);
  CHECK_EQ(current.sent() - before, 4u);
  CHECK_EQ(current.sent_since(before), ids::kCmd_glGetTexLevelParameteriv);
  CHECK_EQ(current.argument_since(before, 1u), 1u);
  CHECK_EQ(current.argument_since(before, 2u), 0x1000u);
  CHECK_EQ(current.argument_since(before + 1u, 2u), 0x1001u);
  CHECK_EQ(current.argument_since(before + 2u, 2u), 0x8071u);
  CHECK_EQ(current.sent_since(before + 3u), ids::kCmd_glGetTexImage);
  /* Default pack alignment 4: two rows of 3 RGBA pixels. */
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
  /* GL_READ_ONLY through a target: size and fill, the binding the guest's. */
  gen::Wrap_glBindBuffer(array_buffer, 7u);
  answer_size(host, 4);
  host.gl_bytes(nullptr, 0u);
  CHECK(gen::Wrap_glMapBuffer(array_buffer, 0x88B8u) != nullptr);
  CHECK_EQ(current.context().mappings().held(), 1u);
  /* GL_WRITE_ONLY by name: filled too, since what it does not write is kept. */
  answer_size(host, 4);
  host.gl_bytes(nullptr, 0u);
  CHECK(gen::Wrap_glMapNamedBuffer(8u, 0x88B9u) != nullptr);
  CHECK_EQ(current.sent(), 5u);
  CHECK_EQ(current.sent_since(3u), ids::kCmd_glGetNamedBufferParameteri64v);
  CHECK_EQ(current.sent_since(4u), ids::kCmd_glGetNamedBufferSubData);
  /* A target that binds no buffer. */
  CHECK(gen::Wrap_glMapBufferRange(0x1234u, 0, 4, 1u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_enum);
  /* No access bits, refused by the mapping. */
  CHECK(gen::Wrap_glMapNamedBufferRange(9u, 0, 4, 0u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_operation);
  /* The buffer bound at the target is not mapped. */
  gen::Wrap_glBindBuffer(array_buffer, 10u);
  CHECK(!gen::Wrap_glUnmapBuffer(array_buffer));
  CHECK_EQ(current.error(), gl_invalid_operation);
  CHECK_EQ(current.sent(), 6u);
  CHECK_EQ(current.context().mappings().held(), 2u); }

/* The generated entries' hooks: framebuffer 0 is the window's, and a draw
 * sends what persistent maps wrote first - with nothing current, and on a
 * lost context, nothing. */
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

/* Feature 053, T036. A memory object and a semaphore are made by the device
 * inside the import, so a create asks the host nothing and hands out a name of
 * this ICD's own - and every command that names one afterwards has to put the
 * host's name in its place, because a GL name on this wire is the host's and
 * nothing else translates one. A name that was never imported names no host
 * object, so it is refused rather than sent under a number the host holds
 * something else at. */
TEST(manual_calls, an_imported_objects_name_is_the_hosts_by_the_time_it_crosses)
{ av::GlContextRig rig;
  const av::glicd::GlExtensionPolicy policy(nullptr, 0u, nullptr, 0u);
  av::ScriptedThreadSlot slot;
  av::glicd::CurrentContexts currents(slot);
  ManualCalls calls(currents);
  av::glicd::GuestGlContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.log(), policy,
                                    rig.handles(), 1, av::GlContextOptions());
  rig.host().gl_make("host gpu", "4.6");
  CHECK_STATUS(context.make(), av::Status::ok);
  CHECK(currents.make_current(context));

  /* The create sends nothing: there is no host object to name yet. */
  const std::uint32_t quiet = rig.kmt().commands();
  GLuint              memory = 0u;
  calls.create_memory_objects(1, &memory);
  CHECK(memory != 0u);
  CHECK_EQ(rig.kmt().commands(), quiet);
  CHECK_EQ(context.take_error(), 0u);

  /* Used before it is imported, it names nothing and is refused here. */
  calls.buffer_storage_mem(0x8892u, 4096, memory, 0u);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  CHECK_EQ(rig.kmt().commands(), quiet);

  /* A handle this process never held is refused before the device. */
  calls.import_memory_win32(memory, 4096u, 0x9587u, reinterpret_cast<void *>(0xDEADu));
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  CHECK_EQ(rig.kmt().commands(), quiet);

  /* A semaphore is the other namespace: the same number is not this object. */
  GLuint semaphore = 0u;
  calls.gen_semaphores(1, &semaphore);
  CHECK(semaphore != 0u);
  calls.wait_semaphore(semaphore, 0u, nullptr, 0u, nullptr, nullptr);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  calls.signal_semaphore(memory, 0u, nullptr, 0u, nullptr, nullptr);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  CHECK_EQ(rig.kmt().commands(), quiet);

  /* Deleting a name that was never imported has nothing to release. */
  calls.delete_memory_objects(1, &memory);
  calls.delete_semaphores(1, &semaphore);
  CHECK_EQ(rig.kmt().commands(), quiet);
  CHECK_EQ(context.take_error(), 0u); }

/* With nothing current every one of them does nothing, as OpenGL does. */
TEST(manual_calls, the_external_object_entries_do_nothing_with_no_context)
{ av::ScriptedThreadSlot slot;
  av::glicd::CurrentContexts currents(slot);
  ManualCalls calls(currents);
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
  /* And the ones T037 added, which an application can reach as soon as the
   * extension is reported: with nothing current there is no table to ask. */
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

/* The substitution itself: an object imported from a handle this process holds
 * crosses under the *host's* name, never the one the application was given. */
TEST(manual_calls, an_import_puts_the_hosts_name_on_the_wire_and_a_delete_releases_the_object)
{ av::GlContextRig rig;
  const av::glicd::GlExtensionPolicy policy(nullptr, 0u, nullptr, 0u);
  av::ScriptedThreadSlot slot;
  av::glicd::CurrentContexts currents(slot);
  ManualCalls calls(currents);
  av::glicd::GuestGlContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.log(), policy,
                                    rig.handles(), 1, av::GlContextOptions());
  rig.host().gl_make("host gpu", "4.6");
  CHECK_STATUS(context.make(), av::Status::ok);
  CHECK(currents.make_current(context));

  /* A handle another process could have sent: an allocation the kernel minted
   * an identity into, shared, and adopted into this process's table. */
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

  /* Now the use: the body names the host's object, not the application's. */
  calls.buffer_storage_mem(0x8892u, 4096, memory, 0u);
  CHECK_EQ(context.take_error(), 0u);
  context.flush();   /* the call is held in the context's batch until a flush */
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

  /* And the delete releases the object rather than naming it to the host. */
  rig.host().shared_object(0u, 0u);
  calls.delete_memory_objects(1, &memory);
  CHECK_EQ(rig.kmt().command_id(rig.kmt().commands() - 1u), av::CommandHeader::release_shared_object_id);
  /* Released, the name names nothing again. */
  calls.buffer_storage_mem(0x8892u, 4096, memory, 0u);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation}); }

namespace {

/* A Win32 handle this process may open: an allocation the kernel minted an
 * identity into, shared, and adopted into the ICD's own table - the same three
 * steps a second process's handle arrives through. */
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

}  // namespace

/* Feature 053, T036. The other half of the substitution: a name that *was*
 * imported names a host object, and every command carrying it must carry the
 * host's number instead of this ICD's. The refusals are tested above; this is
 * what happens when there is nothing to refuse. */
TEST(manual_calls, an_imported_name_crosses_as_the_hosts_own_on_every_command_that_takes_one)
{ av::GlContextRig rig;
  const av::glicd::GlExtensionPolicy policy(nullptr, 0u, nullptr, 0u);
  av::ScriptedThreadSlot slot;
  av::glicd::CurrentContexts currents(slot);
  ManualCalls calls(currents);
  av::glicd::GuestGlContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.log(), policy,
                                    rig.handles(), 1, av::GlContextOptions());
  rig.host().gl_make("host gpu", "4.6");
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

  /* Each of the three commands that names one goes out with the host's number,
   * which is the first argument of each. */
  context.flush();
  std::uint32_t before = rig.kmt().commands();
  CHECK(context.shadows().trusted());
  calls.buffer_storage_mem(array_buffer, 4096, memory, 0u);
  CHECK_EQ(context.take_error(), 0u);
  /* Another API writes the memory whenever it likes. */
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

/* Every storage command GL_EXT_memory_object has, not the two this feature
 * started with: a row promises all of them, so all of them substitute the
 * host's name and none may reach the host under the application's (feature
 * 053, T037). The last of each pair is a name this context never imported,
 * which must be refused rather than sent - the host's number under it belongs
 * to another application's object, or to nothing. */
TEST(manual_calls, every_storage_command_names_the_hosts_memory_object)
{ av::GlContextRig rig;
  const av::glicd::GlExtensionPolicy policy(nullptr, 0u, nullptr, 0u);
  av::ScriptedThreadSlot slot;
  av::glicd::CurrentContexts currents(slot);
  ManualCalls calls(currents);
  av::glicd::GuestGlContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.log(), policy,
                                    rig.handles(), 1, av::GlContextOptions());
  rig.host().gl_make("host gpu", "4.6");
  CHECK_STATUS(context.make(), av::Status::ok);
  CHECK(currents.make_current(context));
  /* The calls are held in the context's batch until a flush. */
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

  /* Every one of the ten, and the one this feature already had beside them.
   * Each is checked the same way: the command went out, and the name in it is
   * the host's rather than the application's. */
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

  /* The host's name is what each carried: read out of the named-buffer one,
   * whose memory argument is its third - buffer, size, memory, offset. */
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

  /* And a name this context never imported reaches none of them. */
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

/* The six entry points that never cross: what an application asks about an
 * object whose host counterpart does not exist yet, answered by the ICD's own
 * table. A row promises these as much as it promises the storage commands. */
TEST(manual_calls, the_queries_and_parameters_are_answered_without_the_host)
{ av::GlContextRig rig;
  const av::glicd::GlExtensionPolicy policy(nullptr, 0u, nullptr, 0u);
  av::ScriptedThreadSlot slot;
  av::glicd::CurrentContexts currents(slot);
  ManualCalls calls(currents);
  av::glicd::GuestGlContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.log(), policy,
                                    rig.handles(), 1, av::GlContextOptions());
  rig.host().gl_make("host gpu", "4.6");
  CHECK_STATUS(context.make(), av::Status::ok);
  CHECK(currents.make_current(context));

  GLuint memory = 0u;
  GLuint semaphore = 0u;
  calls.create_memory_objects(1, &memory);
  calls.gen_semaphores(1, &semaphore);
  const std::uint32_t quiet = rig.kmt().commands();

  /* Created, they are objects - before anything is imported, which is what
   * the extension says and what the host cannot answer. */
  CHECK(calls.is_memory_object(memory));
  CHECK(calls.is_semaphore(semaphore));
  CHECK(!calls.is_memory_object(semaphore));
  CHECK(!calls.is_semaphore(memory));
  CHECK(!calls.is_memory_object(memory + 0x1000u));

  /* The parameters are remembered, and the import is what refuses them. */
  const GLint dedicated = 1;
  calls.memory_object_parameter(memory, av::glicd::GlExternalObjects::dedicated, &dedicated);
  CHECK_EQ(context.take_error(), 0u);
  GLint read = 0;
  calls.get_memory_object_parameter(memory, av::glicd::GlExternalObjects::dedicated, &read);
  CHECK_EQ(context.take_error(), 0u);
  CHECK_EQ(read, 1);
  calls.import_memory_win32(memory, 4096u, av::glicd::GlExternalObjects::opaque_win32,
                            openable_handle(rig));
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});

  /* A semaphore has no parameter this path can keep, and a pname the
   * extension does not define is refused for either kind. */
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
  /* And a query with nowhere to write, which the API leaves to the driver. */
  calls.get_memory_object_parameter(memory, av::glicd::GlExternalObjects::dedicated, nullptr);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_value});
  calls.memory_object_parameter(memory, av::glicd::GlExternalObjects::dedicated, nullptr);
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_value});

  /* A named section is what another process opens by name; this path shares by
   * handle and says so. */
  calls.import_memory_win32_name(memory, 4096u, av::glicd::GlExternalObjects::opaque_win32, "shared");
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});
  calls.import_semaphore_win32_name(semaphore, av::glicd::GlExternalObjects::opaque_win32, "shared");
  CHECK_EQ(context.take_error(), std::uint32_t{ManualCalls::gl_invalid_operation});

  /* Not one of them reached the device. */
  CHECK_EQ(rig.kmt().commands(), quiet); }

/* An argument no GL call can have, on each of the four entries that takes a
 * count: the table refuses it and the context keeps the error, because these
 * entries answer nothing and glGetError is the only way an application hears
 * about it. */
TEST(manual_calls, a_count_no_call_can_have_is_kept_as_an_error_on_each_entry)
{ av::GlContextRig rig;
  const av::glicd::GlExtensionPolicy policy(nullptr, 0u, nullptr, 0u);
  av::ScriptedThreadSlot slot;
  av::glicd::CurrentContexts currents(slot);
  ManualCalls calls(currents);
  av::glicd::GuestGlContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.log(), policy,
                                    rig.handles(), 1, av::GlContextOptions());
  rig.host().gl_make("host gpu", "4.6");
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
  /* And a semaphore import of a handle this process never held, which is the
   * one refusal that entry has of its own. */
  calls.import_semaphore_win32(1u, av::glicd::GlExternalObjects::opaque_win32,
                               reinterpret_cast<void *>(0xDEADu));
  CHECK_EQ(context.take_error(), std::uint32_t{gl_invalid_value});
  CHECK_EQ(rig.kmt().commands(), quiet); }

/* A command the host runs that writes a buffer forgets the guest's copy of
 * it - the buffer named, or bound at the target named - so the next map of it
 * starts from the host's bytes. A target whose binding is not known forgets
 * every copy. */
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
  gen::Wrap_glGetQueryBufferObjectiv(1u, 6u, 0x8866u /* GL_QUERY_RESULT */, 0);
  CHECK(!kept(6u));
  /* A download into the pack buffer, by hand and generated. */
  calls.bind_buffer(pixel_pack_buffer, 7u);
  current.rig().host().gl_bytes(nullptr, 0u);
  calls.read_pixels(0, 0, 1, 1, rgba, ubyte, nullptr);
  CHECK(!kept(7u));
  shadows.keep(7u, 0u, sizeof bytes, bytes);
  current.rig().host().gl_bytes(nullptr, 0u);
  gen::Wrap_glGetCompressedTexImage(0x0DE1u, 0, nullptr);
  CHECK(!kept(7u));
  /* A query's result into the query buffer. */
  calls.bind_buffer(0x9192u /* GL_QUERY_BUFFER */, 9u);
  current.rig().host().gl_bytes(nullptr, 0u);
  gen::Wrap_glGetQueryObjectiv(1u, 0x8866u, nullptr);
  CHECK(!kept(9u));
  CHECK(kept(1u));
  CHECK(kept(8u));
  /* Bound several at once, the uniform target is not known. */
  const GLuint several[] = {9u};
  gen::Wrap_glBindBuffersBase(uniform, 0u, 1, several);
  CHECK(shadows.trusted());
  gen::Wrap_glClearBufferData(uniform, rgba8, rgba, ubyte, nullptr);
  CHECK_EQ(shadows.held(), 0u);
  CHECK(shadows.trusted()); }

/* glBufferSubData's bytes go into the copy of the buffer bound, whether by
 * glBindBuffer or by an indexed bind; a range OpenGL refuses changes nothing,
 * and a target not known forgets every copy. */
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
  /* Refused by OpenGL: nothing written. */
  gen::Wrap_glBufferSubData(uniform, -1, sizeof data, data);
  gen::Wrap_glBufferSubData(uniform, 0, -1, data);
  gen::Wrap_glNamedBufferSubData(4u, -1, sizeof data, data);
  gen::Wrap_glNamedBufferSubData(4u, 0, -1, data);
  CHECK_EQ(shadows.covering(3u, 0u, 1u)[0], 0u);
  CHECK_EQ(shadows.covering(4u, 0u, 1u)[0], 0u);
  /* An indexed bind binds the target too. */
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

/* A buffer a shader can write - storage, atomic counters, transform feedback,
 * an image unit, imported memory - changes out of the guest's sight, so the
 * context keeps no copies from then on. Binding nothing there, or a buffer
 * where shaders only read, keeps them. */
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
    gen::Wrap_glBindImageTexture(0u, 1u, 0, 0u, 0, 0x88B9u /* GL_WRITE_ONLY */, 0x8058u);
    CHECK(!current.context().shadows().trusted());
  }
  {
    Current current;
    gen::Wrap_glBeginTransformFeedback(0x0000u /* GL_POINTS */);
    CHECK(!current.context().shadows().trusted());
  }
  {
    Current current;
    gen::Wrap_glTransformFeedbackBufferBase(1u, 0u, 5u);
    CHECK(!current.context().shadows().trusted());
  } }

/* New storage drops the copy and, made with no data, is fresh: its first map
 * has nothing to keep. A deletion forgets the copy, and bindings restored out
 * of sight forget every copy. */
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
  /* Storage for a target not known: nothing is known. */
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
  calls.bindings_restored();
  CHECK_EQ(shadows.held(), 0u);
  CHECK(shadows.trusted()); }
