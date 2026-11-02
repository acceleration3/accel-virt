#ifndef AV_SCANOUT_BINDING_HPP
#define AV_SCANOUT_BINDING_HPP

#include "core/support/status.hpp"
#include "device/display/scanout_framebuffer.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class GpuRect;
class Host;
class StateWriter;
struct DisplayHeadsProbe;
struct ScanoutBindingProbe;

class ScanoutBinding {
public:
    ScanoutBinding(Host &host, BlobTable &blobs, uint32_t head);
    ~ScanoutBinding();
    ScanoutBinding(const ScanoutBinding &) = delete;
    ScanoutBinding &operator=(const ScanoutBinding &) = delete;

    [[nodiscard]] Status show(uint32_t blob, const ScanoutFramebuffer &framebuffer);
    void                 hide();
    [[nodiscard]] Status update(const GpuRect &flushed);

    bool     shows() const;
    uint32_t head() const;
    uint32_t blob() const;
    uint64_t frames() const;
    void     report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    bool visible_part(const GpuRect &flushed, uint32_t &x, uint32_t &y, uint32_t &width,
                      uint32_t &height) const;

    Host              &host_;
    BlobTable         &blobs_;
    uint32_t           head_;
    uint32_t           blob_;
    ScanoutFramebuffer framebuffer_;
    bool               shown_;
    uint64_t           frames_;

    friend struct DisplayHeadsProbe;
    friend struct ScanoutBindingProbe;
};

}

#endif
