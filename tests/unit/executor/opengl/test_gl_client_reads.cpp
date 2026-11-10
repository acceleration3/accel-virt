#include "executor/opengl/gl_client_reads.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <initializer_list>
#include <cstring>

using av::GlClientReads;

namespace {

constexpr std::uint64_t gl_unsigned_short = 0x1403u, gl_unsigned_byte = 0x1401u, gl_rgba = 0x1908u;

struct Calls {
    std::uint32_t generated = 0u, bound_pack = 0u, unbound = 0u, uploaded = 0u, read = 0u, fetched = 0u, deleted = 0u;
    GLsizeiptr    size = 0;
    const void   *read_target = &generated;
    GLint         pack_binding = 0;
};

Calls calls;

void fake_gen(GLsizei, GLuint *names) { calls.generated++; *names = 9u; }
void fake_bind(GLenum, GLuint buffer) { if (buffer == 0u) calls.unbound++; else calls.bound_pack++; }
void fake_data(GLenum, GLsizeiptr size, const void *, GLenum) { calls.uploaded++; calls.size = size; }
void fake_read(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *target) { calls.read++; calls.read_target = target; }
void fake_get(GLenum, GLintptr, GLsizeiptr size, void *data) { calls.fetched++; std::memset(data, 0xA5, static_cast<std::size_t>(size)); }
void fake_delete(GLsizei, const GLuint *) { calls.deleted++; }
void fake_integer(GLenum, GLint *value) { *value = calls.pack_binding; }

av::glproto::RealGl table()
{ calls = Calls{};
  av::glproto::RealGl gl;
  gl.glGenBuffers = fake_gen;
  gl.glBindBuffer = fake_bind;
  gl.glBufferData = fake_data;
  gl.glReadPixels = fake_read;
  gl.glGetBufferSubData = fake_get;
  gl.glDeleteBuffers = fake_delete;
  gl.glGetIntegerv = fake_integer;
  return gl; }

struct Read {
    std::uint8_t   bytes[64] = {};
    av::WireWriter writer{bytes, sizeof bytes};
    Read(std::uint64_t type, std::uint8_t present, std::uint64_t size)
    { for (const std::uint64_t value : {2u, 3u, 7u, 5u}) writer.write_varint(value);
      writer.write_varint(gl_rgba);
      writer.write_varint(type);
      writer.write_pod<std::uint8_t>(present);
      if (present != 0u) writer.write_varint(size); }
    av::WireReader reader() const { return av::WireReader(bytes, writer.offset()); }
};

}

TEST(gl_client_reads, only_the_types_the_host_driver_converts_wrongly_for_client_memory_are_wanted)
{ GlClientReads reads;
  CHECK(reads.wants(0x1402u));
  CHECK(reads.wants(0x1403u));
  CHECK(reads.wants(0x1404u));
  CHECK(reads.wants(0x1405u));
  CHECK(reads.wants(0x1406u));
  CHECK(reads.wants(0x140Bu));
  CHECK(!reads.wants(0x1401u));
  CHECK(!reads.wants(0x8035u)); }

TEST(gl_client_reads, a_client_read_goes_through_a_pack_buffer_and_the_bytes_come_back_in_the_reply)
{ GlClientReads reads;
  const av::glproto::RealGl gl = table();
  std::uint8_t block[4096];
  av::WireArena arena(block, sizeof block);
  std::uint8_t reply_bytes[256] = {};
  av::WireWriter reply(reply_bytes, sizeof reply_bytes);
  const Read read(gl_unsigned_short, 1u, 40u);
  CHECK(reads.run(gl, read.reader(), reply, arena));
  CHECK_EQ(calls.generated, 1u);
  CHECK_EQ(calls.bound_pack, 1u);
  CHECK_EQ(calls.unbound, 1u);
  CHECK_EQ(calls.uploaded, 1u);
  CHECK_EQ(calls.size, 40);
  CHECK_EQ(calls.read, 1u);
  CHECK(calls.read_target == nullptr);
  CHECK_EQ(calls.fetched, 1u);
  av::WireReader answer(reply_bytes, reply.offset());
  std::uint64_t  count = 0u;
  CHECK(answer.read_varint(count));
  CHECK_EQ(count, 40u);
  std::uint8_t data[40] = {};
  CHECK(answer.read(data, sizeof data));
  CHECK_EQ(data[0], 0xA5u);
  CHECK_EQ(data[39], 0xA5u);
  CHECK(reads.run(gl, read.reader(), reply, arena));
  CHECK_EQ(calls.generated, 1u);
  reads.release(gl);
  CHECK_EQ(calls.deleted, 1u);
  CHECK(reads.invariants_hold()); }

TEST(gl_client_reads, byte_reads_blob_reads_and_reads_into_an_application_pack_buffer_are_left_to_the_dispatch)
{ GlClientReads reads;
  const av::glproto::RealGl gl = table();
  std::uint8_t block[256];
  av::WireArena arena(block, sizeof block);
  std::uint8_t reply_bytes[64] = {};
  av::WireWriter reply(reply_bytes, sizeof reply_bytes);
  CHECK(!reads.run(gl, Read(gl_unsigned_byte, 1u, 40u).reader(), reply, arena));
  CHECK(!reads.run(gl, Read(gl_unsigned_short, 3u, 40u).reader(), reply, arena));
  CHECK(!reads.run(gl, Read(gl_unsigned_short, 2u, 0u).reader(), reply, arena));
  CHECK(!reads.run(gl, Read(gl_unsigned_short, 0u, 0u).reader(), reply, arena));
  CHECK(!reads.run(gl, Read(gl_unsigned_short, 1u, 0u).reader(), reply, arena));
  calls.pack_binding = 4;
  CHECK(!reads.run(gl, Read(gl_unsigned_short, 1u, 40u).reader(), reply, arena));
  CHECK_EQ(calls.read, 0u);
  CHECK_EQ(reply.offset(), 0u); }

TEST(gl_client_reads, a_driver_without_the_buffer_entry_points_is_left_alone)
{ GlClientReads reads;
  av::glproto::RealGl gl;
  std::uint8_t block[256];
  av::WireArena arena(block, sizeof block);
  std::uint8_t reply_bytes[64] = {};
  av::WireWriter reply(reply_bytes, sizeof reply_bytes);
  CHECK(!reads.run(gl, Read(gl_unsigned_short, 1u, 40u).reader(), reply, arena));
  reads.release(gl); }
