/* A Direct3D 12 session's pipelines being compiled, on threads of their own.
 *
 * The session describes each pipeline into a vacant job (D3d12PipelineJob)
 * and starts it; threads started with the first take started jobs oldest
 * first and compile them, several at once, while the session runs on. The
 * session collects a job - the pipeline into its object table - when a
 * command names the pipeline, when it needs the slot for another, and when it
 * finishes or ends. Jobs are few (job_count) so a guest that makes thousands
 * holds a bounded amount of memory: the next pipeline waits for the oldest.
 *
 * Every call but run() is the session's thread's. */
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

class D3d12PipelineCompiles {
public:
    /* Pipelines described and not yet collected: enough to keep every thread
     * busy while the session reads ahead, few enough that their code - a
     * stage is up to a few hundred KiB - stays small. */
    static constexpr uint32_t job_count = 64u;
    /* The most threads compiling: each is a driver's compiler, which on some
     * hosts takes hundreds of MiB of its own. */
    static constexpr uint32_t most_threads = 8u;

    /* `blocks` is where the jobs' descriptions are written. */
    explicit D3d12PipelineCompiles(BlockSource &blocks);
    ~D3d12PipelineCompiles();
    D3d12PipelineCompiles(const D3d12PipelineCompiles &) = delete;
    D3d12PipelineCompiles &operator=(const D3d12PipelineCompiles &) = delete;

    /* The device pipelines are compiled on; null before a session opens one. */
    void              bind(ID3D12Device *device);
    /* A job to describe the next pipeline in; null while every job holds one
     * not collected. */
    D3d12PipelineJob *vacant();
    /* The job, described, is compiled: on a thread, or here when none can
     * be started. */
    void              start(D3d12PipelineJob &job);
    /* The job compiling or compiled for the pipeline `id`, not collected;
     * null for none. */
    D3d12PipelineJob *pending(uint32_t id);
    /* The job started longest ago and not collected; null for none. */
    D3d12PipelineJob *oldest();
    /* Waits until `job` has compiled - at once for one stop() left
     * uncompiled - and frees its slot, the job cleared: the pipeline, with
     * its reference now the caller's, or null for one the runtime refused
     * or nobody compiled. */
    ID3D12PipelineState *collect(D3d12PipelineJob &job);
    /* The threads go once what they compile is done; jobs started and not yet
     * compiling are compiled no more. Threads start again with the next
     * start(). */
    void              stop();
    /* A compiling thread's body. */
    void              run();

    bool invariants_hold() const;

private:
    enum class Stage : uint8_t { vacant, started, compiling, compiled };

    /* Threads up to what the host has cores for, once. */
    void     start_threads();
    /* The started job with the lowest order; job_count for none. The lock held. */
    uint32_t next_started() const;

    PosixLock         lock_;
    /* Signalled for a started job and for stopping; broadcast as one compiles. */
    PosixCondition    started_;
    PosixCondition    compiled_;
    D3d12PipelineJob  jobs_[job_count];
    Stage             stages_[job_count];
    /* When each job was started, for oldest-first. */
    uint64_t          orders_[job_count];
    uint64_t          next_order_;
    pthread_t         threads_[most_threads];
    uint32_t          thread_count_;
    ID3D12Device     *device_;
    bool              stopping_;
};

}  // namespace av

#endif  // AV_D3D12_PIPELINE_COMPILES_HPP
