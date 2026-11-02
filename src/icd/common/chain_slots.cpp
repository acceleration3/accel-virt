#include "icd/common/chain_slots.hpp"

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/shared_object_call.hpp"
#include "proto/resources/shared_object_kind.hpp"

namespace av::icd {

ChainSlots::ChainSlots()
    : chains_(nullptr), window_(nullptr), width_(0u), height_(0u), count_(0u), ids_{}, bytes_{}, memories_{},
      opened_(false)
{
    AV_SCOPE("ChainSlots::ChainSlots");
}

bool ChainSlots::open(ClientTransport &wire, WindowChains &chains, void *window, uint64_t luid, uint32_t width,
                      uint32_t height, SlotLayout layout, uint32_t count, uint64_t into)
{
    AV_SCOPE("ChainSlots::open");
    release(wire);
    if (window_ != nullptr && (window_ != window || chains_ != &chains)) close();
    window_ = nullptr;
    chains_ = nullptr;
    if (window == nullptr || width == 0u || height == 0u || count == 0u || count > max_slots) return false;
    if (!chains.open(window, luid, width, height, layout, count, ids_, bytes_)) return false;
    chains_ = &chains;
    window_ = window;
    width_ = width;
    height_ = height;
    count_ = count;
    for (uint32_t slot = 0u; slot < count_; slot++) {
        SharedObjectCall call(wire);
        uint64_t         opened = 0u;
        if (call.open(ids_[slot], SharedObjectKind::memory, bytes_[slot], into, opened) == Status::ok && opened != 0u) {
            memories_[slot] = opened;
            continue;
        }
        release(wire);
        close();
        return false;
    }
    opened_ = true;
    return true;
}

bool ChainSlots::opened() const
{
    AV_SCOPE("ChainSlots::opened");
    return opened_;
}

bool ChainSlots::fits(void *window, uint32_t width, uint32_t height) const
{
    AV_SCOPE("ChainSlots::fits");
    return opened_ && window == window_ && width == width_ && height == height_;
}

uint32_t ChainSlots::count() const
{
    AV_SCOPE("ChainSlots::count");
    return count_;
}

uint32_t ChainSlots::width() const
{
    AV_SCOPE("ChainSlots::width");
    return width_;
}

uint32_t ChainSlots::height() const
{
    AV_SCOPE("ChainSlots::height");
    return height_;
}

uint64_t ChainSlots::memory(uint32_t slot) const
{
    AV_SCOPE("ChainSlots::memory");
    return slot < max_slots ? memories_[slot] : 0u;
}

bool ChainSlots::present(uint32_t slot)
{
    AV_SCOPE("ChainSlots::present");
    return opened_ && slot < count_ && chains_->present(window_, slot);
}

void ChainSlots::release(ClientTransport &wire)
{
    AV_SCOPE("ChainSlots::release");
    for (uint32_t slot = 0u; slot < max_slots; slot++) {
        if (memories_[slot] == 0u) continue;
        SharedObjectCall call(wire);
        static_cast<void>(call.release(ids_[slot], SharedObjectKind::memory, bytes_[slot], memories_[slot]));
    }
    forget_slots();
}

void ChainSlots::close()
{
    AV_SCOPE("ChainSlots::close");
    if (window_ != nullptr) chains_->close(window_);
    chains_ = nullptr;
    window_ = nullptr;
    width_ = 0u;
    height_ = 0u;
    forget_slots();
}

void ChainSlots::forget_slots()
{
    AV_SCOPE("ChainSlots::forget_slots");
    for (uint32_t slot = 0u; slot < max_slots; slot++) {
        ids_[slot] = 0u;
        bytes_[slot] = 0u;
        memories_[slot] = 0u;
    }
    count_ = 0u;
    opened_ = false;
}

bool ChainSlots::invariants_hold() const
{
    AV_SCOPE("ChainSlots::invariants_hold");
    if (count_ > max_slots) return false;
    for (uint32_t slot = 0u; slot < max_slots; slot++) {
        if (memories_[slot] != 0u && (ids_[slot] == 0u || bytes_[slot] == 0u || slot >= count_)) return false;
        if (opened_ && slot < count_ && memories_[slot] == 0u) return false;
    }
    if ((window_ == nullptr) != (chains_ == nullptr)) return false;
    return !opened_ || (window_ != nullptr && width_ != 0u && height_ != 0u && count_ != 0u);
}

}
