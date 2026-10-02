/* The subresources a device's application has mapped for writing and not yet
 * unmapped, each with the copy flags its own map gave: DISCARD or
 * NO_OVERWRITE, which the upload at the unmap carries to the host's
 * UpdateSubresource1. Kept per subresource rather than per resource because
 * two subresources of one resource may be open at once with different maps -
 * a cube's faces, a texture's mips - and a map of any subresource of any
 * resource is one the unmap must carry back. Fixed in size: an application
 * holds a handful of maps open at a time, and one past the table is refused
 * at the map rather than lost at the unmap. */
#ifndef AV_D3D_WRITE_MAPS_HPP
#define AV_D3D_WRITE_MAPS_HPP

#include <stdint.h>

namespace av::d3d {

struct WriteMapsProbe;

class WriteMaps {
public:
    static constexpr uint32_t capacity = 256u;

    WriteMaps();
    WriteMaps(const WriteMaps &) = delete;
    WriteMaps &operator=(const WriteMaps &) = delete;

    /* `subresource` of `resource` mapped for writing with `flags`; a map of
     * one already open takes the newer flags. False when the table is full;
     * resource 0 names nothing. */
    [[nodiscard]] bool open(uint32_t resource, uint32_t subresource, uint32_t flags);
    /* The map of `subresource` of `resource` closed: true, with its flags,
     * when it was open for writing; false for one never opened or a read. */
    [[nodiscard]] bool close(uint32_t resource, uint32_t subresource, uint32_t &flags);
    /* Every open map of `resource` forgotten: a resource destroyed mapped. */
    void               forget(uint32_t resource);
    uint32_t           count() const;

    /* An open row names a resource, no subresource of one is open twice, and
     * the rows past the count are empty. */
    bool invariants_hold() const;

private:
    friend struct WriteMapsProbe;

    /* The row of `subresource` of `resource`, or capacity. */
    uint32_t row_of(uint32_t resource, uint32_t subresource) const;
    void     drop(uint32_t row);

    uint32_t resources_[capacity];
    uint32_t subresources_[capacity];
    uint32_t flags_[capacity];
    uint32_t count_;
};

}  // namespace av::d3d

#endif  // AV_D3D_WRITE_MAPS_HPP
