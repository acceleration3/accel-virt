#ifndef AV_BLOB_TABLE_HPP
#define AV_BLOB_TABLE_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "device/memory/blob_resource.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BlobWindow;
class ControlRequest;
class Host;
struct BlobTableProbe;

class BlobTable {
public:
    explicit BlobTable(Host &host);
    ~BlobTable();
    BlobTable(const BlobTable &) = delete;
    BlobTable &operator=(const BlobTable &) = delete;

    [[nodiscard]] Status initialize();
    [[nodiscard]] Status create_guest(uint32_t id, uint64_t size, ControlRequest &request, size_t offset,
                                      uint32_t entries);
    [[nodiscard]] Status pin(uint32_t id, uint64_t minimum_size);
    void                 unpin(uint32_t id);
    [[nodiscard]] Status window(uint32_t id, BlobWindow &window);
    [[nodiscard]] Status view(uint32_t id, const uint8_t *&base);
    [[nodiscard]] Status release(uint32_t id);
    [[nodiscard]] Status retire(uint32_t id);
    void                 release_all();
    bool                 contains(uint32_t id);
    [[nodiscard]] Status size(uint32_t id, uint64_t &size);
    [[nodiscard]] Status backing(uint32_t id, BlobWindow &pages);
    uint32_t             live() const;

    bool invariants_hold() const;

private:
    struct Entry {
        BlobResource *blob_;
        bool          retiring_;
    };

    Entry *entry_of(uint32_t id);
    void   sweep_retired();

    Host                        &host_;
    void                        *lock_;
    ObjectPool<BlobResource>     blobs_;
    KeyedSlots<uint32_t, Entry>  ids_;

    friend struct BlobTableProbe;
};

}

#endif
