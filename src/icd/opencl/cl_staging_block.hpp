#ifndef AV_CLICD_CL_STAGING_BLOCK_HPP
#define AV_CLICD_CL_STAGING_BLOCK_HPP

#include <stdint.h>

namespace av::clicd {

class ClArenaBlocks;

class ClStagingBlock {
public:
    static constexpr uint64_t most = uint64_t{4} << 20u;
    static constexpr uint64_t least = uint64_t{64} << 10u;

    explicit ClStagingBlock(ClArenaBlocks &blocks);
    ~ClStagingBlock();
    ClStagingBlock(const ClStagingBlock &) = delete;
    ClStagingBlock &operator=(const ClStagingBlock &) = delete;

    [[nodiscard]] bool take(uint64_t wanted, uint64_t smallest);
    uint8_t           *at() const;
    uint64_t           bytes() const;

private:
    ClArenaBlocks &blocks_;
    uint8_t       *at_;
    uint64_t       offset_;
    uint64_t       bytes_;
};

}

#endif
