/* The stride kept on a command signature and what it bounds: a whole number
 * of commands from a word-aligned offset, inside the buffer. */
#include "device/executor/d3d12_indirect_stride.hpp"
#include "harness/executor/private_data_object.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3d12IndirectStride;
using av::PrivateDataObject;

TEST(d3d12_indirect_stride, stamped_on_a_signature_it_reads_back)
{ PrivateDataObject<ID3D12CommandSignature> signature;
  CHECK(D3d12IndirectStride(20u).stamp(&signature));
  D3d12IndirectStride stride;
  CHECK(stride.read(&signature));
  CHECK(stride.reaches(0u, 3u, 60u));
  CHECK(!stride.reaches(0u, 4u, 60u)); }

TEST(d3d12_indirect_stride, reaches_only_whole_commands_inside_the_buffer)
{ const D3d12IndirectStride stride(16u);
  CHECK(stride.reaches(16u, 2u, 48u));
  CHECK(!stride.reaches(20u, 2u, 48u));    /* the last command ends past the buffer */
  CHECK(!stride.reaches(18u, 1u, 48u));    /* not a whole word */
  CHECK(!stride.reaches(52u, 0u, 48u));    /* an offset past the buffer */
  CHECK(stride.reaches(48u, 0u, 48u));     /* no command at the very end */
  CHECK(!stride.reaches(0u, 0xFFFFFFFFu, 1u << 20u)); }

TEST(d3d12_indirect_stride, a_signature_that_keeps_none_gives_none_back)
{ PrivateDataObject<ID3D12CommandSignature> refusing;
  refusing.refuse();
  CHECK(!D3d12IndirectStride(16u).stamp(&refusing));
  D3d12IndirectStride stride;
  CHECK(!stride.read(&refusing));
  PrivateDataObject<ID3D12CommandSignature> other;
  CHECK(D3d12IndirectStride(16u).stamp(&other));
  other.resize(2u);
  CHECK(!stride.read(&other)); }
