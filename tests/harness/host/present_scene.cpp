#include "harness/host/present_scene.hpp"

#include "harness/host/mock_host.hpp"
#include "proto/pages/present_progress.hpp"
#include "proto/virtio_gpu/queued_present_request.hpp"

#include <cstring>

namespace av {

namespace {

constexpr std::uint64_t source_first = PresentScene::ram + 5u * PresentScene::page_bytes;
constexpr std::uint64_t source_second = PresentScene::ram + 1u * PresentScene::page_bytes;
constexpr std::uint64_t destination_run = PresentScene::ram + 8u * PresentScene::page_bytes;
constexpr std::uint32_t blt_operation = 1u;
constexpr std::uint32_t fill_operation = 2u;

}  // namespace

PresentScene::PresentScene(MockHost &host) : host_(host)
{
    host_.add_guest_ram(ram, ram_bytes);
    for (std::uint32_t y = 0u; y < height; y++)
        for (std::uint32_t x = 0u; x < width; x++) {
            const std::uint32_t value = 0x80000000u | (y << 8u) | x;
            std::memcpy(source(x, y), &value, sizeof value);
        }
}

std::uint8_t *PresentScene::source(std::uint32_t x, std::uint32_t y) const
{
    const std::uint64_t at = std::uint64_t{y} * pitch + x * 4u;
    return host_.guest_ram((at < page_bytes ? source_first : source_second - page_bytes) + at);
}

std::uint8_t *PresentScene::destination(std::uint32_t x, std::uint32_t y) const
{
    return host_.guest_ram(destination_run + std::uint64_t{y} * pitch + x * 4u);
}

std::uint32_t PresentScene::source_pixel(std::uint32_t x, std::uint32_t y) const
{
    std::uint32_t value = 0u;
    std::memcpy(&value, source(x, y), sizeof value);
    return value;
}

std::uint32_t PresentScene::destination_pixel(std::uint32_t x, std::uint32_t y) const
{
    std::uint32_t value = 0u;
    std::memcpy(&value, destination(x, y), sizeof value);
    return value;
}

bool PresentScene::copied(std::uint32_t first, std::uint32_t rows) const
{
    for (std::uint32_t y = first; y < first + rows; y++)
        for (std::uint32_t x = 0u; x < width; x++)
            if (destination_pixel(x, y) != source_pixel(x, y)) return false;
    return true;
}

GpuRequestBuilder PresentScene::blt(std::uint64_t ticket, std::uint32_t first, std::uint32_t rows) const
{
    GpuRequestBuilder builder(QueuedPresentRequest::type);
    builder.u64(ticket).u32(0u).u32(0u).u64(0u);
    builder.u32(blt_operation).u32(0u).u32(0u).u32(1u).u32(2u).u32(1u).u32(pitch).u32(pitch);
    builder.u64(source_first).u32(page_bytes).u32(0u).u64(source_second).u32(page_bytes).u32(0u);
    builder.u64(destination_run).u32(2u * page_bytes).u32(0u);
    builder.u32(0u).u32(first).u32(0u).u32(first).u32(width).u32(rows);
    return builder;
}

GpuRequestBuilder PresentScene::fill(std::uint64_t ticket, std::uint32_t colour, std::uint32_t first,
                                     std::uint32_t rows) const
{
    GpuRequestBuilder builder(QueuedPresentRequest::type);
    builder.u64(ticket).u32(0u).u32(0u).u64(0u);
    builder.u32(fill_operation).u32(0u).u32(colour).u32(1u).u32(0u).u32(1u).u32(0u).u32(pitch);
    builder.u64(destination_run).u32(2u * page_bytes).u32(0u);
    builder.u32(0u).u32(0u).u32(0u).u32(first).u32(width).u32(rows);
    return builder;
}

GpuRequestBuilder PresentScene::image(std::uint64_t ticket, std::uint64_t key) const
{
    GpuRequestBuilder builder(QueuedPresentRequest::type);
    builder.u64(ticket).u32(1u).u32(1u).u64(key);   /* the surface's first present */
    builder.u32(blt_operation).u32(0u).u32(0u).u32(1u).u32(0u).u32(1u).u32(0u).u32(pitch);
    builder.u64(destination_run).u32(2u * page_bytes).u32(0u);
    builder.u32(0u).u32(0u).u32(0u).u32(0u).u32(width).u32(1u);
    return builder;
}

bool PresentScene::progress(PresentProgress &progress) const
{
    return progress.decode(host_.guest_ram(engine_page), PresentProgress::encoded_size) == Status::ok;
}

}  // namespace av
