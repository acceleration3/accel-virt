/* The input signature of a whole DXBC or DXIL container, read.
 *
 * A pipeline's input layout arrives naming vertex shader input registers, as
 * the DDI gives it, and Direct3D 12 matches layout elements to the shader by
 * semantic. When the driver was handed the application's container itself
 * the names are the application's, in its ISG1 (or ISGN) chunk, and this
 * finds which one a register has. Everything read is bounds-checked: the
 * container came off the wire. */
#ifndef AV_DXBC_SIGNATURE_READER_HPP
#define AV_DXBC_SIGNATURE_READER_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class DxbcSignatureReader {
public:
    /* The container at `bytes`, which must outlive the reader. */
    DxbcSignatureReader(const uint8_t *bytes, size_t size);

    /* Whether `bytes` is a container with an input signature at all. */
    bool        found() const;
    /* The semantic name of the element at input register `register_index`,
     * with its semantic index; null for a register the signature has not
     * got, or a name that does not end inside the chunk. */
    const char *semantic_at(uint32_t register_index, uint32_t &semantic_index) const;

private:
    uint32_t read_u32(size_t at) const;
    void     locate();

    const uint8_t *bytes_;
    size_t         size_;
    /* The chunk's data - after its fourcc and length - and its extent. */
    size_t         chunk_;
    size_t         chunk_size_;
    uint32_t       entry_bytes_;
};

}  // namespace av

#endif  // AV_DXBC_SIGNATURE_READER_HPP
