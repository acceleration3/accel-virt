/* What a Direct3D 12 device's command lists record: every slot of the command
 * list table (D3D12DDI_COMMAND_LIST_FUNCS_3D_0108), each one wire command
 * naming the list, which the host records into the list's own host command
 * list as it arrives. Lists record from whichever thread the application uses;
 * each command is one lock of the process's stream, so commands of two lists
 * interleave on the wire but never within one list.
 *
 * Descriptor handles, GPU addresses and runtime handles are translated here
 * to what the wire names: a (heap, index) pair, the guest's own address, an
 * id. */
#ifndef AV_UM_D3D12_RECORDER_HPP
#define AV_UM_D3D12_RECORDER_HPP

#include "proto/generated/d3d12/d3d12_wire_types.h"
#include "um/d3d/d3d12_ddi.hpp"

#include <stdint.h>

namespace av::d3d12 {

class D3d12Device;

class D3d12Recorder {
public:
    explicit D3d12Recorder(D3d12Device &device);
    D3d12Recorder(const D3d12Recorder &) = delete;
    D3d12Recorder &operator=(const D3d12Recorder &) = delete;

#include "um/d3d/generated/d3d12_recorder_ddi.inc"

private:
    uint32_t list(D3D12DDI_HCOMMANDLIST handle) const;
    void     sent(bool sent_ok, const char *what);
    void     set_root_view(D3D12DDI_HCOMMANDLIST handle, uint32_t compute, UINT parameter, uint32_t kind,
                           D3D12DDI_GPU_VIRTUAL_ADDRESS address);
    /* A copy location from the DDI's placement: a subresource, or a buffer's
     * footprint. */
    d3d12proto::CopyLocation location(const D3D12DDIARG_BUFFER_PLACEMENT &placement,
                                      const D3D12DDIARG_PLACED_RESOURCE &layout) const;

    D3d12Device &device_;
};

}  // namespace av::d3d12

#endif  // AV_UM_D3D12_RECORDER_HPP
