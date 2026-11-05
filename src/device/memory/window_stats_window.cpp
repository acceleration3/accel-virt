#include "device/memory/window_stats_window.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "device/host/host.hpp"
#include "proto/pages/window_stats.hpp"

namespace av {

WindowStatsWindow::WindowStatsWindow(Host &host) : host_(host), page_(nullptr)
{
    AV_SCOPE("WindowStatsWindow::WindowStatsWindow");
}

WindowStatsWindow::~WindowStatsWindow()
{
    AV_SCOPE("WindowStatsWindow::~WindowStatsWindow");
    release();
}

Status WindowStatsWindow::map(uint64_t address)
{
    AV_SCOPE("WindowStatsWindow::map");
    host_.require_main_loop("WindowStatsWindow::map");
    release();
    uint64_t    length = WindowStats::page_size;
    bool        direct = false;
    void *const pointer = host_.map_guest(address, length, direct);
    if (pointer == nullptr || length == 0u) return Status::not_found;
    if (!direct || length < WindowStats::page_size) {
        host_.unmap_guest(pointer, length);
        return Status::not_supported;
    }
    page_ = static_cast<uint8_t *>(pointer);
    WindowStats header;
    if (read(header) != Status::ok) {
        release();
        return Status::malformed;
    }
    return Status::ok;
}

void WindowStatsWindow::release()
{
    AV_SCOPE("WindowStatsWindow::release");
    if (page_ == nullptr) return;
    host_.unmap_guest(page_, WindowStats::page_size);
    page_ = nullptr;
}

bool WindowStatsWindow::mapped() const
{
    AV_SCOPE("WindowStatsWindow::mapped");
    return page_ != nullptr;
}

Status WindowStatsWindow::read(WindowStats &stats) const
{
    AV_SCOPE("WindowStatsWindow::read");
    if (page_ == nullptr) return Status::invalid_state;
    const auto *const sequence = reinterpret_cast<const uint32_t *>(page_ + WindowStats::sequence_at);
    for (uint32_t attempt = 0u; attempt < read_attempts; attempt++) {
        const uint32_t before = __atomic_load_n(sequence, __ATOMIC_ACQUIRE);
        if ((before & 1u) != 0u) continue;
        uint8_t bytes[WindowStats::encoded_size] = {};
        for (uint64_t at = 0u; at < WindowStats::sequence_at; at += sizeof(uint32_t)) {
            const uint32_t word = __atomic_load_n(reinterpret_cast<const uint32_t *>(page_ + at), __ATOMIC_RELAXED);
            __builtin_memcpy(bytes + at, &word, sizeof word);
        }
        for (uint64_t at = WindowStats::fields_at; at < WindowStats::encoded_size; at += sizeof(uint64_t)) {
            const uint64_t word = __atomic_load_n(reinterpret_cast<const uint64_t *>(page_ + at), __ATOMIC_RELAXED);
            __builtin_memcpy(bytes + at, &word, sizeof word);
        }
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (__atomic_load_n(sequence, __ATOMIC_RELAXED) != before) continue;
        return stats.decode(bytes, sizeof bytes);
    }
    return Status::conflict;
}

void WindowStatsWindow::report(StateWriter &writer) const
{
    AV_SCOPE("WindowStatsWindow::report");
    writer.open("window_stats", 0u);
    WindowStats  stats;
    const Status found = read(stats);
    writer.flag("attached", mapped());
    writer.flag("stable", found == Status::ok);
    if (found == Status::ok) {
        writer.number("total_pages", stats.total_pages());
        writer.number("pages_in_use", stats.pages_in_use());
        writer.number("peak_pages_in_use", stats.peak_pages_in_use());
        writer.number("refusals", stats.refusals());
        writer.number("refused_bytes", stats.refused_bytes());
        writer.number("largest_free_run_at_refusal", stats.largest_free_run_at_refusal());
    }
    writer.close();
}

bool WindowStatsWindow::invariants_hold() const
{
    AV_SCOPE("WindowStatsWindow::invariants_hold");
    return page_ == nullptr || reinterpret_cast<uintptr_t>(page_) % alignof(uint64_t) == 0u;
}
}
