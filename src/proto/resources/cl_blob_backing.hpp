/* How the guest says a buffer's memory is already the host's.
 *
 * A buffer the application will map should cost no copy: the guest allocates
 * it out of the context's blob arena, which is one import the device already
 * holds the host address of, and the device makes the host's cl_mem over that
 * same memory with CL_MEM_USE_HOST_PTR. Then a map hands the application the
 * guest's view of those bytes and an unmap writes nothing - both sides are
 * looking at one allocation.
 *
 * The offset cannot travel as clCreateBuffer's host_ptr: that parameter
 * carries bytes inline into a wire arena the decode frees, which is why the
 * decoder refuses CL_MEM_USE_HOST_PTR on it outright. It travels instead as
 * the one property this wire adds to clCreateBufferWithProperties, whose list
 * already reaches the device intact. The list is the ICD's own and never the
 * application's - OpenCL 3.0 defines no buffer property, so one from the
 * application is still refused before anything is sent.
 *
 * A buffer over the application's own memory (CL_MEM_USE_HOST_PTR) is
 * another import: the pages the application lent the device, which became
 * host memory where they are (docs/host-memory.md, "Application memory").
 * A second property names that import's handle, and the offset is then into
 * it rather than into the arena.
 *
 * The offset is a number from the guest, so the device resolves it through
 * the arena's GuestRegion, or the import it names, and refuses what does not
 * lie inside it. Nothing here is trusted on either side; this only says
 * where to look. */
#ifndef AV_PROTO_CL_BLOB_BACKING_HPP
#define AV_PROTO_CL_BLOB_BACKING_HPP

#include <stdint.h>

namespace av {

struct ClBlobBackingProbe;

class ClBlobBacking {
public:
    /* The property's name. Khronos's own are small integers, and a vendor is
     * free to define others; this one is "AV" and an index, so a list that
     * reaches the wrong decoder names nothing it knows. */
    static constexpr uint64_t property = 0x4156000000000001ull;
    /* The import the offset is into; absent, it is the arena. */
    static constexpr uint64_t import_property = 0x4156000000000002ull;
    /* The most words a list this writes takes: two pairs and the zero. */
    static constexpr uint32_t list_words = 5u;

    ClBlobBacking();

    /* Holds `offset` into the arena, so a list can be written for it. */
    void set(uint64_t offset);
    /* Holds `offset` into the import the host named `memory`. */
    void set_in(uint64_t memory, uint64_t offset);
    /* Reads the backing out of a property list, leaving this holding nothing
     * when the list is null, empty or names something else. A list naming it
     * twice is not read: the device would have to choose between them. */
    bool read(const uint64_t *properties);
    /* Writes `{property, offset, 0}` into `list`, or `{property, offset,
     * import_property, memory, 0}` for an import, which must have room for
     * list_words. False unless one is held. */
    bool write(uint64_t *list) const;

    bool     held() const;
    uint64_t offset() const;
    /* The import named, 0 for the arena. */
    uint64_t memory() const;

    bool invariants_hold() const;

private:
    friend struct ClBlobBackingProbe;

    /* A property list is key/value pairs and a zero key; these two are the
     * only pairs, so six words is every list this writes or reads. */
    static constexpr uint32_t max_words = 6u;

    /* Clears everything and answers false: a list this cannot read. */
    bool refuse();

    uint64_t offset_;
    uint64_t memory_;
    bool     held_;
};

}  // namespace av

#endif  // AV_PROTO_CL_BLOB_BACKING_HPP
