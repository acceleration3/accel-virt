#include "executor/opencl/staging_bytes.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/byte_ledger.hpp"

#include <sys/mman.h>

namespace av {

namespace {

constexpr size_t page_bytes = 4096u;

}

StagingBytes::StagingBytes() : bytes_(nullptr), held_(0u), account_()
{
    AV_SCOPE("StagingBytes::StagingBytes");
}

StagingBytes::~StagingBytes()
{
    AV_SCOPE("StagingBytes::~StagingBytes");
    release();
    static_cast<void>(account_.close());
}

void StagingBytes::bind(ByteLedger *ledger)
{
    AV_SCOPE("StagingBytes::bind");
    release();
    account_.bind(ledger);
}

void StagingBytes::release()
{
    AV_SCOPE("StagingBytes::release");
    if (bytes_ != nullptr) static_cast<void>(munmap(bytes_, held_));
    account_.give(held_);
    bytes_ = nullptr;
    held_ = 0u;
}

bool StagingBytes::reserve(size_t bytes)
{
    AV_SCOPE("StagingBytes::reserve");
    if (bytes <= held_) return true;
    if (bytes > SIZE_MAX - page_bytes) return false;
    const size_t whole = (bytes + page_bytes - 1u) / page_bytes * page_bytes;
    if (!account_.take(whole)) return false;
    void *const  mapped = mmap(nullptr, whole, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped == MAP_FAILED) {
        account_.give(whole);
        return false;
    }
    if (bytes_ != nullptr) static_cast<void>(munmap(bytes_, held_));
    account_.give(held_);
    bytes_ = static_cast<uint8_t *>(mapped);
    held_ = whole;
    return true;
}

uint8_t *StagingBytes::bytes() const
{
    AV_SCOPE("StagingBytes::bytes");
    return bytes_;
}

size_t StagingBytes::held() const
{
    AV_SCOPE("StagingBytes::held");
    return held_;
}

}
