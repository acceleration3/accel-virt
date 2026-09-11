/* A hostile device driving one guest context: arbitrary answers to its
 * requests, arbitrary heads and fence pages written into its memory, and the
 * driver submitting and serving between them. Whatever the device does, the
 * context stays consistent, writes nothing outside its blocks (ASan), refuses
 * rather than asserts, and tears down.
 *
 * Input: operations of [u8 kind][u8 count][count bytes]: kind 0 answers the
 * next request with the bytes, 1 writes the ring's head, 2 writes the fence
 * page, 3 submits the next batch of `count` bytes, 4 serves, 5 answers OK. */
#include "core/platform/platform.hpp"
#include "core/rvk/guest_context.hpp"
#include "core/support/status.hpp"
#include "harness/device/fake_doorbell_pages.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"
#include "proto/virtio_gpu/rvk_request_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::uint8_t uuid[av::GpuIdentity::uuid_bytes] = {};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::MockPlatform      platform;
    av::FakeDoorbellPages doorbells;
    av::GuestContext      context(platform, doorbells);
    const av::CapsetInfo  capset(av::ProtocolVersion(1u, 0u), 1u, 64u, 12u, 24u, 0u,
                                 av::GpuIdentity(0u, 0u, 0u, uuid, 0u, 0u, "gpu"));
    if (context.set_up(capset, 1u, av::ContextSetup(av::ProtocolVersion(1u, 0u), av::SubApi::vulkan, 2u, 12u, 3u,
                                                    0u, 4u)) != av::Status::ok)
        __builtin_trap();
    std::uint8_t  request[av::RvkRequestWriter::max_request_size] = {};
    std::size_t   length = 0u;
    std::uint64_t fence = 1u;
    platform.set_irql(av::Irql::dispatch);
    for (std::size_t at = 0u; at + 2u <= size;) {
        const std::uint8_t kind = data[at] % 6u;
        const std::size_t  count = data[at + 1u] < size - at - 2u ? data[at + 1u] : size - at - 2u;
        const std::uint8_t *bytes = data + at + 2u;
        at += 2u + count;
        switch (kind) {
        case 0:
            if (context.next_request(request, sizeof request, length) == av::Status::ok)
                static_cast<void>(context.accept(bytes, count));
            break;
        case 1:
            if (context.ring() != nullptr) std::memcpy(context.ring() + 16u, bytes, count < 8u ? count : 8u);
            break;
        case 2:
            if (context.fence_page() != nullptr)
                std::memcpy(const_cast<std::uint8_t *>(context.fence_page()), bytes, count < 32u ? count : 32u);
            break;
        case 3:
            static_cast<void>(context.submit(av::SubmissionDescriptor(fence++, context.tail(), context.tail() + count + 1u)));
            break;
        case 4: static_cast<void>(context.serve()); break;
        default:
            if (context.next_request(request, sizeof request, length) == av::Status::ok) {
                av::GpuResponse ok;
                ok.set(av::GpuResponse::ok_nodata);
                static_cast<void>(context.accept(ok.bytes(), ok.length()));
            }
            break;
        }
        if (!context.invariants_hold()) __builtin_trap();
    }
    if (context.tear_down() == av::Status::ok) {
        av::GpuResponse ok;
        ok.set(av::GpuResponse::ok_nodata);
        while (context.next_request(request, sizeof request, length) == av::Status::ok)
            static_cast<void>(context.accept(ok.bytes(), ok.length()));
    }
    context.abandon();
    platform.set_irql(av::Irql::passive);
    if (context.release() != av::Status::ok) __builtin_trap();
    return 0;
}
