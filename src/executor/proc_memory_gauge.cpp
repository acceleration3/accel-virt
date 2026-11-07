#include "executor/proc_memory_gauge.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/context/mem_info_text.hpp"

#include <fcntl.h>
#include <malloc.h>
#include <time.h>
#include <unistd.h>

namespace av {

ProcMemoryGauge::ProcMemoryGauge() : fd_(-1)
{
    AV_SCOPE("ProcMemoryGauge::ProcMemoryGauge");
}

ProcMemoryGauge::~ProcMemoryGauge()
{
    AV_SCOPE("ProcMemoryGauge::~ProcMemoryGauge");
    if (fd_ >= 0) close(fd_);
}

uint64_t ProcMemoryGauge::now_ms()
{
    AV_SCOPE("ProcMemoryGauge::now_ms");
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC_COARSE, &now);
    return static_cast<uint64_t>(now.tv_sec) * 1000u + static_cast<uint64_t>(now.tv_nsec) / 1000000u;
}

bool ProcMemoryGauge::available(uint64_t &bytes)
{
    AV_SCOPE("ProcMemoryGauge::available");
    if (fd_ < 0) fd_ = open("/proc/meminfo", O_RDONLY | O_CLOEXEC);
    if (fd_ < 0) return false;
    char          text[4096];
    const ssize_t length = pread(fd_, text, sizeof text, 0);
    if (length <= 0) return false;
    const uint64_t found = MemInfoText(text, static_cast<size_t>(length)).available_bytes();
    if (found == 0u) return false;
    bytes = found;
    return true;
}

void ProcMemoryGauge::relieve()
{
    AV_SCOPE("ProcMemoryGauge::relieve");
    malloc_trim(0u);
}

}
