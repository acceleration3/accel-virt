#ifndef AV_DXBC_SIGNATURE_READER_HPP
#define AV_DXBC_SIGNATURE_READER_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class DxbcSignatureReader {
public:
    DxbcSignatureReader(const uint8_t *bytes, size_t size);

    bool        found() const;
    const char *semantic_at(uint32_t register_index, uint32_t &semantic_index) const;

private:
    uint32_t read_u32(size_t at) const;
    void     locate();

    const uint8_t *bytes_;
    size_t         size_;
    size_t         chunk_;
    size_t         chunk_size_;
    uint32_t       entry_bytes_;
};

}

#endif
