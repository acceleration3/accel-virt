#include "icd/d3d/swap_chain_buffer.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

SwapChainBuffer::SwapChainBuffer(bool primary, uint32_t misc) : primary_(primary), misc_(misc)
{
    AV_SCOPE("SwapChainBuffer::SwapChainBuffer");
}

SwapChainBuffer::Model SwapChainBuffer::model() const
{
    AV_SCOPE("SwapChainBuffer::model");
    if (primary_) return Model::scanout;
    return (misc_ & misc_shared) != 0u ? Model::flip : Model::blt;
}

}
