#ifndef AV_FAILING_BLOCKS_HPP
#define AV_FAILING_BLOCKS_HPP

#include "core/platform/block_source.hpp"

#include <cstdint>

namespace av {

class FailingBlocks final : public BlockSource {
public:
    FailingBlocks();

    void *allocate(size_t bytes, Pool pool) override;
    void  release(void *block) override;
    void  require_irql_at_most(Irql ceiling, const char *function) override;

    void          fail_from(std::uint32_t nth);
    void          heal();
    std::uint32_t requests() const;
    std::uint32_t live() const;
    std::uint32_t refused() const;

private:
    std::uint32_t fail_from_;
    std::uint32_t requests_;
    std::uint32_t live_;
    std::uint32_t refused_;
};

}

#endif
