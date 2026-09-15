/* One range of a host-visible chunk imported as device memory on the host
 * (feature 045, contracts/host_visible_memory.md §2): the transport's own
 * IMPORT_BLOB command on the context the chunk is bound to, the device memory
 * handle the host answered, and its RELEASE_BLOB. The answer is the host's
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

    /* `size` bytes at `offset` of the bound chunk `resource_id`, as
     * `memory_type`. ok: memory() is the host's handle. invalid_parameter: the
     * host refused, host_status() says with what. not_supported: an empty
     * answer, which is what a device that does not know the command leaves.
     * malformed: any other answer the contract does not allow. invalid_state:
     * the call failed, or this already holds an import. */
    [[nodiscard]] Status import(uint32_t resource_id, uint64_t offset, uint64_t size, uint32_t memory_type);
    /* RELEASE_BLOB of the import held, which is forgotten whatever the host
     * answers; ok with nothing sent when none is held. invalid_state when the
     * call failed, malformed when the host answered with bytes. */
    [[nodiscard]] Status release();
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
