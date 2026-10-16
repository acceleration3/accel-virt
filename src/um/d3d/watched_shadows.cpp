#include "um/d3d/watched_shadows.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/written_runs.hpp"

#include <windows.h>

namespace av::d3d11 {

WatchedShadows::WatchedShadows(BlockSource &heap) : heap_(heap)
{
    AV_SCOPE("WatchedShadows::WatchedShadows");
}

uint8_t *WatchedShadows::allocate(size_t bytes, bool &watched)
{
    AV_SCOPE("WatchedShadows::allocate");
    watched = false;
    if (bytes >= watch_threshold) {
        void *region = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);
        if (region != nullptr) {
            watched = true;
            return static_cast<uint8_t *>(region);
        }
    }
    return static_cast<uint8_t *>(heap_.allocate(bytes, Pool::paged));
}

void WatchedShadows::release(uint8_t *shadow, bool watched)
{
    AV_SCOPE("WatchedShadows::release");
    if (watched) VirtualFree(shadow, 0u, MEM_RELEASE);
    else heap_.release(shadow);
}

bool WatchedShadows::take_written(uint8_t *start, size_t bytes, icd::WrittenRuns &runs)
{
    AV_SCOPE("WatchedShadows::take_written");
    runs.clear();
    const uintptr_t begin = reinterpret_cast<uintptr_t>(start);
    const uintptr_t end = begin + bytes;
    void     *pages[max_pages];
    ULONG_PTR found = max_pages;
    ULONG     page = 0u;
    if (GetWriteWatch(WRITE_WATCH_FLAG_RESET, start, bytes, pages, &found, &page) != 0u || page == 0u) return false;
    if (found == max_pages) {
        ResetWriteWatch(start, bytes);
        return false;
    }
    for (ULONG_PTR index = 0u; index < found; index++) {
        const uintptr_t first = reinterpret_cast<uintptr_t>(pages[index]);
        const uintptr_t from = first > begin ? first : begin;
        const uintptr_t to = first + page < end ? first + page : end;
        if (to <= from) continue;
        if (!runs.note(from - begin, to - from)) return false;
    }
    return true;
}

void WatchedShadows::forget_writes(uint8_t *start, size_t bytes)
{
    AV_SCOPE("WatchedShadows::forget_writes");
    ResetWriteWatch(start, bytes);
}

}  // namespace av::d3d11
