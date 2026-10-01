/* What the Direct3D 9 driver keeps of each resource and query d3d9.dll made
 * through it, under the handle it handed back. A resource is a host object
 * named by an id - a texture, a buffer, or for a surface resource one host
 * surface per subresource - or, in system memory, only the runtime's own
 * memory the driver copies from when it is blitted or drawn. A subresource a
 * lock may write has a guest shadow, sent to the host when it is unlocked. */
#ifndef AV_UM_D3D9_RECORDS_HPP
#define AV_UM_D3D9_RECORDS_HPP

#include "um/d3d/d3d9_ddi.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av::d3d9 {

struct D3d9Subresource {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    /* System memory: the runtime's, as CreateResource named it. */
    const uint8_t *system;
    uint32_t       system_pitch;
    uint32_t       system_slice_pitch;
    /* A surface resource's host surface for this subresource. */
    uint32_t       id;
    /* The guest copy a lock hands out, made on its first lock. */
    uint8_t       *shadow;
    size_t         shadow_bytes;
    uint32_t       shadow_pitch;
    uint32_t       shadow_slice_pitch;
    /* What the lock open now covers, sent when it is unlocked. */
    bool             locked;
    D3DDDI_LOCKFLAGS lock_flags;
    D3DDDIARG_LOCK   lock;
};

struct D3d9Resource {
    enum class Kind : uint8_t { system, texture, cube, volume, surface, vertex_buffer, index_buffer };

    Kind                 kind;
    /* The host object; for a surface resource, see each subresource's. */
    uint32_t             id;
    D3DDDIFORMAT         format;
    D3DDDI_POOL          pool;
    D3DDDI_RESOURCEFLAGS flags;
    uint32_t             levels;
    uint32_t             count;
    D3d9Subresource     *subresources;
    /* The runtime's handle, which an allocation for the resource names. */
    HANDLE               runtime;
    /* The kernel allocation a present names, made at its first present. */
    D3DKMT_HANDLE        allocation;
    uint32_t             allocation_pitch;
    uint64_t             shared_key;
    /* Each present of the resource, counted from 1 (PresentPointRecord). */
    uint32_t             presents;
};

struct D3d9Query {
    uint32_t        id;
    D3DDDIQUERYTYPE type;
    /* What GetData answers for this type, in bytes. */
    uint32_t        data_bytes;
};

}  // namespace av::d3d9

#endif  // AV_UM_D3D9_RECORDS_HPP
