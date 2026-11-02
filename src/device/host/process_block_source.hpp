#ifndef AV_PROCESS_BLOCK_SOURCE_HPP
#define AV_PROCESS_BLOCK_SOURCE_HPP

#include "core/platform/block_source.hpp"

#include <stddef.h>

namespace av {

class ProcessBlockSource final : public BlockSource {
public:
    ProcessBlockSource();

    void  *allocate(size_t bytes, Pool pool) override;
    void   release(void *block) override;
    void   require_irql_at_most(Irql ceiling, const char *function) override;
    size_t live() const;

private:
    size_t blocks_;
};

}

#endif
