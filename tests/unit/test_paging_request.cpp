#include "../../src/core/ddi/dxgkarg_build_paging_buffer_record.hpp"
#include "../../src/core/paging_request.hpp"
#include "../../src/core/status.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

#include <cstdint>

using av::DxgkargBuildPagingBufferRecord;
using av::Irql;
using av::MockPlatform;
using av::PagingRequest;
using av::Status;
using av::Violation;

TEST(paging_request_build, an_aperture_operation_needs_no_dma_bytes)
{ MockPlatform platform; std::uint8_t dma[16] = {};
  const std::int32_t operations[] = {2, 5, 6};   /* DISCARD_CONTENT, MAP_APERTURE_SEGMENT, UNMAP_APERTURE_SEGMENT */
  for (std::int32_t operation : operations) {
      DxgkargBuildPagingBufferRecord request = {};
      request.pDmaBuffer = dma;
      request.DmaSize = sizeof dma;
      request.Operation = operation;
      CHECK_STATUS(PagingRequest(platform, request).build(), Status::ok);
      CHECK(request.pDmaBuffer == dma);
  } }

TEST(paging_request_build, every_other_operation_is_refused)
{ MockPlatform platform;
  const std::int32_t operations[] = {0, 1, 3, 4, 7, 8, 9, 10, 11, 15, 17, 22, 23, -1};
  for (std::int32_t operation : operations) {
      DxgkargBuildPagingBufferRecord request = {};
      request.Operation = operation;
      CHECK_STATUS(PagingRequest(platform, request).build(), Status::invalid_parameter);
  } }

TEST(paging_request, is_passive_only)
{ MockPlatform platform; DxgkargBuildPagingBufferRecord request = {};
  request.Operation = 5;
  platform.set_irql(Irql::dispatch);
  static_cast<void>(PagingRequest(platform, request).build());
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }
