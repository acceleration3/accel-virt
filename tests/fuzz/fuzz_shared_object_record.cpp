/* The record that carries a shared object's identity from one process's
 * allocation private data into another's open (feature 053). Both ends of that
 * journey are user mode, and what sits between them is dxgkrnl copying bytes
 * it does not read, so the miniport must survive any 40 bytes at all.
 *
 * What decodes must re-encode to exactly the bytes read, an id minted into it
 * must still decode, and nothing a record answers may be read outside it
 * (ASan). */
#include "core/support/status.hpp"
#include "proto/resources/shared_object_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::SharedObjectRecord record;
    if (record.decode(data, size) != av::Status::ok) return 0;

    std::uint8_t again[av::SharedObjectRecord::encoded_size] = {};
    if (record.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
    if (!record.kind().known()) __builtin_trap();
    if ((record.object_id() == 0u) != (record.adapter_id() == 0u)) __builtin_trap();
    if (record.size() == 0u || record.size() % av::SharedObjectRecord::page_size != 0u) __builtin_trap();

    /* Minting an id into a record a creator wrote must leave something the
     * opening process can read back: the whole point of the journey. */
    av::SharedObjectRecord minted = record;
    /* An id and an adapter travel together, so a mint sets both or neither. */
    minted.assign(record.object_id() | 1u, record.adapter_id() | 1u);
    if (!minted.assigned()) __builtin_trap();
    if (minted.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    av::SharedObjectRecord reread;
    if (reread.decode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (reread.object_id() != minted.object_id() || reread.size() != record.size()) __builtin_trap();
    if (!reread.kind().same_as(record.kind())) __builtin_trap();

    /* A record one byte short or one byte long is framing, never a record. */
    if (record.decode(data, sizeof again - 1u) == av::Status::ok) __builtin_trap();
    return 0;
}
