#include "harness/executor/mock_host_apis.hpp"

namespace av {

MockHostApis::MockHostApis() : gpus_{}, count_(0u), questions_(0u), ledger_questions_(0u), log_(), lines_(log_, "shares: "), d3d11_(lines_), d3d11_index_(max_gpus), gl_(nullptr), heap_(), surfaces_(heap_),
      shares_(heap_, lines_), exporter_(), source_(exporter_, heap_), ledger_(), host_ram_(),
      memory_withheld_(false) {}

void MockHostApis::add_gpu(const uint8_t *uuid, uint32_t sub_apis)
{
    if (count_ >= max_gpus) return;
    Gpu &gpu = gpus_[count_];
    for (uint32_t i = 0u; i < uuid_bytes; i++) gpu.uuid[i] = uuid[i];
    gpu.sub_apis = sub_apis;
    count_++;
}

uint32_t MockHostApis::questions() const { return __atomic_load_n(&questions_, __ATOMIC_RELAXED); }

void MockHostApis::give_d3d11(uint32_t index) { d3d11_index_ = index; }

void MockHostApis::give_gl(const GlDisplay *display) { gl_ = display; }

D3d11HostDevice &MockHostApis::d3d11() { return d3d11_; }

MockMemoryExporter &MockHostApis::exporter() { return exporter_; }

HostMemorySource &MockHostApis::source() { return source_; }

void MockHostApis::withhold_memory_source() { memory_withheld_ = true; }

uint32_t MockHostApis::gpu_count()
{
    __atomic_fetch_add(&questions_, 1u, __ATOMIC_RELAXED);
    return count_;
}

void MockHostApis::gpu_uuid(uint32_t index, uint8_t *out)
{
    __atomic_fetch_add(&questions_, 1u, __ATOMIC_RELAXED);
    if (index >= count_ || out == nullptr) return;
    for (uint32_t i = 0u; i < uuid_bytes; i++) out[i] = gpus_[index].uuid[i];
}

int32_t MockHostApis::render_minor(uint32_t)
{
    __atomic_fetch_add(&questions_, 1u, __ATOMIC_RELAXED);
    return -1;
}

uint32_t MockHostApis::sub_apis(uint32_t index)
{
    __atomic_fetch_add(&questions_, 1u, __ATOMIC_RELAXED);
    return index < count_ ? gpus_[index].sub_apis : 0u;
}

const GlDisplay *MockHostApis::gl_display(uint32_t)
{
    __atomic_fetch_add(&questions_, 1u, __ATOMIC_RELAXED);
    return gl_;
}

const OpenClDevice *MockHostApis::opencl_device(uint32_t)
{
    __atomic_fetch_add(&questions_, 1u, __ATOMIC_RELAXED);
    return nullptr;
}

D3d11HostDevice *MockHostApis::d3d11_device(uint32_t index)
{
    __atomic_fetch_add(&questions_, 1u, __ATOMIC_RELAXED);
    return index == d3d11_index_ ? &d3d11_ : nullptr;
}

D3d9HostDevice *MockHostApis::d3d9_device(uint32_t)
{
    __atomic_fetch_add(&questions_, 1u, __ATOMIC_RELAXED);
    return nullptr;
}

D3d12HostDevice *MockHostApis::d3d12_device(uint32_t)
{
    __atomic_fetch_add(&questions_, 1u, __ATOMIC_RELAXED);
    return nullptr;
}

SharedSurfaces *MockHostApis::surfaces(uint32_t index)
{
    __atomic_fetch_add(&questions_, 1u, __ATOMIC_RELAXED);
    return index < count_ ? &surfaces_ : nullptr;
}

HostShares *MockHostApis::shares(uint32_t index)
{
    __atomic_fetch_add(&questions_, 1u, __ATOMIC_RELAXED);
    return index < count_ ? &shares_ : nullptr;
}

uint32_t MockHostApis::ledger_questions() const
{
    return __atomic_load_n(&ledger_questions_, __ATOMIC_RELAXED);
}

VramLedger *MockHostApis::vram_ledger(uint32_t index)
{
    __atomic_fetch_add(&ledger_questions_, 1u, __ATOMIC_RELAXED);
    __atomic_fetch_add(&questions_, 1u, __ATOMIC_RELAXED);
    return index < count_ ? &ledger_ : nullptr;
}

VramLedger &MockHostApis::ledger() { return ledger_; }

ClFreedMemory *MockHostApis::cl_freed(uint32_t index) { return index < count_ ? &freed_ : nullptr; }

ClFreedMemory &MockHostApis::freed() { return freed_; }

HostRamLedger *MockHostApis::host_ram_ledger() { return &host_ram_; }

HostRamLedger &MockHostApis::host_ram() { return host_ram_; }

HostMemorySource *MockHostApis::memory_source(uint32_t index)
{
    __atomic_fetch_add(&questions_, 1u, __ATOMIC_RELAXED);
    return index < count_ && !memory_withheld_ ? &source_ : nullptr;
}

}
