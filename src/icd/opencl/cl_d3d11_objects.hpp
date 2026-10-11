/* What one OpenCL context made from a Direct3D 11 device keeps of it
 * (docs/d3d11-sharing.md): the device, and each cl_mem made over one of its
 * resources, with whether OpenCL or Direct3D has it now.
 *
 * Acquire and release take a whole list or none of it: every object is judged
 * before any changes hands, as the extension's errors are about the list, and
 * an acquire or release that fails puts the list back as it found it. Its
 * caller serializes it. */
#ifndef AV_CLICD_CL_D3D11_OBJECTS_HPP
#define AV_CLICD_CL_D3D11_OBJECTS_HPP

#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "icd/opencl/d3d11_object.hpp"

#include <CL/cl.h>

#include <stdint.h>

namespace av {
class BlockSource;
class StateWriter;
}  // namespace av

namespace av::clicd {

class D3d11Resource;

class ClD3d11Objects {
public:
    /* cl_d3d11.h's two errors about whose turn it is. */
    static constexpr cl_int already_acquired = -1008;
    static constexpr cl_int not_acquired = -1009;

    explicit ClD3d11Objects(BlockSource &memory);
    ClD3d11Objects(const ClD3d11Objects &) = delete;
    ClD3d11Objects &operator=(const ClD3d11Objects &) = delete;

    /* The ID3D11Device the context was made from; null for a context that
     * was not. */
    void                 share_with(void *device);
    void                *device() const;
    /* `mem` made over `subresource` of `resource`, which nothing is yet. */
    [[nodiscard]] Status keep(const void *mem, void *resource, uint32_t subresource, const D3d11Resource &described,
                              cl_mem_flags flags);
    /* Whether a cl_mem of this context is over `subresource` of `resource`
     * already, which the extension lets there be one of. */
    bool                 made_over(const void *resource, uint32_t subresource);
    /* The row of `mem`; null for a cl_mem not made over Direct3D 11. */
    D3d11Object         *find(const void *mem);
    /* `mem`'s row dropped, answering the resource it was over; false for a
     * cl_mem not made over Direct3D 11. */
    bool                 forget(const void *mem, void *&resource);
    /* `count` objects handed to OpenCL: CL_INVALID_MEM_OBJECT for one not made
     * over Direct3D 11 here, already_acquired for one OpenCL has. */
    cl_int               take(const cl_mem *objects, cl_uint count);
    /* And handed back: not_acquired for one OpenCL has not got. */
    cl_int               give(const cl_mem *objects, cl_uint count);
    /* What a take whose copies failed gave OpenCL, given back; and what a
     * give whose copies failed gave back, OpenCL's again. */
    void                 untake(const cl_mem *objects, cl_uint count);
    void                 ungive(const cl_mem *objects, cl_uint count);

    uint32_t live() const;
    void     report(StateWriter &writer) const;
    bool     invariants_hold() const;

private:
    /* Whether every one of `count` objects is a row here acquired as
     * `acquired` says; the error for the first that is not. */
    cl_int judge(const cl_mem *objects, cl_uint count, bool acquired, cl_int wrong_turn);

    void                   *device_;
    ObjectPool<D3d11Object> objects_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_D3D11_OBJECTS_HPP
