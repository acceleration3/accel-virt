/* An adapter's name as its driver wrote it (HardwareInformation.AdapterString,
 * which Device Manager and DXGI show), turned from the UTF-16 Windows keeps it
 * in into the UTF-8 the user-mode drivers answer with: what an OpenCL device
 * and a vendor library's GPU are called, so that every API calls the one
 * device by one name. */
#ifndef AV_ICD_ADAPTER_STRING_HPP
#define AV_ICD_ADAPTER_STRING_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::icd {

class AdapterString {
public:
    /* The name in UTF-8, ended within this; a longer one is cut at a whole
     * character. */
    static constexpr size_t bytes = 128u;

    AdapterString();

    /* `wide` is UTF-16 and ended; null is no name. An unpaired surrogate
     * becomes U+FFFD. */
    void        take(const uint16_t *wide);
    const char *text() const;

private:
    /* One character's UTF-8 at the end, if it fits with the terminator. */
    bool append(uint32_t code, size_t &used);

    char text_[bytes];
};

}  // namespace av::icd

#endif  // AV_ICD_ADAPTER_STRING_HPP
