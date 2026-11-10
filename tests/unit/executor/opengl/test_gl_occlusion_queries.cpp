#include "executor/opengl/gl_occlusion_queries.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/opengl/gl_protocol_ids.h"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>

using av::GlOcclusionQueries;

namespace {

struct Payload {
    std::uint8_t   bytes[16] = {};
    av::WireWriter writer{bytes, sizeof bytes};
    explicit Payload(std::uint64_t target) { writer.write_varint(target); }
    av::WireReader reader() const { return av::WireReader(bytes, writer.offset()); }
};

constexpr std::uint64_t samples_passed = 0x8914u, any_samples_passed = 0x8C2Fu, conservative = 0x8D6Au,
                        time_elapsed = 0x88BFu;

}

TEST(gl_occlusion_queries, an_occlusion_query_is_open_from_its_begin_to_its_end)
{ GlOcclusionQueries queries;
  CHECK(!queries.open());
  const Payload samples(samples_passed);
  queries.ran(av::glproto::kCmd_glBeginQuery, samples.reader());
  CHECK(queries.open());
  queries.ran(av::glproto::kCmd_glDrawArrays, samples.reader());
  CHECK(queries.open());
  queries.ran(av::glproto::kCmd_glEndQuery, samples.reader());
  CHECK(!queries.open());
  CHECK(queries.invariants_hold()); }

TEST(gl_occlusion_queries, each_occlusion_target_is_kept_apart_and_the_indexed_commands_count)
{ GlOcclusionQueries queries;
  const Payload any(any_samples_passed), conservative_target(conservative), samples(samples_passed);
  queries.ran(av::glproto::kCmd_glBeginQueryIndexed, any.reader());
  queries.ran(av::glproto::kCmd_glBeginQuery, conservative_target.reader());
  queries.ran(av::glproto::kCmd_glEndQueryIndexed, any.reader());
  CHECK(queries.open());
  queries.ran(av::glproto::kCmd_glEndQuery, samples.reader());
  CHECK(queries.open());
  queries.ran(av::glproto::kCmd_glEndQuery, conservative_target.reader());
  CHECK(!queries.open()); }

TEST(gl_occlusion_queries, other_query_targets_and_truncated_payloads_do_not_hold_anything_open)
{ GlOcclusionQueries queries;
  const Payload elapsed(time_elapsed);
  queries.ran(av::glproto::kCmd_glBeginQuery, elapsed.reader());
  CHECK(!queries.open());
  queries.ran(av::glproto::kCmd_glBeginQuery, av::WireReader(nullptr, 0u));
  CHECK(!queries.open());
  const Payload samples(samples_passed);
  queries.ran(av::glproto::kCmd_glBeginQuery, samples.reader());
  queries.clear();
  CHECK(!queries.open());
  CHECK(queries.invariants_hold()); }
