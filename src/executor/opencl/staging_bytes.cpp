#include "executor/opencl/staging_bytes.hpp"

#include "core/trace/function_scope.hpp"

#include <sys/mman.h>

namespace av {

namespace {

constexpr size_t page_bytes = 4096u;

}

StagingBytes::StagingBytes() : bytes_(nullptr), held_(0u)
{
    AV_SCOPE("StagingBytes::StagingBytes");
}

StagingBytes::~StagingBytes()
{
    AV_SCOPE("StagingBytes::~StagingBytes");
    if (bytes_ != nullptr) static_cast<void>(munmap(bytes_, held_));
}

bool StagingBytes::reserve(size_t bytes)
{
    AV_SCOPE("StagingBytes::reserve");
    if (bytes <= held_) return true;
    if (bytes > SIZE_MAX - page_bytes) return false;
    const size_t whole = (bytes + page_bytes - 1u) / page_bytes * page_bytes;
    void *const  mapped = mmap(nullptr, whole, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped == MAP_FAILED) return false;
    if (bytes_ != nullptr) static_cast<void>(munmap(bytes_, held_));
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
