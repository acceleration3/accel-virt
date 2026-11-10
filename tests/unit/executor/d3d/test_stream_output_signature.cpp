#include "executor/d3d/stream_output_signature.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

namespace {

using av::d3dproto::SignatureEntry;
using av::d3dproto::StreamOutEntry;
using Signature = av::StreamOutputSignature;

constexpr std::uint32_t float_type = 3u;

}

TEST(stream_output_signature, outputs_the_shader_names_are_kept_as_they_are)
{ const SignatureEntry outputs[2] = {{1u, 0u, 0xFu, float_type, 0u, 0u}, {0u, 1u, 0x7u, float_type, 0u, 0u}};
  const StreamOutEntry streamed[2] = {{0u, 0u, 0u, 0xFu}, {0u, 0u, 1u, 0x7u}};
  const Signature      signature(outputs, 2u, streamed, 2u);
  CHECK_EQ(signature.count(), 2u);
  CHECK_EQ(signature.entries()[0].system_value, 1u);
  CHECK_EQ(signature.entries()[1].mask, 0x7u); }

TEST(stream_output_signature, a_streamed_register_the_runtime_listed_no_output_for_is_added)
{ const StreamOutEntry streamed[3] = {{0u, 0u, 0u, 0xFu}, {0u, 0u, 1u, 0x7u}, {0u, 0u, 0xFFFFFFFFu, 0x1u}};
  const Signature      signature(nullptr, 0u, streamed, 3u);
  CHECK_EQ(signature.count(), 2u);
  CHECK_EQ(signature.entries()[0].register_index, 0u);
  CHECK_EQ(signature.entries()[0].mask, 0xFu);
  CHECK_EQ(signature.entries()[0].component_type, float_type);
  CHECK_EQ(signature.entries()[1].register_index, 1u);
  CHECK_EQ(signature.entries()[1].mask, 0x7u); }

TEST(stream_output_signature, the_same_register_on_another_stream_is_its_own_output)
{ const SignatureEntry outputs[1] = {{0u, 0u, 0xFu, float_type, 0u, 2u}};
  const StreamOutEntry streamed[2] = {{0u, 0u, 0u, 0xFu}, {2u, 2u, 0u, 0xFu}};
  const Signature      signature(outputs, 1u, streamed, 2u);
  CHECK_EQ(signature.count(), 2u);
  CHECK_EQ(signature.entries()[0].stream, 2u);
  CHECK_EQ(signature.entries()[1].stream, 0u); }

TEST(stream_output_signature, components_streamed_beyond_the_listed_mask_widen_it_once)
{ const SignatureEntry outputs[1] = {{0u, 3u, 0x3u, float_type, 0u, 0u}};
  const StreamOutEntry streamed[2] = {{0u, 0u, 3u, 0xCu}, {0u, 1u, 3u, 0x3u}};
  const Signature      signature(outputs, 1u, streamed, 2u);
  CHECK_EQ(signature.count(), 1u);
  CHECK_EQ(signature.entries()[0].mask, 0xFu); }
