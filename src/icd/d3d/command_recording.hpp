/* A deferred context's recorded commands, in a block that grows.
 *
 * Nothing a deferred context records is sent until its command list runs, so
 * the block must hold all of it - a frame's worth of draws and the data its
 * maps and uploads carry inline. The block starts at `first` bytes and
 * doubles, up to `most`, whenever a command would not fit what is left. */
#ifndef AV_D3D_COMMAND_RECORDING_HPP
#define AV_D3D_COMMAND_RECORDING_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
}  // namespace av

namespace av::d3d {

class CommandRecording {
public:
    CommandRecording(BlockSource &memory, size_t first, size_t most);
    ~CommandRecording();
    CommandRecording(const CommandRecording &) = delete;
    CommandRecording &operator=(const CommandRecording &) = delete;

    /* The first block: no_memory when it cannot be had. */
    [[nodiscard]] Status initialize();
    /* Room for `wanted` more bytes past the `kept` already recorded, which a
     * larger block carries over. False, with the block unchanged, when even
     * the largest would not hold them or a larger one cannot be had. */
    [[nodiscard]] bool   hold(size_t kept, size_t wanted);
    uint8_t             *bytes() const;
    size_t               capacity() const;

private:
    BlockSource &memory_;
    size_t       first_;
    size_t       most_;
    uint8_t     *bytes_;
    size_t       capacity_;
};

}  // namespace av::d3d

#endif  // AV_D3D_COMMAND_RECORDING_HPP
