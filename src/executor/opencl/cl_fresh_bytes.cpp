#include "executor/opencl/cl_fresh_bytes.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/opencl/cl_freed_memory.hpp"
#include "executor/opencl/cl_queue_wait.hpp"

namespace av {

ClFreshBytes::ClFreshBytes(ClQueueWait &wait)
    : wait_(wait), context_(nullptr), device_(nullptr), queue_(nullptr), freed_(nullptr), own_(0u), zeroings_(0u), skipped_(0u), pattern_{}
{
    AV_SCOPE("ClFreshBytes::ClFreshBytes");
}

ClFreshBytes::~ClFreshBytes()
{
    AV_SCOPE("ClFreshBytes::~ClFreshBytes");
    release();
}

void ClFreshBytes::release()
{
    AV_SCOPE("ClFreshBytes::release");
    if (queue_ != nullptr) static_cast<void>(::clReleaseCommandQueue(queue_));
    queue_ = nullptr;
    context_ = nullptr;
    device_ = nullptr;
}

cl_command_queue ClFreshBytes::queue_for(cl_context context, cl_device_id device)
{
    AV_SCOPE("ClFreshBytes::queue_for");
    if (queue_ != nullptr && context_ == context && device_ == device) return queue_;
    release();
    cl_int made = CL_SUCCESS;
    queue_ = ::clCreateCommandQueueWithProperties(context, device, nullptr, &made);
    if (queue_ == nullptr) return nullptr;
    context_ = context;
    device_ = device;
    return queue_;
}

bool ClFreshBytes::finished()
{
    AV_SCOPE("ClFreshBytes::finished");
    return wait_.through(queue_, zero_nanoseconds);
}

uint64_t ClFreshBytes::pattern_bytes(uint64_t bytes) const
{
    AV_SCOPE("ClFreshBytes::pattern_bytes");
    const uint64_t lowest = bytes & (~bytes + 1u);
    return lowest < widest_pattern ? lowest : widest_pattern;
}

void ClFreshBytes::share(ClFreedMemory *freed)
{
    AV_SCOPE("ClFreshBytes::share");
    freed_ = freed;
    own_ = 0u;
}

void ClFreshBytes::leave()
{
    AV_SCOPE("ClFreshBytes::leave");
    own_ = 0u;
}

uint64_t ClFreshBytes::reach_of(uint64_t bytes) const
{
    AV_SCOPE("ClFreshBytes::reach_of");
    if (bytes > (ClFreedMemory::saturated >> 2)) return ClFreedMemory::saturated >> 1;
    uint64_t rounded = 1u;
    while (rounded < bytes) rounded <<= 1;
    return rounded << 1;
}

void ClFreshBytes::freed(uint64_t bytes)
{
    AV_SCOPE("ClFreshBytes::freed");
    if (freed_ == nullptr || bytes == 0u) return;
    const uint64_t reach = reach_of(bytes);
    freed_->add(reach);
    own_ = reach > ClFreedMemory::saturated - own_ ? ClFreedMemory::saturated : own_ + reach;
}

bool ClFreshBytes::may_hold_others(uint64_t bytes) const
{
    AV_SCOPE("ClFreshBytes::may_hold_others");
    if (freed_ == nullptr) return true;
    const uint64_t total = freed_->total();
    if (total == ClFreedMemory::saturated || own_ == ClFreedMemory::saturated) return true;
    const uint64_t others = total - own_;
    return bytes < slab_bytes ? others != 0u : others >= bytes;
}

bool ClFreshBytes::zero_buffer(cl_context context, cl_device_id device, cl_mem buffer, uint64_t bytes)
{
    AV_SCOPE("ClFreshBytes::zero_buffer");
    if (bytes == 0u) return true;
    if (!may_hold_others(bytes)) {
        skipped_++;
        return true;
    }
    cl_command_queue const queue = queue_for(context, device);
    if (queue == nullptr) return false;
    if (::clEnqueueFillBuffer(queue, buffer, pattern_, static_cast<size_t>(pattern_bytes(bytes)), 0u,
                              static_cast<size_t>(bytes), 0u, nullptr, nullptr) != CL_SUCCESS)
        return false;
    if (!finished()) return false;
    zeroings_++;
    return true;
}

bool ClFreshBytes::zero_svm(cl_context context, cl_device_id device, void *memory, uint64_t bytes)
{
    AV_SCOPE("ClFreshBytes::zero_svm");
    if (bytes == 0u) return true;
    if (!may_hold_others(bytes)) {
        skipped_++;
        return true;
    }
    cl_command_queue const queue = queue_for(context, device);
    if (queue == nullptr) return false;
    if (::clEnqueueSVMMemFill(queue, memory, pattern_, static_cast<size_t>(pattern_bytes(bytes)),
                              static_cast<size_t>(bytes), 0u, nullptr, nullptr) != CL_SUCCESS)
        return false;
    if (!finished()) return false;
    zeroings_++;
    return true;
}

bool ClFreshBytes::zero_image(cl_context context, cl_device_id device, cl_mem image)
{
    AV_SCOPE("ClFreshBytes::zero_image");
    cl_mem_object_type type = 0u;
    cl_mem             backing = nullptr;
    size_t             width = 0u, height = 0u, depth = 0u, layers = 0u;
    if (::clGetMemObjectInfo(image, CL_MEM_TYPE, sizeof type, &type, nullptr) != CL_SUCCESS ||
        ::clGetMemObjectInfo(image, CL_MEM_ASSOCIATED_MEMOBJECT, sizeof backing, &backing, nullptr) != CL_SUCCESS)
        return false;
    if (backing != nullptr || type == CL_MEM_OBJECT_IMAGE1D_BUFFER) return true;
    if (::clGetImageInfo(image, CL_IMAGE_WIDTH, sizeof width, &width, nullptr) != CL_SUCCESS ||
        ::clGetImageInfo(image, CL_IMAGE_HEIGHT, sizeof height, &height, nullptr) != CL_SUCCESS ||
        ::clGetImageInfo(image, CL_IMAGE_DEPTH, sizeof depth, &depth, nullptr) != CL_SUCCESS ||
        ::clGetImageInfo(image, CL_IMAGE_ARRAY_SIZE, sizeof layers, &layers, nullptr) != CL_SUCCESS)
        return false;
    size_t bytes = 0u;
    if (freed_ != nullptr &&
        (::clGetMemObjectInfo(image, CL_MEM_SIZE, sizeof bytes, &bytes, nullptr) != CL_SUCCESS || bytes == 0u))
        bytes = 1u;
    if (!may_hold_others(bytes)) {
        skipped_++;
        return true;
    }
    size_t region[3] = {width, height == 0u ? 1u : height, depth == 0u ? 1u : depth};
    if (type == CL_MEM_OBJECT_IMAGE1D_ARRAY) region[1] = layers;
    if (type == CL_MEM_OBJECT_IMAGE2D_ARRAY) region[2] = layers;
    cl_command_queue const queue = queue_for(context, device);
    if (queue == nullptr) return false;
    const size_t origin[3] = {0u, 0u, 0u};
    if (::clEnqueueFillImage(queue, image, pattern_, origin, region, 0u, nullptr, nullptr) != CL_SUCCESS)
        return false;
    if (!finished()) return false;
    zeroings_++;
    return true;
}

uint64_t ClFreshBytes::skipped() const
{
    AV_SCOPE("ClFreshBytes::skipped");
    return skipped_;
}

uint64_t ClFreshBytes::zeroings() const
{
    AV_SCOPE("ClFreshBytes::zeroings");
    return zeroings_;
}

}
