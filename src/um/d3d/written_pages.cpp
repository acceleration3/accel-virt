#include "um/d3d/written_pages.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av::d3d11 {

WrittenPages::WrittenPages() : pages_{}
{
    AV_SCOPE("WrittenPages::WrittenPages");
}

uint8_t *WrittenPages::allocate(size_t bytes)
{
    AV_SCOPE("WrittenPages::allocate");
    return static_cast<uint8_t *>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH,
                                               PAGE_READWRITE));
}

void WrittenPages::release(uint8_t *block)
{
    AV_SCOPE("WrittenPages::release");
    static_cast<void>(VirtualFree(block, 0u, MEM_RELEASE));
}

void WrittenPages::written(uint8_t *at, size_t bytes, size_t &first, size_t &end)
{
    AV_SCOPE("WrittenPages::written");
    first = 0u;
    end = 0u;
    if (bytes == 0u) return;
    const uintptr_t low = reinterpret_cast<uintptr_t>(at) & ~(page_bytes - 1u);
    const uintptr_t high = (reinterpret_cast<uintptr_t>(at) + bytes + page_bytes - 1u) & ~(page_bytes - 1u);
    ULONG_PTR count = max_pages;
    ULONG     granularity = 0u;
    if (GetWriteWatch(WRITE_WATCH_FLAG_RESET, reinterpret_cast<void *>(low), high - low, pages_, &count,
                      &granularity) != 0u ||
        count == max_pages) {
        static_cast<void>(ResetWriteWatch(reinterpret_cast<void *>(low), high - low));
        end = bytes;
        return;
    }
    if (count == 0u) return;
    uintptr_t lowest = high, highest = low;
    for (ULONG_PTR index = 0u; index < count; index++) {
        const uintptr_t page = reinterpret_cast<uintptr_t>(pages_[index]);
        if (page < lowest) lowest = page;
        if (page + granularity > highest) highest = page + granularity;
    }
    const uintptr_t start = reinterpret_cast<uintptr_t>(at);
    first = lowest > start ? lowest - start : 0u;
    end = highest - start < bytes ? highest - start : bytes;
}

}  // namespace av::d3d11
