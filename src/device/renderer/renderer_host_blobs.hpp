#ifndef AV_RENDERER_HOST_BLOBS_HPP
#define AV_RENDERER_HOST_BLOBS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "device/memory/host_blob_owners.hpp"

#include <stdint.h>

namespace av {

class Host;

class RendererHostBlobs final : public HostBlobOwners {
public:
    explicit RendererHostBlobs(Host &host);
    ~RendererHostBlobs();
    RendererHostBlobs(const RendererHostBlobs &) = delete;
    RendererHostBlobs &operator=(const RendererHostBlobs &) = delete;

    [[nodiscard]] Status initialize();
    [[nodiscard]] Status adopt(uint32_t id, uint32_t context);
    void                 forget(uint32_t id);
    void                 forget_all();
    bool                 owns(uint32_t context, uint32_t id) override;
    uint32_t             live() const;

private:
    Host                           &host_;
    void                           *lock_;
    KeyedSlots<uint32_t, uint32_t> owners_;
};

}

#endif
