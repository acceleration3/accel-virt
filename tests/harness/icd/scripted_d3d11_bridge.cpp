#include "harness/icd/scripted_d3d11_bridge.hpp"

#include "icd/opencl/d3d11_rows.hpp"

#include <cstring>

namespace av {

ScriptedD3d11Bridge::ScriptedD3d11Bridge()
    : device_(nullptr), resources_{}, count_(0u), held_{}, references_{}, staging_{}, reads_(0u), writes_(0u),
      subresource_(0u), refuse_(false), twin_(nullptr), twin_handle_(0u), twin_bytes_(0u), twin_device_(0u), closed_(0u), to_twins_(0u),
      from_twins_(0u), twin_hook_(nullptr), twin_hook_context_(nullptr)
{
}

void ScriptedD3d11Bridge::name_device(void *device) { device_ = device; }

void ScriptedD3d11Bridge::name_resource(void *resource, void *device, const clicd::D3d11Resource &described,
                                        std::uint8_t *bytes, std::size_t size, std::size_t row_pitch,
                                        std::size_t slice_pitch)
{
    if (count_ == max_resources) return;
    resources_[count_++] = Resource{resource, device, described, bytes, size, row_pitch, slice_pitch};
}

void ScriptedD3d11Bridge::refuse_next() { refuse_ = true; }

bool ScriptedD3d11Bridge::device(void *object) { return object != nullptr && object == device_; }

bool ScriptedD3d11Bridge::describe(void *object, void *device, clicd::D3d11Resource &described)
{
    described = clicd::D3d11Resource();
    const Resource *const resource = named(object);
    if (resource == nullptr || resource->device != device) return false;
    described = resource->described;
    return true;
}

void ScriptedD3d11Bridge::hold(void *object)
{
    for (std::uint32_t index = 0u; index <= max_resources; ++index) {
        if (held_[index] == object || held_[index] == nullptr) {
            held_[index] = object;
            ++references_[index];
            return;
        }
    }
}

void ScriptedD3d11Bridge::let_go(void *object)
{
    for (std::uint32_t index = 0u; index <= max_resources; ++index) {
        if (held_[index] == object) {
            --references_[index];
            return;
        }
    }
}

cl_int ScriptedD3d11Bridge::read(void *device, void *resource, std::uint32_t subresource, clicd::D3d11Rows &rows)
{
    ++reads_;
    subresource_ = subresource;
    Resource *const named_resource = named(resource);
    if (refused() || named_resource == nullptr || device != device_ || named_resource->size > staging_bytes)
        return CL_OUT_OF_RESOURCES;
    std::memcpy(staging_, named_resource->bytes, named_resource->size);
    return rows.visit(staging_, named_resource->row_pitch, named_resource->slice_pitch);
}

cl_int ScriptedD3d11Bridge::write(void *device, void *resource, std::uint32_t subresource, clicd::D3d11Rows &rows)
{
    ++writes_;
    subresource_ = subresource;
    Resource *const named_resource = named(resource);
    if (refused() || named_resource == nullptr || device != device_ || named_resource->size > staging_bytes)
        return CL_OUT_OF_RESOURCES;
    std::memset(staging_, 0, sizeof staging_);
    const cl_int visited = rows.visit(staging_, named_resource->row_pitch, named_resource->slice_pitch);
    if (visited == CL_SUCCESS) std::memcpy(named_resource->bytes, staging_, named_resource->size);
    return visited;
}

std::int32_t ScriptedD3d11Bridge::references(const void *object) const
{
    for (std::uint32_t index = 0u; index <= max_resources; ++index)
        if (held_[index] == object) return references_[index];
    return 0;
}

std::uint32_t ScriptedD3d11Bridge::reads() const { return reads_; }

std::uint32_t ScriptedD3d11Bridge::writes() const { return writes_; }

std::uint32_t ScriptedD3d11Bridge::subresource() const { return subresource_; }

void ScriptedD3d11Bridge::offer_twin(void *twin, std::uint64_t handle, std::uint64_t bytes)
{
    twin_ = twin;
    twin_handle_ = handle;
    twin_bytes_ = bytes;
}

void ScriptedD3d11Bridge::on_twin(void (*hook)(void *context), void *context)
{
    twin_hook_ = hook;
    twin_hook_context_ = context;
}

std::uint32_t ScriptedD3d11Bridge::twin_device() const { return twin_device_; }

void *ScriptedD3d11Bridge::twin(void *, void *, std::uint32_t, std::uint32_t kmt_device, std::uint64_t &handle,
                                std::uint64_t &bytes)
{
    if (twin_hook_ != nullptr) twin_hook_(twin_hook_context_);
    void *const made = twin_;
    twin_device_ = kmt_device;
    handle = twin_handle_;
    bytes = twin_bytes_;
    twin_ = nullptr;
    twin_handle_ = 0u;
    twin_bytes_ = 0u;
    if (made != nullptr) hold(made);
    return made;
}

void ScriptedD3d11Bridge::close_handle(std::uint64_t) { closed_++; }

cl_int ScriptedD3d11Bridge::to_twin(void *, void *, std::uint32_t subresource, void *)
{
    subresource_ = subresource;
    if (refused()) return CL_OUT_OF_RESOURCES;
    to_twins_++;
    return CL_SUCCESS;
}

cl_int ScriptedD3d11Bridge::from_twin(void *, void *, void *, std::uint32_t subresource)
{
    subresource_ = subresource;
    if (refused()) return CL_OUT_OF_RESOURCES;
    from_twins_++;
    return CL_SUCCESS;
}

std::uint32_t ScriptedD3d11Bridge::closed() const { return closed_; }

std::uint32_t ScriptedD3d11Bridge::to_twins() const { return to_twins_; }

std::uint32_t ScriptedD3d11Bridge::from_twins() const { return from_twins_; }

ScriptedD3d11Bridge::Resource *ScriptedD3d11Bridge::named(const void *object)
{
    for (std::uint32_t index = 0u; index < count_; ++index)
        if (object != nullptr && resources_[index].object == object) return &resources_[index];
    return nullptr;
}

bool ScriptedD3d11Bridge::refused()
{
    const bool refusing = refuse_;
    refuse_ = false;
    return refusing;
}

}
