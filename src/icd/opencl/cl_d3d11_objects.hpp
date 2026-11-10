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
}

namespace av::clicd {

class D3d11Resource;

class ClD3d11Objects {
public:
    static constexpr cl_int already_acquired = -1008;
    static constexpr cl_int not_acquired = -1009;
    static constexpr cl_int already_acquired_d3d10 = -1004;
    static constexpr cl_int not_acquired_d3d10 = -1005;

    explicit ClD3d11Objects(BlockSource &memory);
    ClD3d11Objects(const ClD3d11Objects &) = delete;
    ClD3d11Objects &operator=(const ClD3d11Objects &) = delete;

    void                 share_with(void *device, bool d3d10 = false);
    bool                 d3d10() const;
    void                *device() const;
    [[nodiscard]] Status keep(const void *mem, void *resource, uint32_t subresource, const D3d11Resource &described,
                              cl_mem_flags flags);
    bool                 made_over(const void *resource, uint32_t subresource);
    D3d11Object         *find(const void *mem);
    bool                 forget(const void *mem, void *&resource);
    cl_int               take(const cl_mem *objects, cl_uint count);
    cl_int               give(const cl_mem *objects, cl_uint count);
    void                 untake(const cl_mem *objects, cl_uint count);
    void                 ungive(const cl_mem *objects, cl_uint count);

    uint32_t live() const;
    void     report(StateWriter &writer) const;
    bool     invariants_hold() const;

private:
    cl_int judge(const cl_mem *objects, cl_uint count, bool acquired, cl_int wrong_turn);

    void                   *device_;
    bool                    d3d10_;
    ObjectPool<D3d11Object> objects_;
};

}

#endif
