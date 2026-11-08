#include "icd/d3d/swap_chain_buffer.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

SwapChainBuffer::SwapChainBuffer() : primary_(false), misc_(0u)
{
    AV_SCOPE("SwapChainBuffer::SwapChainBuffer");
}

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

SwapChainBuffer::Model SwapChainBuffer::model_for(bool blt, bool flip) const
{
    AV_SCOPE("SwapChainBuffer::model_for");
    if (primary_) return Model::scanout;
    if (blt && !flip) return Model::blt;
    return model();
}

}
