#include "device/host/process_block_source.hpp"

#include "core/trace/function_scope.hpp"

extern "C" {
void *calloc(size_t count, size_t bytes);
void  free(void *block);
}

namespace av {

ProcessBlockSource::ProcessBlockSource() : blocks_(0u)
{
    AV_SCOPE("ProcessBlockSource::ProcessBlockSource");
}

void *ProcessBlockSource::allocate(size_t bytes, Pool)
{
    AV_SCOPE("ProcessBlockSource::allocate");
    void *const block = calloc(1u, bytes);
    blocks_ += static_cast<size_t>(block != nullptr);
    return block;
}

void ProcessBlockSource::release(void *block)
{
    AV_SCOPE("ProcessBlockSource::release");
    blocks_--;
    free(block);
}

size_t ProcessBlockSource::live() const
{
    AV_SCOPE("ProcessBlockSource::live");
    return blocks_;
}

void ProcessBlockSource::require_irql_at_most(Irql, const char *)
{
    AV_SCOPE("ProcessBlockSource::require_irql_at_most");
}

}
