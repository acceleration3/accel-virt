#include "device/execution/gathered_command.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"

namespace av {

namespace {

/* The least a block is: a piece is most of a guest's room, so a smaller
 * block would only be grown at once. */
constexpr size_t least_block = size_t{1} << 20;

}  // namespace

GatheredCommand::GatheredCommand(Host &host) : host_(host), block_(nullptr), capacity_(0u), used_(0u)
{
    AV_SCOPE("GatheredCommand::GatheredCommand");
}

GatheredCommand::~GatheredCommand()
{
    AV_SCOPE("GatheredCommand::~GatheredCommand");
    clear();
}

Status GatheredCommand::gather(const uint8_t *body, size_t length)
{
    AV_SCOPE("GatheredCommand::gather");
    return append(body, length);
}

bool GatheredCommand::gathering() const
{
    AV_SCOPE("GatheredCommand::gathering");
    return used_ != 0u;
}

Status GatheredCommand::close(const uint8_t *body, size_t length, const uint8_t *&payload, size_t &size)
{
    AV_SCOPE("GatheredCommand::close");
    payload = nullptr;
    size = 0u;
    const Status appended = append(body, length);
    if (appended != Status::ok) return appended;
    payload = block_;
    size = used_;
    return Status::ok;
}

/* Doubled as it grows, so a command of n bytes is copied a logarithmic number
 * of times on its way in. */
Status GatheredCommand::append(const uint8_t *body, size_t length)
{
    AV_SCOPE("GatheredCommand::append");
    if (length == 0u) return Status::ok;
    if (length > ~size_t{0} - used_) {
        clear();
        return Status::no_memory;
    }
    if (used_ + length > capacity_) {
        size_t bytes = capacity_ > least_block ? capacity_ : least_block;
        while (bytes < used_ + length) bytes = bytes > ~size_t{0} / 2u ? used_ + length : bytes * 2u;
        auto *const grown = static_cast<uint8_t *>(host_.allocate(bytes));
        if (grown == nullptr) {
            clear();
            return Status::no_memory;
        }
        if (used_ != 0u) __builtin_memcpy(grown, block_, used_);
        if (block_ != nullptr) host_.release(block_);
        block_ = grown;
        capacity_ = bytes;
    }
    __builtin_memcpy(block_ + used_, body, length);
    used_ += length;
    return Status::ok;
}

void GatheredCommand::clear()
{
    AV_SCOPE("GatheredCommand::clear");
    if (block_ != nullptr) host_.release(block_);
    block_ = nullptr;
    capacity_ = 0u;
    used_ = 0u;
}

bool GatheredCommand::invariants_hold() const
{
    AV_SCOPE("GatheredCommand::invariants_hold");
    return used_ <= capacity_ && (block_ == nullptr) == (capacity_ == 0u);
}

}  // namespace av
