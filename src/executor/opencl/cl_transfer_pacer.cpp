#include "executor/opencl/cl_transfer_pacer.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClTransferPacer::ClTransferPacer() : gathered_(0u), previous_(nullptr)
{
    AV_SCOPE("ClTransferPacer::ClTransferPacer");
}

ClTransferPacer::~ClTransferPacer()
{
    AV_SCOPE("ClTransferPacer::~ClTransferPacer");
    stop();
}

void ClTransferPacer::transferred(cl_command_queue queue, uint64_t bytes)
{
    AV_SCOPE("ClTransferPacer::transferred");
    gathered_ += bytes;
    if (gathered_ >= window_bytes) settle(queue);
}

void ClTransferPacer::settle(cl_command_queue queue)
{
    AV_SCOPE("ClTransferPacer::settle");
    gathered_ = 0u;
    cl_event marker = nullptr;
    if (::clEnqueueMarkerWithWaitList(queue, 0u, nullptr, &marker) != CL_SUCCESS) marker = nullptr;
    static_cast<void>(::clFlush(queue));
    if (previous_ != nullptr) {
        static_cast<void>(::clWaitForEvents(1u, &previous_));
        static_cast<void>(::clReleaseEvent(previous_));
    }
    previous_ = marker;
}

void ClTransferPacer::stop()
{
    AV_SCOPE("ClTransferPacer::stop");
    gathered_ = 0u;
    if (previous_ != nullptr) static_cast<void>(::clReleaseEvent(previous_));
    previous_ = nullptr;
}

}
