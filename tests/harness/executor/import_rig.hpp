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
    static constexpr std::uint32_t context = 1u;
    static constexpr std::uint32_t other_context = 2u;
    static constexpr std::uint64_t page = 4096u;

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
    std::uint32_t          blob(std::uint32_t slot) const;
    [[nodiscard]] Status   create_host_blob(std::uint32_t id, std::uint32_t owner, std::uint32_t pages = 1u);
    std::uint8_t          *memory_of(std::uint32_t id);

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

}

#endif
