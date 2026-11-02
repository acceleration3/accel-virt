#ifndef AV_ICD_ADAPTER_STRING_HPP
#define AV_ICD_ADAPTER_STRING_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::icd {

class AdapterString {
public:
    static constexpr size_t bytes = 128u;

    AdapterString();

    void        take(const uint16_t *wide);
    const char *text() const;

private:
    bool append(uint32_t code, size_t &used);

    char text_[bytes];
};

}

#endif
