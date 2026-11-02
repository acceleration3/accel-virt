#ifndef AV_EXPORTED_MEMORY_HPP
#define AV_EXPORTED_MEMORY_HPP

#include <stdint.h>

namespace av {

class ExportedMemory {
public:
    ExportedMemory();
    ExportedMemory(uint64_t handle, int fd, uint8_t *pointer, uint64_t size);

    bool     made() const;
    uint64_t handle() const;
    int      fd() const;
    uint8_t *pointer() const;
    uint64_t size() const;

private:
    uint64_t handle_;
    int      fd_;
    uint8_t *pointer_;
    uint64_t size_;
};

}

#endif
