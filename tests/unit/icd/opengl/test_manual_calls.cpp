/* The OpenGL entries the ICD writes itself, called as an application calls
 * them - by the addresses opengl32 hands out - on a context made current over
 * MockKmt: what each sends, what each keeps, and what each refuses with no
 * host call. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/gl_icd_rig.hpp"
#include "icd/opengl/gl_entry_points.hpp"
#include "icd/opengl/gl_error_query.hpp"
#include "icd/opengl/guest_gl_context.hpp"
#include "icd/opengl/manual_calls.hpp"

#include "gl_icd_entries.h"
#include "gl_protocol_ids.h"

#include <cstdint>
#include <cstring>

using av::GlIcdRig;
using av::glicd::ManualCalls;
namespace gen = av::glicd::generated;
namespace ids = av::glproto;

namespace av::glicd {

struct ManualCallsProbe {
    static void drop_extensions(ManualCalls &calls) { calls.no_extensions_ = nullptr; }
    static void name_extensions(ManualCalls &calls) { calls.no_extensions_ = "GL_ARB_nothing"; }
};

}  // namespace av::glicd

namespace {

constexpr GLenum gl_invalid_enum = 0x0500u, gl_invalid_value = 0x0501u, gl_invalid_operation = 0x0502u;
constexpr GLenum array_buffer = 0x8892u, element_array_buffer = 0x8893u, pixel_pack_buffer = 0x88EBu;
constexpr GLenum gl_triangles = 0x0004u, gl_unsigned_short = 0x1403u, gl_float = 0x1406u;

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
    std::uint32_t sent() { return rig_.kmt().commands() - commands_; }
    /* glGetError as opengl32's table holds it: the ICD's own entry. */
    std::uint32_t error() { return av::glicd::entry::get_error(); }

private:
    GlIcdRig      rig_;
    std::uint32_t commands_ = 0u;
};

}  // namespace

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
  for (std::uint32_t index = 0u; index < 7u; index++) CHECK_EQ(current.sent_since(index), expected[index]); }

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

TEST(manual_calls, nothing_is_mapped_until_buffer_mapping_arrives)
{ Current current;
  CHECK(gen::Wrap_glMapBuffer(array_buffer, 0x88B8u) == nullptr);
  CHECK_EQ(current.error(), gl_invalid_operation);
  CHECK(gen::Wrap_glMapBufferRange(array_buffer, 0, 4, 1u) == nullptr);
  CHECK(gen::Wrap_glMapNamedBuffer(1u, 0x88B8u) == nullptr);
  CHECK(gen::Wrap_glMapNamedBufferEXT(1u, 0x88B8u) == nullptr);
  CHECK(gen::Wrap_glMapNamedBufferRange(1u, 0, 4, 1u) == nullptr);
  CHECK(gen::Wrap_glMapNamedBufferRangeEXT(1u, 0, 4, 1u) == nullptr);
  CHECK(!gen::Wrap_glUnmapBuffer(array_buffer));
  CHECK(!gen::Wrap_glUnmapNamedBuffer(1u));
  CHECK(!gen::Wrap_glUnmapNamedBufferEXT(1u));
  gen::Wrap_glFlushMappedBufferRange(array_buffer, 0, 4);
  gen::Wrap_glFlushMappedNamedBufferRange(1u, 0, 4);
  gen::Wrap_glFlushMappedNamedBufferRangeEXT(1u, 0, 4);
  void *pointer = reinterpret_cast<void *>(0x1u);
  gen::Wrap_glGetBufferPointerv(array_buffer, 0x88BDu, &pointer);
  CHECK(pointer == nullptr);
  pointer = reinterpret_cast<void *>(0x1u);
  gen::Wrap_glGetNamedBufferPointerv(1u, 0x88BDu, &pointer);
  CHECK(pointer == nullptr);
  /* The first refusal is the one glGetError answers. */
  CHECK_EQ(current.error(), gl_invalid_operation);
  gen::Wrap_glGetBufferPointerv(array_buffer, 0x1234u, &pointer);
  CHECK_EQ(current.error(), gl_invalid_enum);
  gen::Wrap_glGetBufferPointerv(array_buffer, 0x88BDu, nullptr);
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
  CHECK(calls.invariants_hold());
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
  CHECK(calls.refuse_map() == nullptr);
  CHECK(!calls.refuse_unmap());
  calls.refuse();
  calls.buffer_pointer(0x88BDu, &pointer);
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

TEST(manual_calls_invariants, fail_when_an_extension_list_appears)
{ GlIcdRig dropped;
  av::glicd::ManualCallsProbe::drop_extensions(dropped.root().manual());
  CHECK(!dropped.root().manual().invariants_hold());
  GlIcdRig named;
  av::glicd::ManualCallsProbe::name_extensions(named.root().manual());
  CHECK(!named.root().manual().invariants_hold()); }

TEST(manual_calls, the_extension_count_is_answered_without_the_host)
{ Current current;
  GLint count = 9;
  av::glicd::entry::get_integerv(0x821Du /* GL_NUM_EXTENSIONS */, &count);
  CHECK_EQ(count, 0);
  av::glicd::entry::get_integerv(0x821Du, nullptr);
  CHECK_EQ(current.sent(), 0u); }
