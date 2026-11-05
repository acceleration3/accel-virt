#include "executor/opengl/gl_error_queue.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"
#include "proto/submission/gl_error_word.hpp"

#include <cstdint>
#include <initializer_list>

using av::GlErrorQueue;
using av::GlErrorWord;

namespace {

constexpr std::uint32_t invalid_enum = 0x0500u;
constexpr std::uint32_t invalid_value = 0x0501u;
constexpr std::uint32_t context_lost = 0x0507u;

std::uint32_t scripted[16];
std::uint32_t scripted_count = 0u;
std::uint32_t scripted_taken = 0u;

void script(std::initializer_list<std::uint32_t> errors)
{
    scripted_count = 0u;
    scripted_taken = 0u;
    for (const std::uint32_t error : errors) scripted[scripted_count++] = error;
}

GLenum next_error() { return scripted_taken < scripted_count ? scripted[scripted_taken++] : 0u; }

}

TEST(gl_error_queue, takes_one_error_at_a_time_and_reads_none_when_the_driver_has_no_queue)
{ av::glproto::RealGl gl;
  GlErrorWord word;
  GlErrorQueue queue(gl, word);
  CHECK_EQ(queue.take(), 0u);
  CHECK(!queue.lost());
  gl.glGetError = next_error;
  script({invalid_enum, invalid_value});
  CHECK_EQ(queue.take(), invalid_enum);
  CHECK_EQ(queue.take(), invalid_value);
  CHECK_EQ(queue.take(), 0u);
  CHECK_EQ(word.count(0u), 0u);
  CHECK(queue.invariants_hold()); }

TEST(gl_error_queue, collects_what_the_driver_holds_into_the_word_up_to_a_bound)
{ av::glproto::RealGl gl;
  gl.glGetError = next_error;
  GlErrorWord word;
  GlErrorQueue queue(gl, word);
  script({invalid_enum, invalid_enum, invalid_value});
  queue.collect();
  CHECK(word.published());
  CHECK_EQ(word.count(0u), 2u);
  CHECK_EQ(word.count(1u), 1u);
  CHECK_EQ(scripted_taken, 3u);
  script({invalid_enum, invalid_enum, invalid_enum, invalid_enum, invalid_enum, invalid_enum, invalid_enum,
          invalid_enum, invalid_enum, invalid_enum});
  GlErrorWord bounded;
  GlErrorQueue limited(gl, bounded);
  limited.collect();
  CHECK_EQ(scripted_taken, GlErrorQueue::most_collected); }

TEST(gl_error_queue, records_an_error_of_its_own_and_notes_a_lost_context_until_recovered)
{ av::glproto::RealGl gl;
  gl.glGetError = next_error;
  GlErrorWord word;
  GlErrorQueue queue(gl, word);
  queue.record(invalid_value);
  CHECK_EQ(word.count(1u), 1u);
  script({context_lost});
  CHECK_EQ(queue.take(), context_lost);
  CHECK(queue.lost());
  CHECK(queue.invariants_hold());
  queue.recover();
  CHECK(!queue.lost()); }
