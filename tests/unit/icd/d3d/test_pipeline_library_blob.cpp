#include "harness/framework/test_registry.hpp"
#include "icd/d3d/hresult.hpp"
#include "icd/d3d/pipeline_library_blob.hpp"

#include <cstdint>
#include <cstring>

using av::d3d::PipelineLibraryBlob;

namespace {

constexpr av::d3d::Hresult mismatch = static_cast<av::d3d::Hresult>(0x887E0002u);

}

TEST(pipeline_library_blob, the_header_is_avpl_and_the_version_little_endian)
{ const PipelineLibraryBlob blob;
  CHECK_EQ(blob.size(), PipelineLibraryBlob::header_bytes);
  std::uint8_t written[PipelineLibraryBlob::header_bytes + 1u];
  std::memset(written, 0xEE, sizeof written);
  blob.write(written);
  const std::uint8_t expected[PipelineLibraryBlob::header_bytes] = {'A', 'V', 'P', 'L', 1u, 0u, 0u, 0u};
  CHECK(std::memcmp(written, expected, sizeof expected) == 0);
  CHECK_EQ(written[PipelineLibraryBlob::header_bytes], 0xEEu); }

TEST(pipeline_library_blob, a_new_library_and_its_own_blob_open)
{ const PipelineLibraryBlob blob;
  CHECK_EQ(blob.opened(nullptr, 0u), av::d3d::hr_ok);
  std::uint8_t written[PipelineLibraryBlob::header_bytes];
  blob.write(written);
  CHECK_EQ(blob.opened(written, sizeof written), av::d3d::hr_ok);
  const PipelineLibraryBlob other;
  CHECK_EQ(other.opened(written, sizeof written), av::d3d::hr_ok); }

TEST(pipeline_library_blob, any_other_blob_is_a_driver_version_mismatch)
{ const PipelineLibraryBlob blob;
  CHECK_EQ(av::d3d::hr_d3d12_driver_version_mismatch, mismatch);
  std::uint8_t written[PipelineLibraryBlob::header_bytes + 4u] = {};
  blob.write(written);
  CHECK_EQ(blob.opened(nullptr, PipelineLibraryBlob::header_bytes), mismatch);
  CHECK_EQ(blob.opened(written, PipelineLibraryBlob::header_bytes - 1u), mismatch);
  CHECK_EQ(blob.opened(written, sizeof written), mismatch);
  for (std::size_t byte = 0u; byte < PipelineLibraryBlob::header_bytes; byte++) {
      std::uint8_t changed[PipelineLibraryBlob::header_bytes];
      blob.write(changed);
      changed[byte] = static_cast<std::uint8_t>(changed[byte] + 1u);
      CHECK_EQ(blob.opened(changed, sizeof changed), mismatch);
  }
  const std::uint8_t zeroes[PipelineLibraryBlob::header_bytes] = {};
  CHECK_EQ(blob.opened(zeroes, sizeof zeroes), mismatch); }
