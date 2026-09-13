/* Reading the batch a guest left in a DMA buffer. The buffer is the guest's, so
 * it is copied before it is trusted, and every field is judged: a short buffer,
 * bytes no descriptor could be, and a batch longer than the ring are each
 * refused rather than acted on (feature 043). */
#include "core/rvk/submission_reader.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/submission_descriptor.hpp"

#include <cstdint>

using av::MockPlatform;
using av::RingHeader;
using av::Status;
using av::SubmissionDescriptor;
using av::SubmissionReader;

namespace {

/* A ring of order 12 = 4096 data bytes, empty. */
RingHeader ring() { return RingHeader(12u, 0u, 0u); }

}  // namespace

TEST(submission_reader, reads_a_valid_batch)
{ MockPlatform platform;
  SubmissionReader reader(platform);
  std::uint8_t dma[SubmissionDescriptor::encoded_size] = {};
  CHECK_STATUS(SubmissionDescriptor(1u, 0u, 64u).encode(dma, sizeof dma), Status::ok);

  SubmissionDescriptor got;
  CHECK_STATUS(reader.read(dma, sizeof dma, ring(), got), Status::ok);
  CHECK_EQ(got.fence_id(), 1u);
  CHECK_EQ(got.begin(), 0u);
  CHECK_EQ(got.end(), 64u); }

TEST(submission_reader, a_short_buffer_is_refused)
{ MockPlatform platform;
  SubmissionReader reader(platform);
  std::uint8_t dma[SubmissionDescriptor::encoded_size] = {};
  CHECK_STATUS(SubmissionDescriptor(1u, 0u, 64u).encode(dma, sizeof dma), Status::ok);
  SubmissionDescriptor got;
  for (std::size_t size = 0u; size < SubmissionDescriptor::encoded_size; size++)
      CHECK_STATUS(reader.read(dma, size, ring(), got), Status::malformed);
  CHECK_STATUS(reader.read(nullptr, sizeof dma, ring(), got), Status::malformed); }

TEST(submission_reader, bytes_no_descriptor_could_be_are_refused)
{ MockPlatform platform;
  SubmissionReader reader(platform);
  std::uint8_t dma[SubmissionDescriptor::encoded_size] = {};
  CHECK_STATUS(SubmissionDescriptor(1u, 0u, 64u).encode(dma, sizeof dma), Status::ok);
  dma[0] ^= 0xFFu;  /* the magic */
  SubmissionDescriptor got;
  CHECK_STATUS(reader.read(dma, sizeof dma, ring(), got), Status::malformed); }

/* A batch longer than the ring cannot have been written into it. */
TEST(submission_reader, a_batch_larger_than_the_ring_is_refused)
{ MockPlatform platform;
  SubmissionReader reader(platform);
  std::uint8_t dma[SubmissionDescriptor::encoded_size] = {};
  CHECK_STATUS(SubmissionDescriptor(1u, 0u, 8192u).encode(dma, sizeof dma), Status::ok);
  SubmissionDescriptor got;
  CHECK_STATUS(reader.read(dma, sizeof dma, ring(), got), Status::invalid_state); }
