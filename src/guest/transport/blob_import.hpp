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

    [[nodiscard]] Status import(uint32_t resource_id);
    [[nodiscard]] Status release();
    void                 forget();
    uint64_t             memory() const;
    uint32_t             host_status() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status judge(WireReader &answer);

    ClientTransport &transport_;
    uint64_t         memory_;
    uint32_t         host_status_;

    friend struct BlobImportProbe;
};

}

#endif
