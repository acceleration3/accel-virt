#ifndef AV_BLOB_RESOURCE_HPP
#define AV_BLOB_RESOURCE_HPP

#include "core/support/status.hpp"
#include "device/memory/guest_page_alias.hpp"
#include "device/memory/guest_page_mapping.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobWindow;
class ControlRequest;
class Host;

class BlobResource {
public:
    BlobResource(Host &host, uint32_t id);
    BlobResource(const BlobResource &) = delete;
    BlobResource &operator=(const BlobResource &) = delete;

    [[nodiscard]] Status back_with_guest(ControlRequest &request, size_t offset, uint32_t entries, uint64_t size);

    uint32_t id() const;
    uint64_t size() const;
    void     pin();
    void     unpin();
    bool     pinned() const;
    BlobWindow window() const;
    [[nodiscard]] Status view(const uint8_t *&base);

private:
    Host            &host_;
    uint32_t         id_;
    GuestPageMapping guest_;
    uint32_t         pins_;
    GuestPageAlias   alias_;
};

}

#endif
