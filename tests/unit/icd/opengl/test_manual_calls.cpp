/* The OpenGL entries the ICD writes itself, called as an application calls
 * them - by the addresses opengl32 hands out - on a context made current over
 * MockKmt: what each sends, what each keeps, and what each refuses with no
 * host call. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_context_rig.hpp"
#include "harness/icd/gl_icd_rig.hpp"
#include "harness/icd/scripted_thread_slot.hpp"
#include "icd/opengl/current_contexts.hpp"
#include "icd/opengl/gl_extension_policy.hpp"
#include "icd/opengl/gl_entry_points.hpp"
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
        commands_ = rig_.kmt().commands();
    }
    GlIcdRig                  &rig() { return rig_; }
    av::glicd::GuestGlContext &context() { return *rig_.root().current(); }
    /* The ids sent since the context was made, in order, as one comparable number list. */
    std::uint32_t sent_since(std::uint32_t index) { return rig_.kmt().command_id(commands_ + index); }
    /* The `nth` varint argument of the command sent `index`-th since the make. */
    std::uint64_t argument_since(std::uint32_t index, std::uint32_t nth)
    {
        av::WireReader reader(rig_.kmt().command_bytes(commands_ + index) + av::CommandHeader::encoded_size,
                              av::MockKmt::journal_bytes - av::CommandHeader::encoded_size);
        std::uint64_t  value = 0u;
        for (std::uint32_t at = 0u; at <= nth; at++) CHECK(reader.read_varint(value));
        return value;
    }
    std::uint32_t sent() { return rig_.kmt().commands() - commands_; }
    const std::uint8_t *command_since(std::uint32_t index) { return rig_.kmt().command_bytes(commands_ + index); }
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
  CHECK(std::strcmp(reinterpret_cast<const char *>(gen::Wrap_glGetString(0x1F00u)), "rvk host vendor") == 0);
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
  host.gl_integer(7);
  answer_size(host, sizeof data);
  host.gl_bytes(data, sizeof data);
  auto *const mapped = static_cast<std::uint8_t *>(gen::Wrap_glMapBuffer(array_buffer, gl_read_write));
  CHECK(mapped != nullptr);
  CHECK(std::memcmp(mapped, data, sizeof data) == 0);
  CHECK_EQ(current.sent(), 3u);
  CHECK_EQ(current.sent_since(0u), ids::kCmd_glGetIntegerv);
  CHECK_EQ(current.argument_since(0u, 0u), 0x8894u);
  CHECK_EQ(current.sent_since(1u), ids::kCmd_glGetNamedBufferParameteri64v);
  CHECK_EQ(current.argument_since(1u, 0u), 7u);
  CHECK_EQ(current.argument_since(1u, 1u), 0x8764u);
  CHECK_EQ(current.sent_since(2u), ids::kCmd_glGetNamedBufferSubData);
  CHECK_EQ(current.argument_since(2u, 2u), sizeof data);
  CHECK_EQ(current.rig().log().holding("buffer maps are guest copies"), 1u);
  CHECK(current.context().mappings().invariants_hold());
  host.gl_integer(7);
  void *pointer = nullptr;
  gen::Wrap_glGetBufferPointerv(array_buffer, gl_buffer_map_pointer, &pointer);
  CHECK(pointer == mapped);
  pointer = nullptr;
  gen::Wrap_glGetNamedBufferPointerv(7u, gl_buffer_map_pointer, &pointer);
  CHECK(pointer == mapped);
  CHECK_EQ(current.sent(), 4u);
  mapped[0] = 0xAAu;
  host.gl_integer(7);
  CHECK(gen::Wrap_glUnmapBuffer(array_buffer) == 1u);
  CHECK_EQ(current.sent(), 6u);
  CHECK_EQ(current.sent_since(5u), ids::kCmd_glNamedBufferSubData);
  CHECK_EQ(current.argument_since(5u, 0u), 7u);
  CHECK_EQ(current.argument_since(5u, 1u), 0u);
  CHECK_EQ(current.argument_since(5u, 2u), sizeof data);
  CHECK_EQ(current.context().mappings().held(), 0u);
  CHECK_EQ(current.error(), 0u); }

TEST(manual_calls, an_explicit_range_writes_back_only_what_was_flushed)
{ Current current;
  av::ScriptedHost &host = current.rig().host();
  host.gl_integer(7);
  answer_size(host, 16u);
  const GLbitfield explicit_write = 0x0002u | 0x0010u;
  auto *const mapped = static_cast<std::uint8_t *>(gen::Wrap_glMapBufferRange(array_buffer, 4, 8, explicit_write));
  CHECK(mapped != nullptr);
  CHECK_EQ(mapped[7], 0u);
  CHECK_EQ(current.sent(), 2u);
  host.gl_integer(7);
  gen::Wrap_glFlushMappedBufferRange(array_buffer, 1, 2);
  gen::Wrap_glFlushMappedNamedBufferRange(7u, 6, 1);
  CHECK(gen::Wrap_glUnmapNamedBuffer(7u) == 1u);
  CHECK_EQ(current.sent(), 4u);
  CHECK_EQ(current.sent_since(3u), ids::kCmd_glNamedBufferSubData);
  CHECK_EQ(current.argument_since(3u, 1u), 5u);
  CHECK_EQ(current.argument_since(3u, 2u), 6u);
  CHECK_EQ(current.error(), 0u); }

/* A persistent map's bytes reach the host before each hand-written draw. */
TEST(manual_calls, a_persistent_write_map_is_sent_before_every_draw)
{ Current current;
  av::ScriptedHost &host = current.rig().host();
  answer_size(host, 8u);
  const GLbitfield persistent_write = 0x0002u | 0x0040u;
  CHECK(gen::Wrap_glMapNamedBufferRange(3u, 0, 8, persistent_write) != nullptr);
  gen::Wrap_glDrawArrays(gl_triangles, 0, 3);
  gen::Wrap_glDrawElements(gl_triangles, 3, gl_unsigned_short, nullptr);
  gen::Wrap_glDrawRangeElements(gl_triangles, 0u, 2u, 3, gl_unsigned_short, nullptr);
  const std::uint32_t expected[] = {ids::kCmd_glGetNamedBufferParameteri64v, ids::kCmd_glNamedBufferSubData,
                                    ids::kCmd_glDrawArrays, ids::kCmd_glNamedBufferSubData,
                                    ids::kCmd_glDrawElements, ids::kCmd_glNamedBufferSubData,
                                    ids::kCmd_glDrawRangeElements};
  CHECK_EQ(current.sent(), 7u);
  for (std::uint32_t index = 0u; index < 7u; index++) CHECK_EQ(current.sent_since(index), expected[index]);
  /* Deleting the buffer unmaps it with nothing written. */
  const GLuint deleted[] = {3u};
  gen::Wrap_glDeleteBuffers(1, deleted);
  gen::Wrap_glDrawArrays(gl_triangles, 0, 3);
  CHECK_EQ(current.sent(), 9u);
  CHECK_EQ(current.sent_since(8u), ids::kCmd_glDrawArrays);
  CHECK_EQ(current.context().mappings().held(), 0u); }

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
  /* An access that is no enum, after the binding was asked. */
  host.gl_integer(7);
  CHECK(gen::Wrap_glMapBuffer(array_buffer, 0x1234u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_enum);
  CHECK(gen::Wrap_glMapNamedBuffer(7u, 0x1234u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_enum);
  host.gl_integer(7);
  CHECK(gen::Wrap_glMapBufferRange(array_buffer, 0, -1, 1u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_value);
  CHECK(gen::Wrap_glMapNamedBufferRange(7u, 0, -1, 1u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_value);
  CHECK_EQ(current.sent(), 2u);
  /* Nothing bound, nothing mapped. */
  host.gl_integer(0);
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
  CHECK_EQ(current.sent(), 3u); }

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
  calls.attribute_pointer(0u, 0x8645u, &pointer);
  calls.pointer(0x8244u, &pointer);
  CHECK(pointer == reinterpret_cast<void *>(0x1u));
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
  av::glicd::GuestGlContext context(rig.connection(), rig.platform(), rig.send_lock(), rig.log(), policy, 1,
                                    av::GlContextOptions());
  CHECK(calls.string_i(0x1F03u, 0u) == nullptr);
  rig.host().gl_make("host gpu", "4.6");
  rig.host().gl_integer(2);
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
  /* GL_READ_ONLY through a target: bound buffer, size, fill. */
  host.gl_integer(7);
  answer_size(host, 4);
  host.gl_bytes(nullptr, 0u);
  CHECK(gen::Wrap_glMapBuffer(array_buffer, 0x88B8u) != nullptr);
  CHECK_EQ(current.context().mappings().held(), 1u);
  /* GL_WRITE_ONLY by name: no fill. */
  answer_size(host, 4);
  CHECK(gen::Wrap_glMapNamedBuffer(8u, 0x88B9u) != nullptr);
  CHECK_EQ(current.sent(), 4u);
  CHECK_EQ(current.sent_since(3u), ids::kCmd_glGetNamedBufferParameteri64v);
  /* A target that binds no buffer. */
  CHECK(gen::Wrap_glMapBufferRange(0x1234u, 0, 4, 1u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_enum);
  /* No access bits, refused by the mapping. */
  CHECK(gen::Wrap_glMapNamedBufferRange(9u, 0, 4, 0u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_operation);
  /* The buffer bound at the target is not mapped. */
  host.gl_integer(10);
  CHECK(!gen::Wrap_glUnmapBuffer(array_buffer));
  CHECK_EQ(current.error(), gl_invalid_operation);
  CHECK_EQ(current.sent(), 5u);
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
  CHECK(gen::Wrap_glMapNamedBufferRange(3u, 0, 4, 0x0002u | 0x0040u) != nullptr);
  const GLenum buffers[] = {0x8CE0u};
  gen::Wrap_glDrawBuffers(1, buffers);
  CHECK_EQ(current.sent(), 4u);
  CHECK_EQ(current.sent_since(2u), ids::kCmd_glNamedBufferSubData);
  CHECK_EQ(current.sent_since(3u), ids::kCmd_glDrawBuffers);
  current.rig().kmt().withhold_answer_at(1u);
  GLint value = 0;
  gen::Wrap_glGetIntegerv(0x0BA2u, &value);
  const std::uint32_t lost = current.sent();
  gen::Wrap_glDrawBuffers(1, buffers);
  CHECK_EQ(current.sent(), lost); }
