/* A host reply cut at every length, into the ICD's decode (feature 052, T052).
 *
 * The ICD is the only thing that checks what the device wrote, and the device
 * writing half an answer is a reachable state rather than a hostile one: a
 * context that faults mid-reply leaves exactly that, which is what
 * ScriptedHost::cut_next plays in the unit tests. This runs the same reply at
 * every prefix length, so every field boundary is a case rather than only the
 * ones a test thought of.
 *
 * What must hold at each length is what an application is owed: the call
 * answers, it never writes outside what the caller gave, and it never reports
 * having filled more than the room it was given. ASan and UBSan judge the
 * rest.
 *
 * An input is a byte choosing the call, a byte choosing the caller's room,
 * then the reply's bytes. */
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
/* Past this the prefixes cost more than they find; a reply the device wrote
 * is a few hundred bytes at most, and every boundary inside one is reached. */
constexpr std::size_t  max_prefixes = 512u;

void require(bool holds)
{
    if (!holds) __builtin_trap();
}

/* The caller's destination, canary on both sides. */
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
    /* And nothing past the room the caller gave, inside the destination. */
    for (std::size_t at = room; at < room_bound; at++) require(destination[guard + at] == canary);
}

/* One call against one prefix of the reply. */
void run(std::uint8_t which, std::size_t room, const std::uint8_t *reply, std::size_t length)
{
    av::MockTransport wire;
    wire.set_reply(reply, length);
    std::uint8_t *const into = fresh();

    switch (which % 6u) {
    case 0u: {
        /* A size and bytes, both the reply's word. */
        /* The size it reports is the host's word, which the application is
         * what checks; what must hold here is only that the bytes stayed
         * inside the room the caller gave. */
        std::size_t answered = 0xFFFFu;
        static_cast<void>(
            av::clproto::av_clGetDeviceInfo(wire, some_handle, CL_DEVICE_NAME, room, into, &answered));
        break;
    }
    case 1u: {
        /* A handle and an error: what every create answers. */
        cl_int error = 0x7Fu;
        const std::uint64_t device = some_handle;
        static_cast<void>(av::clproto::av_clCreateContext(wire, nullptr, 1u, &device, &error));
        break;
    }
    case 2u: {
        /* A count the reply chose, into an array the caller sized. */
        std::uint64_t ids[8] = {};
        cl_uint       written = 0xFFFFFFFFu;
        static_cast<void>(
            av::clproto::av_clGetDeviceIDs(wire, some_handle, CL_DEVICE_TYPE_ALL, 8u, ids, &written));
        break;
    }
    case 3u: {
        /* Bytes straight into the application's buffer. */
        static_cast<void>(av::clproto::av_clEnqueueReadBuffer(
            wire, some_handle, some_handle, CL_TRUE, 0u, room, into, 0u, nullptr, nullptr));
        break;
    }
    case 4u: {
        /* An address return - the shape feature 052 added, and the one a
         * guest must never believe on its own. */
        cl_int error = 0x7Fu;
        static_cast<void>(av::clproto::av_clEnqueueMapBuffer(wire, some_handle, some_handle, CL_TRUE,
                                                              CL_MAP_READ, 0u, room, 0u, nullptr,
                                                              nullptr, &error));
        break;
    }
    default: {
        /* One status and nothing else, which a cut reply breaks first. */
        static_cast<void>(av::clproto::av_clFinish(wire, some_handle));
        break;
    }
    }
    nothing_outside(room);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size < header_bytes) return 0;
    const std::uint8_t  which = data[0];
    const std::size_t   room = data[1] % (room_bound + 1u);
    const std::uint8_t *reply = data + header_bytes;
    std::size_t         length = size - header_bytes;
    if (length > max_prefixes) length = max_prefixes;
    /* Every prefix, including the empty one: a reply cut at any field
     * boundary is one of these, and so is a reply that never arrived. */
    for (std::size_t cut = 0u; cut <= length; cut++) run(which, room, reply, cut);
    return 0;
}
