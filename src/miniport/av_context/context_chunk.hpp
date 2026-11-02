#ifndef AV_CONTEXT_CHUNK_HPP
#define AV_CONTEXT_CHUNK_HPP

#include <stdint.h>

namespace av {

class ContextChunk {
public:
    ContextChunk(uint32_t slot, uint32_t resource_id, uint32_t first_page, uint32_t pages, void *mapping);
    ContextChunk(uint32_t slot, uint32_t resource_id, void *locked);
    ContextChunk(const ContextChunk &) = delete;
    ContextChunk &operator=(const ContextChunk &) = delete;

    uint32_t slot() const;
    uint32_t resource_id() const;
    uint32_t first_page() const;
    uint32_t pages() const;
    void    *mapping() const;
    void    *locked() const;

    bool invariants_hold() const;

private:
    uint32_t slot_;
    uint32_t resource_id_;
    uint32_t first_page_;
    uint32_t pages_;
    void    *mapping_;
    void    *locked_;
};

}

#endif
