#ifndef AV_HOST_BLOB_HPP
#define AV_HOST_BLOB_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "proto/resources/host_memory_request.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class GuestRamLending;
class Host;

class HostBlob {
public:
    HostBlob(Host &host, uint32_t context, const AvHostMemory &memory);
    HostBlob(Host &host, uint32_t context, uint64_t size, HostMemoryRequest request);
    ~HostBlob();
    HostBlob(const HostBlob &) = delete;
    HostBlob &operator=(const HostBlob &) = delete;

    uint32_t context() const;
    uint64_t size() const;
    uint32_t caching() const;

    bool                 waiting() const;
    HostMemoryRequest    request() const;
    void                 lend(GuestRamLending &lending, bool made_here, const AvHostMemory &memory);
    GuestRamLending     *lending() const;
    bool                 lender() const;
    uint32_t             entries_laid() const;
    void                 laid(uint32_t entries);
    [[nodiscard]] Status map(uint64_t offset);
    [[nodiscard]] Status unmap();
    bool                 mapped() const;
    uint64_t             offset() const;
    bool                 overlaps(uint64_t offset, uint64_t size) const;

private:
    Host            &host_;
    uint64_t         size_;
    uint64_t         offset_;
    uint32_t         context_;
    int              fd_;
    uint32_t         caching_;
    bool             mapped_;
    bool             waiting_;
    bool             lender_;
    uint32_t         request_;
    uint32_t         laid_;
    GuestRamLending *lending_;
};

}

#endif
