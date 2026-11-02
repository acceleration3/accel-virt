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

constexpr cl_platform_info platform_queries[] = {
    CL_PLATFORM_PROFILE, CL_PLATFORM_VERSION,    CL_PLATFORM_NAME,
    CL_PLATFORM_VENDOR,  CL_PLATFORM_EXTENSIONS, CL_PLATFORM_ICD_SUFFIX_KHR,
    CL_PLATFORM_HOST_TIMER_RESOLUTION, 0x9999u };

void require(bool holds)
{
    if (!holds) __builtin_trap();
}

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
        for (std::size_t at = 0u; at < room_bound; at++) require(into[at] == canary);
    } else {
        for (std::size_t at = room; at < room_bound; at++) require(into[at] == canary);
    }
    return result;
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size < 2u) return 0;
    const cl_platform_info name =
        platform_queries[data[0] % (sizeof platform_queries / sizeof platform_queries[0])];
    const std::size_t room = data[1] % (room_bound + 1u);

    ClPlatform  platform;
    std::size_t needed = 0u;
    const cl_int counted = ask(platform, name, 0u, false, needed);
    if (counted != CL_SUCCESS) {
        std::size_t again = 0xFFFFu;
        require(ask(platform, name, room, true, again) == counted);
        return 0;
    }
    require(needed != 0u && needed <= room_bound);

    std::size_t  written = 0u;
    const cl_int result = ask(platform, name, room, true, written);
    if (room < needed) {
        require(result == CL_INVALID_VALUE);
    } else {
        require(result == CL_SUCCESS && written == needed);
    }

    std::size_t exact = 0u;
    require(ask(platform, name, needed, true, exact) == CL_SUCCESS && exact == needed);
    std::size_t counted_again = 0u;
    require(ask(platform, name, room, false, counted_again) == CL_SUCCESS &&
            counted_again == needed);
    return 0;
}
