/* One context's blob imports over the adversarial host: file-backed guest RAM
 * a test writes pages of, the device's blob table, a mock executor library
 * (version 3) and the context's executor made on one long-lived worker - the
 * thread every import must run on, as the executor ABI requires - with the
 * context's resources beginning at `ring`. For the tests and the fuzz target
 * of what imports a guest blob. */
#ifndef AV_IMPORT_RIG_HPP
#define AV_IMPORT_RIG_HPP

#include "core/support/status.hpp"
#include "device/execution/blob_imports.hpp"
#include "device/execution/context_executor.hpp"
#include "device/execution/executor_library.hpp"
#include "device/execution/executor_shares.hpp"
#include "device/memory/blob_table.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>

namespace av {

class ImportRig {
public:
    static constexpr std::uint64_t ram = 0x80000000u;
    static constexpr std::uint32_t ram_pages = 256u;
    static constexpr std::uint64_t page = 4096u;
    static constexpr std::uint32_t ring = 200u;

    /* `executor` false leaves the context without an executor on its worker. */
    explicit ImportRig(bool executor = true);
    ~ImportRig();
    ImportRig(const ImportRig &) = delete;
    ImportRig &operator=(const ImportRig &) = delete;

    MockHost        &host();
    MockExecutorOps &ops();
    BlobTable       &blobs();
    BlobImports     &imports();
    ContextExecutor &executor();
    /* Guest page `index`, which the rig filled with the byte 0x10 + index. */
    std::uint8_t    *guest_page(std::uint32_t index);
    /* The id of chunk slot `slot`. */
    std::uint32_t    chunk(std::uint32_t slot) const;
    /* A guest blob `id` over one page at each of `pages`, in that order. */
    [[nodiscard]] Status create_guest_blob(std::uint32_t id, const std::uint32_t *pages, std::uint32_t count);

    /* Runs `body` on the context's worker and waits for it. */
    template <typename Body>
    void on_worker(Body body);

private:
    void run_job(void (*job)(void *), void *context);
    void serve();

    MockHost        host_;
    MockExecutorOps ops_;
    ExecutorLibrary library_;
    ExecutorShares  shares_;
    ContextExecutor executor_;
    BlobTable       blobs_;
    BlobImports     imports_;
    void           *start_;
    void           *done_;
    void           *thread_;
    void          (*job_)(void *);
    void           *job_context_;
    bool            stopping_;
};

template <typename Body>
void ImportRig::on_worker(Body body)
{
    run_job([](void *context) { (*static_cast<Body *>(context))(); }, &body);
}

}  // namespace av

#endif  // AV_IMPORT_RIG_HPP
