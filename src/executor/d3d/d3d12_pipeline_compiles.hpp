#ifndef AV_D3D12_PIPELINE_COMPILES_HPP
#define AV_D3D12_PIPELINE_COMPILES_HPP

#include "device/shim/posix_condition.hpp"
#include "device/shim/posix_lock.hpp"
#include "executor/d3d/d3d12_pipeline_job.hpp"

#include <pthread.h>
#include <stdint.h>

struct ID3D12Device;
struct ID3D12PipelineState;

namespace av {

class BlockSource;
class TeardownBudget;

class D3d12PipelineCompiles {
public:
    static constexpr uint32_t job_count = 64u;
    static constexpr uint32_t most_threads = 8u;

    explicit D3d12PipelineCompiles(BlockSource &blocks);
    ~D3d12PipelineCompiles();
    D3d12PipelineCompiles(const D3d12PipelineCompiles &) = delete;
    D3d12PipelineCompiles &operator=(const D3d12PipelineCompiles &) = delete;

    void              bind(ID3D12Device *device);
    D3d12PipelineJob *vacant();
    void              start(D3d12PipelineJob &job);
    D3d12PipelineJob *pending(uint32_t id);
    D3d12PipelineJob *oldest();
    ID3D12PipelineState *collect(D3d12PipelineJob &job);
    [[nodiscard]] bool settle(TeardownBudget &budget);
    void              stop();
    void              run();

    bool invariants_hold() const;

private:
    enum class Stage : uint8_t { vacant, started, compiling, compiled };

    void     start_threads();
    uint32_t next_started() const;

    PosixLock         lock_;
    PosixCondition    started_;
    PosixCondition    compiled_;
    D3d12PipelineJob  jobs_[job_count];
    Stage             stages_[job_count];
    uint64_t          orders_[job_count];
    uint64_t          next_order_;
    pthread_t         threads_[most_threads];
    uint32_t          thread_count_;
    ID3D12Device     *device_;
    bool              stopping_;
};

}

#endif
