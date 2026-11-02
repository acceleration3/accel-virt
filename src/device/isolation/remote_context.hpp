#ifndef AV_REMOTE_CONTEXT_HPP
#define AV_REMOTE_CONTEXT_HPP

#include "core/support/status.hpp"
#include "device/context/context_binding.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class CapsetInfo;
class ContextSetup;
class FenceInterrupts;
class Host;
class RendererLink;

class RemoteContext {
public:
    RemoteContext(Host &host, BlobTable &blobs, FenceInterrupts &interrupts, uint32_t id);
    ~RemoteContext();
    RemoteContext(const RemoteContext &) = delete;
    RemoteContext &operator=(const RemoteContext &) = delete;

    [[nodiscard]] Status bind(const ContextSetup &setup, const CapsetInfo &capset, RendererLink &link,
                              Status &outcome);
    void                 unbind();
    void                 lose();

    uint32_t id() const;
    bool     bound() const;
    bool     lost() const;
    SubApi   sub_api() const;
    uint32_t doorbell() const;

    bool invariants_hold() const;

private:
    [[nodiscard]] Status connect_doorbell(uint32_t index, int &descriptor);
    void                 disconnect_doorbell();

    Host            &host_;
    FenceInterrupts &interrupts_;
    ContextBinding   binding_;
    uint32_t         id_;
    void            *doorbell_event_;
    bool             lost_;
};

}

#endif
