/* The subresources a device's application has mapped for writing and not yet
 * unmapped, each with the copy flags its own map gave: DISCARD or
 * NO_OVERWRITE, which the upload at the unmap carries to the host's
 * UpdateSubresource1. Kept per subresource rather than per resource because
 * two subresources of one resource may be open at once with different maps -
 * a cube's faces, a texture's mips - and a map of any subresource of any
 * resource is one the unmap must carry back. The table grows with the maps an
 * application holds open; one it cannot make room for is refused at the map
 * rather than lost at the unmap. */
#ifndef AV_D3D_WRITE_MAPS_HPP
#define AV_D3D_WRITE_MAPS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
}  // namespace av

namespace av::d3d {

struct WriteMapsProbe;

class WriteMaps {
public:
    explicit WriteMaps(BlockSource &memory);
    WriteMaps(const WriteMaps &) = delete;
    WriteMaps &operator=(const WriteMaps &) = delete;

    /* `subresource` of `resource` mapped for writing with `flags`; a map of
     * one already open takes the newer flags. invalid_parameter for resource
     * 0, which names nothing; no_memory when the table cannot grow. */
    [[nodiscard]] Status open(uint32_t resource, uint32_t subresource, uint32_t flags);
    /* The map of `subresource` of `resource` closed: true, with its flags,
     * when it was open for writing; false for one never opened or a read. */
    [[nodiscard]] bool   close(uint32_t resource, uint32_t subresource, uint32_t &flags);
    /* Every open map of `resource` forgotten: a resource destroyed mapped. */
    void                 forget(uint32_t resource);
    uint32_t             count() const;

    /* Every open map names a resource. */
    bool invariants_hold() const;

private:
    friend struct WriteMapsProbe;

    uint64_t key_of(uint32_t resource, uint32_t subresource) const;

    KeyedSlots<uint64_t, uint32_t> flags_;
};

}  // namespace av::d3d

#endif  // AV_D3D_WRITE_MAPS_HPP
