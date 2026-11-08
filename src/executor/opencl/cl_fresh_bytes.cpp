#include "executor/opencl/cl_fresh_bytes.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/opencl/cl_queue_wait.hpp"

namespace av {

ClFreshBytes::ClFreshBytes(ClQueueWait &wait)
    : wait_(wait), context_(nullptr), device_(nullptr), queue_(nullptr), zeroings_(0u), pattern_{}
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

bool ClFreshBytes::zero_buffer(cl_context context, cl_device_id device, cl_mem buffer, uint64_t bytes)
{
    AV_SCOPE("ClFreshBytes::zero_buffer");
    if (bytes == 0u) return true;
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

uint64_t ClFreshBytes::zeroings() const
{
    AV_SCOPE("ClFreshBytes::zeroings");
    return zeroings_;
}

}
