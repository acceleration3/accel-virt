#include "executor/d3d/d3d12_pipeline_compiles.hpp"

#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"

#include <unistd.h>

namespace av {

namespace {

void *run_pipeline_compiles(void *compiles)
{
    AV_SCOPE("run_pipeline_compiles");
    static_cast<D3d12PipelineCompiles *>(compiles)->run();
    return nullptr;
}

}

D3d12PipelineCompiles::D3d12PipelineCompiles(BlockSource &blocks)
    : lock_(), started_(lock_), compiled_(lock_), jobs_(), stages_(), orders_(), next_order_(0u), threads_(),
      thread_count_(0u), device_(nullptr), stopping_(false)
{
    AV_SCOPE("D3d12PipelineCompiles::D3d12PipelineCompiles");
    for (uint32_t index = 0u; index < job_count; index++) {
        jobs_[index].bind(blocks);
        stages_[index] = Stage::vacant;
    }
}

D3d12PipelineCompiles::~D3d12PipelineCompiles()
{
    AV_SCOPE("D3d12PipelineCompiles::~D3d12PipelineCompiles");
    stop();
}

void D3d12PipelineCompiles::bind(ID3D12Device *device)
{
    AV_SCOPE("D3d12PipelineCompiles::bind");
    ThreadLockHold held(lock_);
    device_ = device;
}

D3d12PipelineJob *D3d12PipelineCompiles::vacant()
{
    AV_SCOPE("D3d12PipelineCompiles::vacant");
    ThreadLockHold held(lock_);
    for (uint32_t index = 0u; index < job_count; index++)
        if (stages_[index] == Stage::vacant) return &jobs_[index];
    return nullptr;
}

void D3d12PipelineCompiles::start(D3d12PipelineJob &job)
{
    AV_SCOPE("D3d12PipelineCompiles::start");
    const auto index = static_cast<uint32_t>(&job - jobs_);
    start_threads();
    lock_.acquire();
    orders_[index] = next_order_++;
    if (thread_count_ != 0u) {
        stages_[index] = Stage::started;
        started_.signal();
        lock_.release();
        return;
    }
    stages_[index] = Stage::compiling;
    ID3D12Device *const device = device_;
    lock_.release();
    job.compile(device);
    ThreadLockHold held(lock_);
    stages_[index] = Stage::compiled;
}

D3d12PipelineJob *D3d12PipelineCompiles::pending(uint32_t id)
{
    AV_SCOPE("D3d12PipelineCompiles::pending");
    ThreadLockHold held(lock_);
    for (uint32_t index = 0u; index < job_count; index++)
        if (stages_[index] != Stage::vacant && jobs_[index].id() == id) return &jobs_[index];
    return nullptr;
}

D3d12PipelineJob *D3d12PipelineCompiles::oldest()
{
    AV_SCOPE("D3d12PipelineCompiles::oldest");
    ThreadLockHold held(lock_);
    uint32_t found = job_count;
    for (uint32_t index = 0u; index < job_count; index++)
        if (stages_[index] != Stage::vacant && (found == job_count || orders_[index] < orders_[found])) found = index;
    return found == job_count ? nullptr : &jobs_[found];
}

ID3D12PipelineState *D3d12PipelineCompiles::collect(D3d12PipelineJob &job)
{
    AV_SCOPE("D3d12PipelineCompiles::collect");
    const auto index = static_cast<uint32_t>(&job - jobs_);
    lock_.acquire();
    while (stages_[index] == Stage::compiling || (stages_[index] == Stage::started && thread_count_ != 0u))
        compiled_.wait();
    lock_.release();
    ID3D12PipelineState *const made = job.take();
    job.clear();
    ThreadLockHold held(lock_);
    stages_[index] = Stage::vacant;
    return made;
}

void D3d12PipelineCompiles::stop()
{
    AV_SCOPE("D3d12PipelineCompiles::stop");
    lock_.acquire();
    const uint32_t count = thread_count_;
    stopping_ = true;
    started_.broadcast();
    lock_.release();
    for (uint32_t index = 0u; index < count; index++) pthread_join(threads_[index], nullptr);
    ThreadLockHold held(lock_);
    thread_count_ = 0u;
    stopping_ = false;
}

void D3d12PipelineCompiles::start_threads()
{
    AV_SCOPE("D3d12PipelineCompiles::start_threads");
    ThreadLockHold held(lock_);
    if (thread_count_ != 0u) return;
    const long     online = sysconf(_SC_NPROCESSORS_ONLN);
    const uint32_t half = online > 1 ? static_cast<uint32_t>(online / 2) : 1u;
    const uint32_t wanted = half < most_threads ? half : most_threads;
    while (thread_count_ < wanted &&
           pthread_create(&threads_[thread_count_], nullptr, &run_pipeline_compiles, this) == 0)
        thread_count_++;
}

uint32_t D3d12PipelineCompiles::next_started() const
{
    AV_SCOPE("D3d12PipelineCompiles::next_started");
    uint32_t found = job_count;
    for (uint32_t index = 0u; index < job_count; index++)
        if (stages_[index] == Stage::started && (found == job_count || orders_[index] < orders_[found])) found = index;
    return found;
}

void D3d12PipelineCompiles::run()
{
    AV_SCOPE("D3d12PipelineCompiles::run");
    lock_.acquire();
    for (;;) {
        uint32_t index = next_started();
        while (!stopping_ && index == job_count) {
            started_.wait();
            index = next_started();
        }
        if (stopping_) break;
        stages_[index] = Stage::compiling;
        ID3D12Device *const device = device_;
        lock_.release();
        jobs_[index].compile(device);
        lock_.acquire();
        stages_[index] = Stage::compiled;
        compiled_.broadcast();
    }
    lock_.release();
}

bool D3d12PipelineCompiles::invariants_hold() const
{
    AV_SCOPE("D3d12PipelineCompiles::invariants_hold");
    if (thread_count_ > most_threads) return false;
    for (uint32_t index = 0u; index < job_count; index++)
        if ((stages_[index] == Stage::vacant) != (jobs_[index].id() == 0u)) return false;
    return true;
}

}
