#ifndef AV_DISPLAY_HEADS_HPP
#define AV_DISPLAY_HEADS_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "device/display/scanout_binding.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class GpuRect;
class Host;
class ScanoutFramebuffer;
class StateWriter;
struct DisplayHeadsProbe;

class DisplayHeads {
public:
    static constexpr uint32_t max_heads = 16u;

    DisplayHeads(Host &host, BlobTable &blobs);
    ~DisplayHeads();
    DisplayHeads(const DisplayHeads &) = delete;
    DisplayHeads &operator=(const DisplayHeads &) = delete;

    [[nodiscard]] Status show(uint32_t head, uint32_t blob, const ScanoutFramebuffer &framebuffer);
    [[nodiscard]] Status hide(uint32_t head);
    [[nodiscard]] Status flush(uint32_t blob, const GpuRect &flushed);
    void                 release_all();
    bool                 shows(uint32_t blob) const;
    uint32_t             showing() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    void                  say_once(bool &said, const char *what);
    ScanoutBinding       &binding(uint32_t head);
    const ScanoutBinding *made(uint32_t head) const;

    Host                            &host_;
    BlobTable                       &blobs_;
    bool                             said_no_display_;
    bool                             said_showing_;
    ObjectStorage<ScanoutBinding>    heads_[max_heads];

    friend struct DisplayHeadsProbe;
};

}

#endif
