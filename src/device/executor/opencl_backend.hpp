/* One guest context's OpenCL, run on the host's own.
 *
 * The generated `RealDispatch` already forwards all 105 commands the profile
 * carries into whatever OpenCL this process loaded, so what this adds is the
 * three answers the ABI needs: whether an id is carried, whether a frame
 * decoded, and whether the host lost its device. */
#ifndef AV_OPENCL_BACKEND_HPP
#define AV_OPENCL_BACKEND_HPP

#include "device/executor/handle_registry.hpp"
#include "device/executor/session_backend.hpp"
#include "proto/generated/opencl/cl_real_dispatch.h"

#include <stdint.h>

namespace av {

class OpenClBackend final : public clproto::RealDispatch, public SessionBackend {
public:
    OpenClBackend();

    bool knows(uint32_t command_id) const override;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;
    [[nodiscard]] bool adopt(ImportedBlob &blob) override;
    void abandon(ImportedBlob &blob) override;

    /* Releases what the guest never did. A context that dies owning a context
     * or a queue must take it with it. */
    void     shut_down();
    uint64_t ran() const;

private:
    /* Every OpenCL command id carries this in its top 16 bits. */
    static constexpr uint32_t opencl_api = 2u;

    HandleRegistry handles_;
    uint64_t       ran_;
    bool           lost_;
};

}  // namespace av

#endif  // AV_OPENCL_BACKEND_HPP
