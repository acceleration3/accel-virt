#ifndef AV_D3D_COMMAND_RECORDING_HPP
#define AV_D3D_COMMAND_RECORDING_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::d3d {

class CommandRecording {
public:
    CommandRecording(BlockSource &memory, size_t first, size_t most);
    ~CommandRecording();
    CommandRecording(const CommandRecording &) = delete;
    CommandRecording &operator=(const CommandRecording &) = delete;

    [[nodiscard]] Status initialize();
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

}

#endif
