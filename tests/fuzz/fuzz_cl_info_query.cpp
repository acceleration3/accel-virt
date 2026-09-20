/* An info query as the application asks it (feature 052, T039).
 *
 * clGetPlatformInfo and clGetDeviceInfo take a size and a buffer, both from
 * the application, and a driver that trusts either writes outside what it was
 * given. The ICD answers four device queries and every platform query from its
 * own bytes - the adapter's, not the host's - so those are the ones where the
 * write is the ICD's own and this is what bounds it.
 *
 * An input is: a byte choosing the query, a byte of room, then nothing else
 * that matters. The destination is surrounded by canary, and what this proves
 * is that a short buffer is refused rather than half-filled, that a null
 * buffer still answers the size, and that the size answered is the size a
 * second call with exactly that much room writes. */
#include "icd/opencl/cl_device_report.hpp"
#include "icd/opencl/cl_platform.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

using av::clicd::ClDeviceReport;
using av::clicd::ClPlatform;

constexpr std::size_t  room_bound = 96u;
constexpr std::size_t  guard = 64u;
constexpr std::uint8_t canary = 0x5Au;

/* The queries the ICD answers itself. Everything else it forwards, and a
 * forwarded query writes nothing here. */
constexpr cl_platform_info platform_queries[] = {
    CL_PLATFORM_PROFILE, CL_PLATFORM_VERSION,    CL_PLATFORM_NAME,
    CL_PLATFORM_VENDOR,  CL_PLATFORM_EXTENSIONS, CL_PLATFORM_ICD_SUFFIX_KHR,
    CL_PLATFORM_HOST_TIMER_RESOLUTION, 0x9999u /* one it does not answer */};

void require(bool holds)
{
    if (!holds) __builtin_trap();
}

/* One query into a destination surrounded by canary: the size it answers, and
 * that nothing outside the room moved. `give_buffer` false is the size-only
 * call every OpenCL query is asked first, which is a different thing from a
 * buffer of no bytes. */
cl_int ask(ClPlatform &platform, cl_platform_info name, std::size_t room, bool give_buffer,
           std::size_t &answered)
{
    static std::uint8_t destination[guard + room_bound + guard];
    std::memset(destination, canary, sizeof destination);
    std::uint8_t *const into = destination + guard;
    answered = 0u;
    const cl_int result =
        platform.info(platform.as_handle(), name, room, give_buffer ? into : nullptr, &answered);
    for (std::size_t at = 0u; at < guard; at++) require(destination[at] == canary);
    for (std::size_t at = 0u; at < guard; at++)
        require(destination[guard + room_bound + at] == canary);
    if (result != CL_SUCCESS) {
        /* A refused query writes nothing at all, which is what lets an
         * application reuse a buffer across a failed call. */
        for (std::size_t at = 0u; at < room_bound; at++) require(into[at] == canary);
    } else {
        for (std::size_t at = room; at < room_bound; at++) require(into[at] == canary);
    }
    return result;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size < 2u) return 0;
    const cl_platform_info name =
        platform_queries[data[0] % (sizeof platform_queries / sizeof platform_queries[0])];
    const std::size_t room = data[1] % (room_bound + 1u);

    ClPlatform  platform;
    std::size_t needed = 0u;
    /* A null buffer still answers the size, which is how every OpenCL query
     * is asked the first time. */
    const cl_int counted = ask(platform, name, 0u, false, needed);
    if (counted != CL_SUCCESS) {
        /* A query this platform does not answer is refused whichever way it
         * is asked, and says nothing about a size. */
        std::size_t again = 0xFFFFu;
        require(ask(platform, name, room, true, again) == counted);
        return 0;
    }
    require(needed != 0u && needed <= room_bound);

    std::size_t  written = 0u;
    const cl_int result = ask(platform, name, room, true, written);
    if (room < needed) {
        /* Too little room is refused rather than truncated: OpenCL has no
         * partial answer, and a truncated string would be read as whole. */
        require(result == CL_INVALID_VALUE);
    } else {
        require(result == CL_SUCCESS && written == needed);
    }

    /* Exactly the size the size-only call named is always enough, and names
     * the same size again - the property every two-call query depends on. */
    std::size_t exact = 0u;
    require(ask(platform, name, needed, true, exact) == CL_SUCCESS && exact == needed);
    /* And the size-only call is the same whether or not a room is named
     * beside it: a null buffer is what makes it a size-only call. */
    std::size_t counted_again = 0u;
    require(ask(platform, name, room, false, counted_again) == CL_SUCCESS &&
            counted_again == needed);
    return 0;
}
