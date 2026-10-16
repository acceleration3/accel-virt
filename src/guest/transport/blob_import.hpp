/* One host window chunk imported into the context that opened it
 * (docs/host-memory.md): the transport's own IMPORT_BLOB command, the handle
 * the host answered - for Vulkan the VkDeviceMemory - and its RELEASE_BLOB. The answer is the host's
 * word, read from the reply slot: a BlobImportAnswer of exactly its size. */
#ifndef AV_BLOB_IMPORT_HPP
#define AV_BLOB_IMPORT_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class ClientTransport;
class WireReader;
struct BlobImportProbe;

class BlobImport {
public:
    explicit BlobImport(ClientTransport &transport);
    BlobImport(const BlobImport &) = delete;
    BlobImport &operator=(const BlobImport &) = delete;

    /* Chunk `resource_id`, whole. ok: memory() is the host's handle. no_memory:
     * the host had no memory to keep the import in. invalid_parameter: the
     * host refused otherwise, host_status() says with what. not_supported: an empty
     * answer, which is what a device that does not know the command leaves.
     * malformed: any other answer the contract does not allow. invalid_state:
     * the call failed, or this already holds an import. */
    [[nodiscard]] Status import(uint32_t resource_id);
    /* RELEASE_BLOB of the import held, which is forgotten whatever the host
     * answers; ok with nothing sent when none is held. invalid_state when the
     * call failed, malformed when the host answered with bytes. */
    [[nodiscard]] Status release();
    /* The import held is dropped with nothing sent: its context is gone on
     * the host, which frees what the context imported. */
    void                 forget();
    /* The device memory the host answered; 0 unless an import is held. */
    uint64_t             memory() const;
    /* The status of the host's last refusal; 0 after an import it took. */
    uint32_t             host_status() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status judge(WireReader &answer);

    ClientTransport &transport_;
    uint64_t         memory_;
    uint32_t         host_status_;

    friend struct BlobImportProbe;
};

}  // namespace av

#endif  // AV_BLOB_IMPORT_HPP
