/* One context's imports of host blobs over the adversarial host: the
 * device's host blob table, allocating through a mock executor library whose
 * memory is real, and the context's executor made on one long-lived worker -
 * the thread every import must run on, as the executor ABI requires. For the
 * tests and the fuzz target of what imports a host blob. */
#ifndef AV_IMPORT_RIG_HPP
#define AV_IMPORT_RIG_HPP

#include "core/support/status.hpp"
#include "device/execution/blob_imports.hpp"
#include "device/execution/context_executor.hpp"
#include "device/execution/executor_library.hpp"
#include "device/execution/executor_shares.hpp"
#include "device/memory/host_blob_table.hpp"
#include "device/recording/wire_recorder.hpp"
#include "harness/context/recording_context_plane.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <cstdint>

namespace av {

class ImportRig {
public:
    /* The context whose imports these are, and another one. */
    static constexpr std::uint32_t context = 1u;
    static constexpr std::uint32_t other_context = 2u;
    static constexpr std::uint64_t page = 4096u;

    /* `executor` false leaves the context without an executor on its worker. */
    explicit ImportRig(bool executor = true);
    ~ImportRig();
    ImportRig(const ImportRig &) = delete;
    ImportRig &operator=(const ImportRig &) = delete;

    MockHost              &host();
    MockExecutorOps       &ops();
    HostBlobTable         &host_blobs();
    RecordingContextPlane &plane();
    BlobImports           &imports();
    WireRecorder          &recorder();
    ContextExecutor       &executor();
    /* The id of host blob slot `slot`. */
    std::uint32_t          blob(std::uint32_t slot) const;
    /* Host blob `id` of `pages` pages of upload memory, made for `owner`. */
    [[nodiscard]] Status   create_host_blob(std::uint32_t id, std::uint32_t owner, std::uint32_t pages = 1u);
    /* The library's own view of host blob `id`'s memory, null for none. */
    std::uint8_t          *memory_of(std::uint32_t id);

    /* Runs `body` on the context's worker and waits for it. */
    template <typename Body>
    void on_worker(Body body);

private:
    void run_job(void (*job)(void *), void *argument);
    void serve();

    MockHost              host_;
    MockExecutorOps       ops_;
    WireRecorder          recorder_;
    ExecutorLibrary       library_;
    ExecutorShares        shares_;
    ContextExecutor       executor_;
    RecordingContextPlane plane_;
    HostBlobTable         host_blobs_;
    BlobImports           imports_;
    void                 *start_;
    void                 *done_;
    void                 *thread_;
    void                (*job_)(void *);
    void                 *job_context_;
    bool                  stopping_;
};

template <typename Body>
void ImportRig::on_worker(Body body)
{
    run_job([](void *context) { (*static_cast<Body *>(context))(); }, &body);
}

}  // namespace av

#endif  // AV_IMPORT_RIG_HPP
