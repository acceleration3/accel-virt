#include "icd/opencl/cl_d3d11_objects.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/opencl/d3d11_resource.hpp"

namespace av::clicd {

ClD3d11Objects::ClD3d11Objects(BlockSource &memory) : device_(nullptr), d3d10_(false), objects_(memory)
{
    AV_SCOPE("ClD3d11Objects::ClD3d11Objects");
}

void ClD3d11Objects::share_with(void *device, bool d3d10)
{
    AV_SCOPE("ClD3d11Objects::share_with");
    device_ = device;
    d3d10_ = d3d10;
}

bool ClD3d11Objects::d3d10() const
{
    AV_SCOPE("ClD3d11Objects::d3d10");
    return d3d10_;
}

void *ClD3d11Objects::device() const
{
    AV_SCOPE("ClD3d11Objects::device");
    return device_;
}

Status ClD3d11Objects::keep(const void *mem, void *resource, uint32_t subresource, const D3d11Resource &described,
                            cl_mem_flags flags)
{
    AV_SCOPE("ClD3d11Objects::keep");
    D3d11Object *made = nullptr;
    return objects_.create(made, mem, resource, subresource, described, flags);
}

bool ClD3d11Objects::made_over(const void *resource, uint32_t subresource)
{
    AV_SCOPE("ClD3d11Objects::made_over");
    return objects_.find_if([&](const D3d11Object &object) { return object.over(resource, subresource); }) !=
           nullptr;
}

D3d11Object *ClD3d11Objects::find(const void *mem)
{
    AV_SCOPE("ClD3d11Objects::find");
    return objects_.find_if([&](const D3d11Object &object) { return object.is(mem); });
}

bool ClD3d11Objects::forget(const void *mem, void *&resource)
{
    AV_SCOPE("ClD3d11Objects::forget");
    resource = nullptr;
    D3d11Object *const object = find(mem);
    if (object == nullptr) return false;
    resource = object->resource();
    static_cast<void>(objects_.destroy(object));
    return true;
}

cl_int ClD3d11Objects::take(const cl_mem *objects, cl_uint count)
{
    AV_SCOPE("ClD3d11Objects::take");
    const cl_int judged = judge(objects, count, false, d3d10_ ? already_acquired_d3d10 : already_acquired);
    if (judged != CL_SUCCESS) return judged;
    for (cl_uint index = 0u; index < count; ++index) find(objects[index])->acquire();
    return CL_SUCCESS;
}

cl_int ClD3d11Objects::give(const cl_mem *objects, cl_uint count)
{
    AV_SCOPE("ClD3d11Objects::give");
    const cl_int judged = judge(objects, count, true, d3d10_ ? not_acquired_d3d10 : not_acquired);
    if (judged != CL_SUCCESS) return judged;
    for (cl_uint index = 0u; index < count; ++index) find(objects[index])->release();
    return CL_SUCCESS;
}

void ClD3d11Objects::untake(const cl_mem *objects, cl_uint count)
{
    AV_SCOPE("ClD3d11Objects::untake");
    for (cl_uint index = 0u; index < count; ++index) {
        D3d11Object *const object = find(objects[index]);
        if (object != nullptr) object->release();
    }
}

void ClD3d11Objects::ungive(const cl_mem *objects, cl_uint count)
{
    AV_SCOPE("ClD3d11Objects::ungive");
    for (cl_uint index = 0u; index < count; ++index) {
        D3d11Object *const object = find(objects[index]);
        if (object != nullptr) object->acquire();
    }
}

cl_int ClD3d11Objects::judge(const cl_mem *objects, cl_uint count, bool acquired, cl_int wrong_turn)
{
    AV_SCOPE("ClD3d11Objects::judge");
    if (objects == nullptr || count == 0u) return CL_INVALID_VALUE;
    for (cl_uint index = 0u; index < count; ++index) {
        const D3d11Object *const object = find(objects[index]);
        if (object == nullptr) return CL_INVALID_MEM_OBJECT;
        if (object->acquired() != acquired) return wrong_turn;
    }
    return CL_SUCCESS;
}

uint32_t ClD3d11Objects::live() const
{
    AV_SCOPE("ClD3d11Objects::live");
    return objects_.live();
}

void ClD3d11Objects::report(StateWriter &writer) const
{
    AV_SCOPE("ClD3d11Objects::report");
    writer.open(d3d10_ ? "d3d10" : "d3d11", 0u);
    writer.address("device", device_);
    writer.number("live", objects_.live());
    uint64_t index = 0u;
    objects_.for_each([&](const D3d11Object &object) {
        writer.open("object", index++);
        writer.address("resource", object.resource());
        writer.number("subresource", object.subresource());
        writer.flag("acquired", object.acquired());
        writer.close();
    });
    writer.close();
}

bool ClD3d11Objects::invariants_hold() const
{
    AV_SCOPE("ClD3d11Objects::invariants_hold");
    if (device_ == nullptr && objects_.live() != 0u) return false;
    return objects_.all_of([](const D3d11Object &object) { return object.resource() != nullptr; });
}

}
