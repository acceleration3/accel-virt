#include "executor/d3d/dxbc_signature_reader.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr size_t   header_bytes = 32u;
constexpr uint32_t most_chunks = 64u;
constexpr uint32_t isgn_entry_bytes = 24u;
constexpr uint32_t isg1_entry_bytes = 32u;
constexpr uint32_t isgn_register_word = 4u;
constexpr uint32_t isg1_register_word = 5u;

}

DxbcSignatureReader::DxbcSignatureReader(const uint8_t *bytes, size_t size)
    : bytes_(bytes), size_(bytes == nullptr ? 0u : size), chunk_(0u), chunk_size_(0u), entry_bytes_(0u)
{
    AV_SCOPE("DxbcSignatureReader::DxbcSignatureReader");
    locate();
}

uint32_t DxbcSignatureReader::read_u32(size_t at) const
{
    AV_SCOPE("DxbcSignatureReader::read_u32");
    if (at > size_ || size_ - at < 4u) return 0u;
    return static_cast<uint32_t>(bytes_[at]) | static_cast<uint32_t>(bytes_[at + 1u]) << 8u |
           static_cast<uint32_t>(bytes_[at + 2u]) << 16u | static_cast<uint32_t>(bytes_[at + 3u]) << 24u;
}

void DxbcSignatureReader::locate()
{
    AV_SCOPE("DxbcSignatureReader::locate");
    if (size_ < header_bytes || bytes_[0] != 'D' || bytes_[1] != 'X' || bytes_[2] != 'B' || bytes_[3] != 'C') return;
    const uint32_t chunks = read_u32(28u);
    if (chunks > most_chunks || header_bytes + static_cast<size_t>(chunks) * 4u > size_) return;
    for (uint32_t chunk = 0u; chunk < chunks; chunk++) {
        const size_t at = read_u32(header_bytes + chunk * 4u);
        if (at > size_ || size_ - at < 8u) continue;
        const bool isg1 = bytes_[at] == 'I' && bytes_[at + 1u] == 'S' && bytes_[at + 2u] == 'G' && bytes_[at + 3u] == '1';
        const bool isgn = bytes_[at] == 'I' && bytes_[at + 1u] == 'S' && bytes_[at + 2u] == 'G' && bytes_[at + 3u] == 'N';
        const size_t length = read_u32(at + 4u);
        if ((!isg1 && !isgn) || length > size_ - at - 8u) continue;
        chunk_ = at + 8u;
        chunk_size_ = length;
        entry_bytes_ = isg1 ? isg1_entry_bytes : isgn_entry_bytes;
        return;
    }
}

bool DxbcSignatureReader::found() const
{
    AV_SCOPE("DxbcSignatureReader::found");
    return entry_bytes_ != 0u;
}

const char *DxbcSignatureReader::semantic_at(uint32_t register_index, uint32_t &semantic_index) const
{
    AV_SCOPE("DxbcSignatureReader::semantic_at");
    if (!found() || chunk_size_ < 8u) return nullptr;
    const uint32_t count = read_u32(chunk_);
    const size_t   table = chunk_ + read_u32(chunk_ + 4u);
    const uint32_t register_word = entry_bytes_ == isg1_entry_bytes ? isg1_register_word : isgn_register_word;
    const uint32_t name_word = entry_bytes_ == isg1_entry_bytes ? 1u : 0u;
    const size_t   end = chunk_ + chunk_size_;
    if (table > end || count > (end - table) / entry_bytes_) return nullptr;
    for (uint32_t element = 0u; element < count; element++) {
        const size_t at = table + static_cast<size_t>(element) * entry_bytes_;
        if (read_u32(at + register_word * 4u) != register_index) continue;
        const size_t name = chunk_ + read_u32(at + name_word * 4u);
        size_t       length = 0u;
        while (name + length < end && bytes_[name + length] != 0u) length++;
        if (name + length >= end) return nullptr;
        semantic_index = read_u32(at + (name_word + 1u) * 4u);
        return reinterpret_cast<const char *>(bytes_ + name);
    }
    return nullptr;
}

}
