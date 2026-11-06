#ifndef AV_SCRIPTED_D3D11_BRIDGE_HPP
#define AV_SCRIPTED_D3D11_BRIDGE_HPP

#include "icd/opencl/d3d11_bridge.hpp"
#include "icd/opencl/d3d11_resource.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class ScriptedD3d11Bridge final : public clicd::D3d11Bridge {
public:
    static constexpr std::uint32_t max_resources = 8u;
    static constexpr std::size_t   staging_bytes = 4096u;

    ScriptedD3d11Bridge();

    void name_device(void *device);
    void name_resource(void *resource, void *device, const clicd::D3d11Resource &described, std::uint8_t *bytes,
                       std::size_t size, std::size_t row_pitch, std::size_t slice_pitch);
    void refuse_next();
    void offer_twin(void *twin, std::uint64_t handle, std::uint64_t bytes = 0u);
    void on_twin(void (*hook)(void *context), void *context);
    std::uint32_t twin_device() const;

    bool   device(void *object) override;
    bool   describe(void *object, void *device, clicd::D3d11Resource &described) override;
    void   hold(void *object) override;
    void   let_go(void *object) override;
    cl_int read(void *device, void *resource, std::uint32_t subresource, clicd::D3d11Rows &rows) override;
    cl_int write(void *device, void *resource, std::uint32_t subresource, clicd::D3d11Rows &rows) override;
    void  *twin(void *device, void *resource, std::uint32_t subresource, std::uint32_t kmt_device,
               std::uint64_t &handle, std::uint64_t &bytes) override;
    void   close_handle(std::uint64_t handle) override;
    cl_int to_twin(void *device, void *resource, std::uint32_t subresource, void *twin) override;
    cl_int from_twin(void *device, void *twin, void *resource, std::uint32_t subresource) override;

    std::int32_t  references(const void *object) const;
    std::uint32_t reads() const;
    std::uint32_t writes() const;
    std::uint32_t subresource() const;
    std::uint32_t closed() const;
    std::uint32_t to_twins() const;
    std::uint32_t from_twins() const;

private:
    struct Resource {
        void                *object;
        void                *device;
        clicd::D3d11Resource described;
        std::uint8_t        *bytes;
        std::size_t          size;
        std::size_t          row_pitch;
        std::size_t          slice_pitch;
    };

    Resource    *named(const void *object);
    bool         refused();

    void         *device_;
    Resource      resources_[max_resources];
    std::uint32_t count_;
    const void   *held_[max_resources + 1u];
    std::int32_t  references_[max_resources + 1u];
    std::uint8_t  staging_[staging_bytes];
    std::uint32_t reads_;
    std::uint32_t writes_;
    std::uint32_t subresource_;
    bool          refuse_;
    void         *twin_;
    std::uint64_t twin_handle_;
    std::uint64_t twin_bytes_;
    std::uint32_t twin_device_;
    std::uint32_t closed_;
    std::uint32_t to_twins_;
    std::uint32_t from_twins_;
    void        (*twin_hook_)(void *context);
    void         *twin_hook_context_;
};

}

#endif
