#ifndef AV_ICD_HELD_FLUSHER_HPP
#define AV_ICD_HELD_FLUSHER_HPP

#include <stdint.h>

namespace av::icd {

class HeldFlusher {
public:
    HeldFlusher(const HeldFlusher &) = delete;
    HeldFlusher &operator=(const HeldFlusher &) = delete;

    [[nodiscard]] virtual uint32_t flush_held() = 0;

protected:
    HeldFlusher() = default;
    ~HeldFlusher() = default;
};

}

#endif
