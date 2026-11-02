#include "harness/transport/mock_transport.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::size_t  header_bytes = 2u;
constexpr std::size_t  room_bound = 64u;
constexpr std::size_t  guard = 32u;
constexpr std::uint8_t canary = 0x5Au;
constexpr std::uint64_t some_handle = 0x6400u;
constexpr std::size_t  max_prefixes = 512u;

void require(bool holds)
{
    if (!holds) __builtin_trap();
}

std::uint8_t destination[guard + room_bound + guard];

std::uint8_t *fresh()
{
    std::memset(destination, canary, sizeof destination);
    return destination + guard;
}

void nothing_outside(std::size_t room)
{
    for (std::size_t at = 0u; at < guard; at++) require(destination[at] == canary);
    for (std::size_t at = 0u; at < guard; at++)
        require(destination[guard + room_bound + at] == canary);
    for (std::size_t at = room; at < room_bound; at++) require(destination[guard + at] == canary);
}

void run(std::uint8_t which, std::size_t room, const std::uint8_t *reply, std::size_t length)
{
    av::MockTransport wire;
    wire.set_reply(reply, length);
    std::uint8_t *const into = fresh();

    switch (which % 6u) {
    case 0u: {
        std::size_t answered = 0xFFFFu;
        static_cast<void>(
            av::clproto::av_clGetDeviceInfo(wire, some_handle, CL_DEVICE_NAME, room, into, &answered));
        break;
    }
    case 1u: {
        cl_int error = 0x7Fu;
        const std::uint64_t device = some_handle;
        static_cast<void>(av::clproto::av_clCreateContext(wire, nullptr, 1u, &device, &error));
        break;
    }
    case 2u: {
        std::uint64_t ids[8] = {};
        cl_uint       written = 0xFFFFFFFFu;
        static_cast<void>(
            av::clproto::av_clGetDeviceIDs(wire, some_handle, CL_DEVICE_TYPE_ALL, 8u, ids, &written));
        break;
    }
    case 3u: {
        static_cast<void>(av::clproto::av_clEnqueueReadBuffer(
            wire, some_handle, some_handle, CL_TRUE, 0u, room, into, 0u, nullptr, nullptr));
        break;
    }
    case 4u: {
        cl_int error = 0x7Fu;
        static_cast<void>(av::clproto::av_clEnqueueMapBuffer(wire, some_handle, some_handle, CL_TRUE,
                                                              CL_MAP_READ, 0u, room, 0u, nullptr,
                                                              nullptr, &error));
        break;
    }
    default: {
        static_cast<void>(av::clproto::av_clFinish(wire, some_handle));
        break;
    }
    }
    nothing_outside(room);
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size < header_bytes) return 0;
    const std::uint8_t  which = data[0];
    const std::size_t   room = data[1] % (room_bound + 1u);
    const std::uint8_t *reply = data + header_bytes;
    std::size_t         length = size - header_bytes;
    if (length > max_prefixes) length = max_prefixes;
    for (std::size_t cut = 0u; cut <= length; cut++) run(which, room, reply, cut);
    return 0;
}
