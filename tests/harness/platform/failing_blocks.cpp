#include "harness/platform/failing_blocks.hpp"

#include <cstdlib>

namespace av {

FailingBlocks::FailingBlocks() : fail_from_(0u), requests_(0u), live_(0u), refused_(0u) {}

void *FailingBlocks::allocate(size_t bytes, Pool)
{
    requests_++;
    if (fail_from_ != 0u && requests_ >= fail_from_) {
        refused_++;
        return nullptr;
    }
    void *const block = std::malloc(bytes);
    if (block != nullptr) live_++;
    return block;
}

void FailingBlocks::release(void *block)
{
    if (block == nullptr) return;
    live_--;
    std::free(block);
}

void FailingBlocks::require_irql_at_most(Irql, const char *) {}

void FailingBlocks::fail_from(std::uint32_t nth) { fail_from_ = nth; }

void FailingBlocks::heal() { fail_from_ = 0u; }

std::uint32_t FailingBlocks::requests() const { return requests_; }

std::uint32_t FailingBlocks::live() const { return live_; }

std::uint32_t FailingBlocks::refused() const { return refused_; }

}
