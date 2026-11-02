#include "harness/icd/scripted_window_chains.hpp"

namespace av {

namespace {

constexpr std::uint64_t page = 4096u;

}

ScriptedWindowChains::ScriptedWindowChains()
    : refuse_opens_(true), refuse_presents_(false), opens_(0u), presents_(0u), closes_(0u), window_(nullptr),
      luid_(0u), width_(0u), height_(0u), layout_(icd::SlotLayout::rgba_bottom_up), slots_(0u), slot_(0u), closed_(nullptr)
{
}

void ScriptedWindowChains::refuse_opens(bool refuse) { refuse_opens_ = refuse; }
void ScriptedWindowChains::refuse_presents(bool refuse) { refuse_presents_ = refuse; }

bool ScriptedWindowChains::open(void *window, std::uint64_t luid, std::uint32_t width, std::uint32_t height,
                                icd::SlotLayout layout, std::uint32_t slots, std::uint64_t *ids, std::uint64_t *bytes)
{
    opens_++;
    if (refuse_opens_ || slots > max_slots) return false;
    window_ = window;
    luid_ = luid;
    width_ = width;
    height_ = height;
    layout_ = layout;
    slots_ = slots;
    for (std::uint32_t slot = 0u; slot < slots; slot++) {
        ids[slot] = first_id + slot;
        bytes[slot] = (static_cast<std::uint64_t>(width) * height * 4u + page - 1u) / page * page;
    }
    return true;
}

bool ScriptedWindowChains::present(void *window, std::uint32_t slot)
{
    if (refuse_presents_ || window != window_ || slot >= slots_) return false;
    presents_++;
    slot_ = slot;
    return true;
}

void ScriptedWindowChains::close(void *window)
{
    closes_++;
    closed_ = window;
    if (window == window_) window_ = nullptr;
}

std::uint32_t ScriptedWindowChains::opens() const { return opens_; }
std::uint32_t ScriptedWindowChains::presents() const { return presents_; }
std::uint32_t ScriptedWindowChains::closes() const { return closes_; }
void         *ScriptedWindowChains::open_window() const { return window_; }
std::uint64_t ScriptedWindowChains::last_luid() const { return luid_; }
std::uint32_t ScriptedWindowChains::last_width() const { return width_; }
std::uint32_t ScriptedWindowChains::last_height() const { return height_; }
icd::SlotLayout ScriptedWindowChains::last_layout() const { return layout_; }
std::uint32_t ScriptedWindowChains::last_slots() const { return slots_; }
std::uint32_t ScriptedWindowChains::last_slot() const { return slot_; }
void         *ScriptedWindowChains::last_closed() const { return closed_; }

}
