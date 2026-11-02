#ifndef AV_IMPORTED_BLOB_HPP
#define AV_IMPORTED_BLOB_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class SourceMemory;

class ImportedBlob {
public:
    ImportedBlob();
    ImportedBlob(const ImportedBlob &) = delete;
    ImportedBlob &operator=(const ImportedBlob &) = delete;

    [[nodiscard]] Status take(uint64_t blob_id, SourceMemory &memory);
    SourceMemory        *give_back();

    bool                holds(uint64_t blob_id) const;
    bool                live() const;
    uint64_t            id() const;
    const SourceMemory &memory() const;
    uint64_t            handle() const;

    bool invariants_hold() const;

private:
    friend class ImportTable;
    friend struct ImportedBlobProbe;

    void record(uint64_t handle);

    uint64_t      id_;
    SourceMemory *memory_;
    uint64_t      handle_;
};

}

#endif
